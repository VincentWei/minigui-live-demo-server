/**
 * unixsocket.h: Utilities for UnixSocket server.
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

#define TABLESIZE(table)    (sizeof(table)/sizeof(table[0]))

typedef struct _RECT
{
    int left;
    int top;
    int right;
    int bottom;
} RECT;

/* A UnixSocket Client */
typedef struct USClient_
{
    int fd;                         /* UNIX socket FD */
    pid_t pid;                      /* client PID */
    struct _vfb_info vfb_info;      /* the virtual frame buffer info of the local display client */
    int row_pitch;                  /* the row pitch of the shadow FB */
    int bytes_per_pixel;            /* the bytes_per_pixel of the shadow FB */
    uint8_t* shadow_fb;             /* the shadow frame buffer */
    RECT rc_dirty;                  /* the dirty rectangle which is not sent to WSClient */
    struct timeval last_flush_time; /* the last time flushing the dirty pixels to WebSocket client */
} USClient;

int us_listen (const char* name);
int us_accept (int listenfd, pid_t *pidptr, uid_t *uidptr);

int us_on_connected (USClient* us_client);
int us_ping_client (const USClient* us_client);
int us_on_client_data (USClient* us_client);

/* microsecond */
#define MAX_FLUSH_PIXELS_TIME       50000

int us_check_dirty_pixels (const USClient* us_client);
void us_reset_dirty_pixels (USClient* us_client);

int us_client_cleanup (USClient* us_client);

#endif // for #ifndef UNIXSOCKET_H
