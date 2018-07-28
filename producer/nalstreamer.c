#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>

static size_t pack_uint32 (void* buf, uint32_t val)
{
    uint32_t v32 = htonl (val);
    memcpy(buf, &v32, sizeof(uint32_t));
    return sizeof(uint32_t);
}

static size_t unpack_uint32 (const void *b, uint32_t * val)
{
    uint32_t v32 = 0;
    memcpy (&v32, b, sizeof (uint32_t));
    *val = ntohl (v32);
    return sizeof (uint32_t);
}

static int read_nal (int fd_h264, unsigned char* buff, int* buff_size)
{
    int nal_len = 0;
    int end_count = 0;
    int extract = 0;

    // Extract bytes and put into buff for sending
    while (1) {
        unsigned char byte_read;
        if (read (fd_h264, &byte_read, 1) < 1)
            break;

        if (extract) {
            buff [nal_len] = byte_read;
            nal_len ++;

            if (nal_len >= *buff_size) {
                *buff_size = (*buff_size) << 1;
                buff = realloc (buff, *buff_size);
                if (buff == NULL)
                    return -1;
            }
        }

        // Check for NAL header 0 0 0 1
        if ((byte_read == 0 && end_count < 3) || (byte_read == 1 && end_count == 3)) {
            end_count++;
        }
        else {
            end_count = 0;
        }

        if (end_count == 4) {
            // Reset NAL header count
            end_count = 0;
            if (extract) {
                // Delete beginning of next NAL from current NAL array and decrement read pointer so that NAL is available for next read
                lseek (fd_h264, -4, SEEK_CUR);
                nal_len -= 4;
                break;
            }
            else {
                if ((nal_len + 4) >= *buff_size) {
                    *buff_size = (*buff_size) << 1;
                    buff = realloc (buff, *buff_size);
                    if (buff == NULL)
                        return -1;
                }

                // Insert NAL header that's been detected into NAL array
                for (int i = 0; i < 3; i++) {
                    buff [nal_len] = 0;
                    nal_len ++;
                }
                buff [nal_len] = 1;
                nal_len ++;
                extract = 1;
            }
        }
    }

	return nal_len;
}

#define REQUEST_NONE    0
#define REQUEST_START   1
#define REQUEST_STOP    2

#define SELECT_TIMEOUT  35000

static int send_nal_frame (int fd_send, int listener, unsigned char* buff, int nal_size)
{
    char p[PIPE_BUF];
    char* ptr;

    ptr = p;
    ptr += pack_uint32 (ptr, listener);
    ptr += pack_uint32 (ptr, 0x02);  /* binary data */
    ptr += pack_uint32 (ptr, nal_size);

    if (write (fd_send, p, sizeof(uint32_t) * 3) < (ssize_t) (sizeof(uint32_t) * 3)) {
        perror ("failed to write frame header: ");
        return -1;
    }

    if (write (fd_send, buff, nal_size) < nal_size) {
        perror ("failed to write frame payload: ");
        return -1;
    }

    return 0;
}

/*
 * 0: no data, timeout
 * 1: start stream
 * 2: stop stream
 * <0: error
 */
static int wait_request (int fd_read, int* listener)
{
    fd_set set;
    struct timeval timeout = {0, SELECT_TIMEOUT};
    int retval;
    uint32_t size = 0, type = 0;
    char hdr[PIPE_BUF] = { 0 }, buf[PIPE_BUF] = {0};
    char *ptr = NULL;

    FD_ZERO (&set);
    FD_SET (fd_read, &set);

    retval = select (fd_read + 1, &set, NULL, NULL, &timeout);
    if (retval == 0) {
        return REQUEST_NONE;
    }
    else if (retval < 0) {
        return -1;
    }

    if (!FD_ISSET (fd_read, &set))
        return REQUEST_NONE;

    if (hdr[0] == '\0') {
        if (read (fd_read, hdr, sizeof (uint32_t) * 3) < 1)
            return -1;
    }

    ptr = hdr;
    ptr += unpack_uint32 (ptr, (uint32_t*)listener);
    ptr += unpack_uint32 (ptr, &type);
    ptr += unpack_uint32 (ptr, &size);

    if (read (fd_read, buf, size) < 1)
        return -1;

    printf ("client: %d, msg: %s\n", *listener, buf);
    if (strncmp (buf, "REQUESTSTREAM", 13) == 0) {
        return REQUEST_START;
    }
    else if (strncmp (buf, "STOPSTREAM", 10) == 0) {
        return REQUEST_STOP;
    }

    return -1;
}

int main (int argc, char* argv[]) {
    const char *fifo_read = "/tmp/wspipeout.fifo";
    const char *fifo_send = "/tmp/wspipein.fifo";
    int fd_send, fd_read, fd_h264;

    int started = 0;
    int buff_size = 1024*64;
    unsigned char* buff = calloc (sizeof (unsigned char), buff_size);
    if (buff == NULL) {
        perror ("failed to allocate buffer: ");
        exit (1);
    }

    if (argc > 1) {
        fd_h264 = open (argv[1], O_RDONLY);
        if (fd_h264 < 0) {
            perror ("failed to open H264 file: ");
            exit (2);
        }
    }
    else {
        printf ("No H264 file given\n");
        exit (0);
    }

    fd_send = open (fifo_send, O_WRONLY);
    if (fd_send < 0) {
        perror ("failed to open FIFO for sending data: ");
        exit (3);
    }

    fd_read = open (fifo_read, O_RDWR | O_NONBLOCK);
    if (fd_read < 0) {
        perror ("failed to open FIFO for reading data: ");
        exit (4);
    }

    while (1) {
        int listener = 0;
        int retval = wait_request (fd_read, &listener);

        if (retval == REQUEST_START) {
            int nal_size = read_nal (fd_h264, buff, &buff_size);
            if (nal_size > 0) {
                send_nal_frame (fd_send, listener, buff, nal_size);
                started = 1;
            }
        }
        else if (retval == REQUEST_STOP) {
            printf ("Stopped\n");
            lseek (fd_h264, 0, SEEK_SET);
            started = 0;
        }
        else if (retval == REQUEST_NONE) {
            if (started) {
                int nal_size = read_nal (fd_h264, buff, &buff_size);
                if (nal_size > 0) {
                    send_nal_frame (fd_send, listener, buff, nal_size);
                }
            }
        }
    }

    free (buff);
    close (fd_read);
    close (fd_send);
    close (fd_h264);

    return 0;
}

