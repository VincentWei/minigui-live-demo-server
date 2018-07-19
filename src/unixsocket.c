/*
** unixsocket.c: Utilities for UNIX socket server.
**
** Wei Yongming.
**
** NOTE: The idea comes from sample code in APUE.
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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "log.h"
#include "unixsocket.h"

/* returns fd if all OK, -1 on error */
int us_listen (const char *name)
{
    int    fd, len;
    struct sockaddr_un unix_addr;

    /* create a Unix domain stream socket */
    if ( (fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
        return (-1);

    fcntl( fd, F_SETFD, FD_CLOEXEC );

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
    pid_str = strrchr (unix_addr.sun_path, '/');
    pid_str++;

    *pidptr = atoi (pid_str);
    
    unlink (unix_addr.sun_path);        /* we're done with pathname now */
    return (clifd);
}

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

static struct _demo_info {
    char* const demo_name;
    char* const exe_file;
    char* const def_mode;
} _demo_list [] = {
    {"mguxdemo", "/usr/local/bin/mguxdemo", "480x640-16bpp"},
    {"cbplusui", "/usr/local/bin/cbplusui", "240x240-16bpp"},
};

pid_t us_start_client (const char* demo_name, const char* video_mode)
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
        ACCESS_LOG (("fork child for %s of %s\n", demo_name, video_mode));
    }
    else if (pid == 0) {
        char env_mode [32];
        if (video_mode == NULL)
            video_mode = _demo_list[found].def_mode;
        strcpy (env_mode, "MG_DEFAULTMODE=");
        strcat (env_mode, video_mode);

        char *const argv[] = {_demo_list[found].demo_name, NULL};
        char *const envp[] = {"MG_GAL_ENGINE=commlcd", "MG_IAL_ENGINE=common", env_mode, NULL};
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

/* The pixel format */
#define COMMLCD_TRUE_RGB565      3
#define COMMLCD_TRUE_RGB8888     4

struct _commlcd_info {
    short height, width;  // Size of the screen
    short bpp;            // Depth (bits-per-pixel)
    short type;           // Pixel type
    short rlen;           // Length of one scan line in bytes
    void  *fb;            // Frame buffer
};

#define FT_MODE         1
    #define MAX_MODE        15
#define FT_VFBINFO      2
#define FT_SHMID        3
#define FT_SEMID        4

#define FT_PING         11
#define FT_PONG         12

#define FT_EVENT        13

#define FT_DIRTY        14
#define FT_ACK          15

struct _frame_header {
    int type;
    size_t payload_len;
    unsigned char payload[0];
};

int us_on_connected (int fd, const char* video_mode)
{
    ssize_t n = 0;
    int retval;
    struct _frame_header header;
    struct _commlcd_info lcd_info;
    uint8_t* shadow_fb = NULL;
    uint8_t* virtual_fb = (uint8_t*) -1;
    int shm_id, sem_id;

    header.type = FT_MODE;
    header.payload_len = strlen (video_mode);
    n += write (fd, &header, sizeof (struct _frame_header));
    n += write (fd, video_mode, header.payload_len);

    if (n != sizeof (struct _frame_header) + header.payload_len) {
        retval = 1;
        goto error;
    }

    /* read info of virtual fram buffer */
    n = read (fd, &header, sizeof (struct _frame_header));
    if (n < sizeof (struct _frame_header) || header.type != FT_VFBINFO) {
        retval = 2;
        goto error;
    }
    n = read (fd, &lcd_info, sizeof (struct _commlcd_info));
    if (n < header.payload_len) {
        retval = 3;
        goto error;
    }

    /* create shadow frame buffer */
    shadow_fb = malloc (lcd_info.rlen * lcd_info.height);
    if (shadow_fb == NULL) {
        retval = 4;
        goto error;
    }

    /* read id of shared virtual fram buffer */
    n = read (fd, &header, sizeof (struct _frame_header));
    if (n < sizeof (struct _frame_header) || header.type != FT_SHMID) {
        retval = 5;
        goto error;
    }
    n = read (fd, &shm_id, sizeof (int));
    if (n < header.payload_len) {
        retval = 6;
        goto error;
    }

    /* attach to the shared virtual fram buffer */
    virtual_fb = (uint8_t*)shmat (shm_id, 0, 0); 
    if (virtual_fb == (uint8_t*)-1) {
        retval = 7;
        goto error;
    }

    /* read id of the semphamore */
    n = read (fd, &header, sizeof (struct _frame_header));
    if (n < sizeof (struct _frame_header) || header.type != FT_SEMID) {
        retval = 8;
        goto error;
    }
    n = read (fd, &sem_id, sizeof (int));
    if (n < header.payload_len) {
        retval = 9;
        goto error;
    }

    return 0;

error:
    LOG (("us_on_connected: failed (%d)\n", retval));

    if (shadow_fb) {
        free (shadow_fb);
    }

    if (virtual_fb != (uint8_t*)-1) {
        shmdt (virtual_fb);
    }

    close (fd);

    return retval;
}

