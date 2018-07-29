/**
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 *
 * Copyright (c) 2018 FMSoft
 * Author: Vincent Wei (https://github.com/VincentWei)
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef UNIXSOCKET_H_INCLUDED
#define UNIXSOCKET_H_INCLUDED

#define USS_PATH            "/var/tmp/web-display-server"

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

typedef struct _RECT
{
    int left;
    int top;
    int right;
    int bottom;
} RECT;

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

/* The pixel format */
#define COMMLCD_TRUE_RGB565      3
#define COMMLCD_TRUE_RGB8888     4

struct _vfb_info {
    short height, width;  // Size of the screen
    short bpp;            // Depth (bits-per-pixel)
    short type;           // Pixel type
    short rlen;           // Length of one scan line in bytes
    void  *fb;            // Frame buffer
};

/* A UnixSocket Client */
typedef struct USClient_
{
    int fd;                         /* UNIX socket FD */
    pid_t pid;                      /* client PID */

    int shm_id, sem_id;             /* identifies for SysV IPC objects */

    struct _vfb_info vfb_info;      /* info or virtual frame buffer */
    uint8_t* virtual_fb;            /* the virtual frame buffer */

    int bytes_per_pixel;            /* the bytes_per_pixel of the shadow FB */
    int row_pitch;                  /* the row pitch of the shadow FB */
    uint8_t* shadow_fb;             /* the shadow frame buffer */

    RECT rc_dirty;                  /* the dirty rectangle which is not sent to WSClient */
} USClient;

int us_listen (const char* name);
int us_accept (int listenfd, pid_t *pidptr, uid_t *uidptr);

pid_t us_start_client (const char* demo_name, const char* video_mode);
int us_on_connected (USClient* us_client, const char* video_mode);
int us_ping_client (const USClient* us_client);
int us_on_client_data (USClient* us_client);
int us_client_cleanup (USClient* us_client);

#endif // for #ifndef UNIXSOCKET_H
