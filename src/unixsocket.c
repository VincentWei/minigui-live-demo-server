/*
** unixsocket.c: Utilities for UNIX socket server.
**
** Copyright (c) 2018 FMSoft (http://www.fmsoft.cn)
** Author: Vincent Wei (https://github.com/VincentWei)
**
** The MIT License (MIT)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/time.h>

#include "log.h"
#include "wdserver.h"
#include "unixsocket.h"

/* returns fd if all OK, -1 on error */
int us_listen (const char *name)
{
    int    fd, len;
    struct sockaddr_un unix_addr;

    /* create a Unix domain stream socket */
    if ((fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
        return (-1);

    fcntl (fd, F_SETFD, FD_CLOEXEC);

    /* in case it already exists */
    unlink (name);

    /* fill in socket address structure */
    memset (&unix_addr, 0, sizeof(unix_addr));
    unix_addr.sun_family = AF_UNIX;
    strcpy (unix_addr.sun_path, name);
    len = sizeof (unix_addr.sun_family) + strlen (unix_addr.sun_path);

    /* bind the name to the descriptor */
    if (bind (fd, (struct sockaddr *) &unix_addr, len) < 0)
        goto error;
    if (chmod (name, 0666) < 0)
        goto error;

    /* tell kernel we're a server */
    if (listen (fd, 5) < 0)
        goto error;

    return (fd);

error:
    close (fd);
    return (-1);
}

#define    STALE    30    /* client's name can't be older than this (sec) */

/* Wait for a client connection to arrive, and accept it.
 * We also obtain the client's pid from the pathname
 * that it must bind before calling us.
 */
/* returns new fd if all OK, < 0 on error */
int us_accept (int listenfd, pid_t *pidptr, uid_t *uidptr)
{
    int                clifd;
    socklen_t          len;
    time_t             staletime;
    struct sockaddr_un unix_addr;
    struct stat        statbuf;
    const char*        pid_str;

    len = sizeof (unix_addr);
    if ( (clifd = accept (listenfd, (struct sockaddr *) &unix_addr, &len)) < 0)
        return (-1);        /* often errno=EINTR, if signal caught */

    /* obtain the client's uid from its calling address */
    len -= /* th sizeof(unix_addr.sun_len) - */ sizeof(unix_addr.sun_family);
                    /* len of pathname */
    unix_addr.sun_path[len] = 0;            /* null terminate */
    if (stat(unix_addr.sun_path, &statbuf) < 0)
        return(-2);
#ifdef S_ISSOCK    /* not defined for SVR4 */
    if (S_ISSOCK(statbuf.st_mode) == 0)
        return(-3);        /* not a socket */
#endif
    if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
        (statbuf.st_mode & S_IRWXU) != S_IRWXU)
          return(-4);    /* is not rwx------ */

    staletime = time(NULL) - STALE;
    if (statbuf.st_atime < staletime ||
        statbuf.st_ctime < staletime ||
        statbuf.st_mtime < staletime)
          return(-5);    /* i-node is too old */

    if (uidptr != NULL)
        *uidptr = statbuf.st_uid;    /* return uid of caller */

    /* get pid of client from sun_path */
    pid_str = strrchr (unix_addr.sun_path, 'P');
    pid_str++;

    *pidptr = atoi (pid_str);
    
    unlink (unix_addr.sun_path);        /* we're done with pathname now */
    return (clifd);
}

static struct _demo_info {
    char* const demo_name;
    char* const working_dir;
    char* const exe_file;
    char* const def_mode;
} _demo_list [] = {
    {"mguxdemo", "/usr/local/bin/", "/usr/local/bin/mguxdemo", "480x640-16bpp"},
    {"cbplusui", "/usr/local/bin/", "/usr/local/bin/cbplusui", "240x240-16bpp"},
};

pid_t us_launch_client (const char* demo_name)
{
    int i, found = -1;
    pid_t pid = 0;

    for (i = 0; i < TABLESIZE (_demo_list); i++) {
        if (strcmp (_demo_list[i].demo_name, demo_name) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        return 0;
    }

    if ((pid = vfork ()) > 0) {
        ACCESS_LOG (("fork child for %s\n", demo_name));
    }
    else if (pid == 0) {
        int retval;
        char env_mode [32];

        retval = chdir (_demo_list[found].working_dir);
        if (retval)
            perror ("chdir");

        strcpy (env_mode, "MG_DEFAULTMODE=");
        strcat (env_mode, _demo_list[found].def_mode);
        char *const argv[] = {_demo_list[found].demo_name, NULL};
        char *const envp[] = {"MG_GAL_ENGINE=usvfb", "MG_IAL_ENGINE=usvfb", env_mode, NULL};
        if (execve (_demo_list[found].exe_file, argv, envp) < 0)
			fprintf (stderr, "execve error\n");

        perror ("execl");
        _exit (1);
    }
    else {
        perror ("vfork");
        return -1;
    }

    return pid;
}

int us_on_connected (USClient* us_client)
{
    ssize_t n = 0;
    int retval;
    struct _frame_header header;

    us_client->shadow_fb = NULL;

    /* read info of virtual frame buffer */
    n = read (us_client->fd, &header, sizeof (struct _frame_header));
    if (n < sizeof (struct _frame_header) || header.type != FT_VFBINFO) {
        printf ("us_on_connected: frame header info: %ld, %d.\n", n, header.type);
        retval = 1;
        goto error;
    }

    n = read (us_client->fd, &us_client->vfb_info, sizeof (struct _vfb_info));
    if (n < header.payload_len) {
        retval = 2;
        goto error;
    }

    if (us_client->vfb_info.type == USVFB_TRUE_RGB565) {
        us_client->bytes_per_pixel = 3;
        us_client->row_pitch = us_client->vfb_info.width * 3;
    }
    else if (us_client->vfb_info.type == USVFB_TRUE_RGB0888) {
        us_client->bytes_per_pixel = 3;
        us_client->row_pitch = us_client->vfb_info.width * 3;
    }
    else {
        /* not support pixel type */
        retval = 3;
        goto error;
    }

    /* create shadow frame buffer */
    us_client->shadow_fb = malloc (us_client->row_pitch * us_client->vfb_info.height);
    if (us_client->shadow_fb == NULL) {
        retval = 4;
        goto error;
    }

    gettimeofday (&us_client->last_flush_time, NULL);
    return 0;

error:
    LOG (("us_on_connected: failed (%d)\n", retval));

    if (us_client->shadow_fb) {
        free (us_client->shadow_fb);
    }

    return retval;
}

/* return zero on success; none-zero on error */
int us_ping_client (const USClient* us_client)
{
    ssize_t n = 0;
    struct _frame_header header;

    header.type = FT_PING;
    header.payload_len = 0;
    n = write (us_client->fd, &header, sizeof (struct _frame_header));
    if (n != sizeof (struct _frame_header)) {
        return 1;
    }

    return 0;
}

/* return zero on success; none-zero on error */
int us_on_client_data (USClient* us_client)
{
    ssize_t n = 0;
    struct _frame_header header;

    n = read (us_client->fd, &header, sizeof (struct _frame_header));
    if (n == 0) {
        return 0;
    }

    if (n < sizeof (struct _frame_header)) {
        printf ("us_on_client_data: read bytes %ld\n", n);
        return 1;
    }

    if (header.type == FT_DIRTYPIXELS) {
        int y, Bpp_vfb;
        RECT rc_dirty;

        n = read (us_client->fd, &rc_dirty, sizeof (RECT));
        if (n < sizeof (RECT)) {
            return 2;
        }

        /* copy pixel data to shadow frame buffer here */
        void* buff = malloc (us_client->vfb_info.rlen);
        if (buff == NULL) {
            return 3;
        }

        if (us_client->vfb_info.type == USVFB_TRUE_RGB565) {
            Bpp_vfb = 2;
        }
        else {
            Bpp_vfb = 4;
        }

        uint8_t* dst_pixel = us_client->shadow_fb + us_client->row_pitch * rc_dirty.top +  rc_dirty.left * us_client->bytes_per_pixel;
        int dirty_pixels = rc_dirty.right - rc_dirty.left;
        for (y = rc_dirty.top; y < rc_dirty.bottom; y++) {
            n = read (us_client->fd, buff, (rc_dirty.right - rc_dirty.left) * Bpp_vfb);

            uint8_t* src_pixel = (uint8_t*)buff;
            if (Bpp_vfb == 4) {
                for (int x = 0; x < dirty_pixels; x++) {
                    uint32_t pixel = *((uint32_t*)src_pixel);
                    dst_pixel [x*3 + 0] = (uint8_t)((pixel&0xFF0000)>>16);
                    dst_pixel [x*3 + 1] = (uint8_t)((pixel&0xFF00)>>8);
                    dst_pixel [x*3 + 2] = (uint8_t)((pixel&0xFF));
                    src_pixel += 4;
                }
            }
            else {
                for (int x = 0; x < dirty_pixels; x++) {
                    uint16_t pixel = *((uint16_t*)src_pixel);
                    dst_pixel [x*3 + 0] = (((pixel&0xF800)>>11)<<3);
                    dst_pixel [x*3 + 1] = (((pixel&0x07E0)>>5)<<2);
                    dst_pixel [x*3 + 2] = ((pixel&0x001F)<<3);
                    src_pixel += 2;
                }
            }

            
            dst_pixel += us_client->row_pitch;
        }
        free (buff);

        /* merge the dirty rect to whole dirty rect */
        if ((us_client->rc_dirty.right - us_client->rc_dirty.left) <= 0
                && (us_client->rc_dirty.bottom - us_client->rc_dirty.top) <= 0) {
            us_client->rc_dirty = rc_dirty;
        }
        else {
            us_client->rc_dirty.left = (us_client->rc_dirty.left < rc_dirty.left) ? us_client->rc_dirty.left : rc_dirty.left;
            us_client->rc_dirty.top  = (us_client->rc_dirty.top < rc_dirty.top) ? us_client->rc_dirty.top : rc_dirty.top;
            us_client->rc_dirty.right = (us_client->rc_dirty.right > rc_dirty.right) ? us_client->rc_dirty.right : rc_dirty.right;
            us_client->rc_dirty.bottom = (us_client->rc_dirty.bottom > rc_dirty.bottom) ? us_client->rc_dirty.bottom : rc_dirty.bottom;
        }
    }
    else if (header.type == FT_PONG) {
        LOG (("us_on_client_data: got FT_PONG from client: %d\n", us_client->fd));
    }
    else {
        LOG (("us_on_client_data: unknown data type: %d\n", header.type));
        return 3;
    }

    return 0;
}

int us_check_dirty_pixels (const USClient* us_client)
{
    struct timeval now;

    if ((us_client->rc_dirty.right - us_client->rc_dirty.left) <= 0
            && (us_client->rc_dirty.bottom - us_client->rc_dirty.top) <= 0)
        return 0;

    gettimeofday (&now, NULL);
    if (us_client->last_flush_time.tv_sec != now.tv_sec) {
        return 1;
    }

    if ((now.tv_usec - us_client->last_flush_time.tv_usec) >= MAX_FLUSH_PIXELS_TIME) {
        return 1;
    }

    return 0;
}

void us_reset_dirty_pixels (USClient* us_client)
{
    us_client->rc_dirty.top = 0;
    us_client->rc_dirty.left = 0;
    us_client->rc_dirty.right = 0;
    us_client->rc_dirty.bottom = 0;
    gettimeofday (&us_client->last_flush_time, NULL);
}

int us_client_cleanup (USClient* us_client)
{
    if (us_client->shadow_fb) {
        free (us_client->shadow_fb);
        us_client->shadow_fb = NULL;
    }

    close (us_client->fd);
    us_client->fd = -1;

    return 0;
}

