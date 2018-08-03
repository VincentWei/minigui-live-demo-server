/**
 * wdserver -- An rfc6455-complaint Web Display Server 
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 FMSoft <http://www.minigui.com>
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef WD_SERVER_H_INCLUDED
#define WD_SERVER_H_INCLUDED

#define WD_VERSION "0.9"

/* The pixel format */
#define USVFB_PSEUDO_RGB332    1
#define USVFB_TRUE_RGB555      2
#define USVFB_TRUE_RGB565      3
#define USVFB_TRUE_RGB888      4
#define USVFB_TRUE_RGB0888     5
#define USVFB_TRUE_ARGB1555    6
#define USVFB_TRUE_ARGB8888    7

struct _vfb_info {
    short height;
    short width;        // Size of the screen
    short bpp;          // Depth (bits-per-pixel)
    short type;         // Pixel type
    short rlen;         // Length of one scan line in bytes
    void  *fb;          // Frame buffer
    short async_update;
};

#define USS_PATH    "/var/tmp/web-display-server"
#define USC_PATH    "/var/tmp/web-display-client-P%d"

#define USC_PERM    S_IRWXU            /* rwx for user only */

#define FT_VFBINFO      10
#define FT_PING         11
#define FT_PONG         12
#define FT_EVENT        13
#define FT_DIRTYPIXELS  14

struct _frame_header {
    int type;
    size_t payload_len;
    unsigned char payload[0];
};

#define EVENT_NULL          0
#define EVENT_MOUSEMOVE     1
#define EVENT_LBUTTONDOWN   2
#define EVENT_LBUTTONUP     3

#define EVENT_KEYDOWN       11
#define EVENT_KEYUP         12

struct _remote_event {
    int type;
    int value1;
    int value2;
};

int wd_set_null_stdio (void);
int wd_daemon (void);

#endif // for #ifndef WD_SERVER_H
