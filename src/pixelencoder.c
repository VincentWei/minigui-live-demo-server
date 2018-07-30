/*
** pixelencodert.c: PNG encoder for pixels.
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

#include <png.h>

#include "log.h"
#include "wdserver.h"
#include "unixsocket.h"
#include "pixelencoder.h"

int save_dirty_pixels_to_png (const char* file_name, const USClient* us_client)
{
    int retval = 0;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    FILE *png_file = NULL;
    png_bytepp pixel_rows = NULL;
    int height, width;

    if (us_client->rc_dirty.left < 0
            || us_client->rc_dirty.top < 0
            || us_client->rc_dirty.right > us_client->vfb_info.width
            || us_client->rc_dirty.bottom > us_client->vfb_info.height) {
        LOG (("save_dirty_pixels_to_png: invalid dirty rect.\n"));
        return -1;
    }

    width = us_client->rc_dirty.right - us_client->rc_dirty.left;
    height = us_client->rc_dirty.bottom - us_client->rc_dirty.top;
    if (width <= 0 || height <= 0) {
        LOG (("save_dirty_pixels_to_png: bad or empty dirty rect.\n"));
        return -2;
    }

    png_file = fopen (file_name, "wb");
    if (!png_file) {
        LOG (("save_dirty_pixels_to_png: failed to create file: %s\n", file_name));
        return -3;
    }

    pixel_rows = (png_bytepp)png_malloc (png_ptr, height * sizeof (png_bytep));
    if (!pixel_rows) {
        LOG (("save_dirty_pixels_to_png: failed to allocate memory for pixel_rows: %d\n", height));
        retval = 1;
        goto error;
    }

    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        LOG (("save_dirty_pixels_to_png: failed to call png_create_write_struct\n"));
        retval = 2;
        goto error;
    }

    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
        LOG (("save_dirty_pixels_to_png: failed to call png_create_info_struct\n"));
        retval = 3;
        goto error;
    }

    if (setjmp (png_jmpbuf (png_ptr))) {
        retval = 4;
        goto error;
    }

    png_init_io (png_ptr, png_file);
    png_set_IHDR (png_ptr, info_ptr,
            us_client->rc_dirty.right - us_client->rc_dirty.left,
            us_client->rc_dirty.bottom - us_client->rc_dirty.top, 
            8,  /* bit_depth */
            (us_client->vfb_info.type == COMMLCD_TRUE_RGB565)?PNG_COLOR_TYPE_RGB:PNG_COLOR_TYPE_RGB_ALPHA,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    {
        int bytes_per_pixel;
        png_color_8 sig_bit;

        if (us_client->vfb_info.type == COMMLCD_TRUE_RGB565) {
            bytes_per_pixel = 3;
            sig_bit.red = 5;
            sig_bit.green = 6;
            sig_bit.blue = 5;
            sig_bit.alpha = 0;
        }
        else if (us_client->vfb_info.type == COMMLCD_TRUE_RGB8888) {
            bytes_per_pixel = 4;
            sig_bit.red = 8;
            sig_bit.green = 8;
            sig_bit.blue = 8;
            sig_bit.alpha = 8;
        }

        png_set_sBIT (png_ptr, info_ptr, &sig_bit);
        for (int i = 0; i < height; i++) {
            pixel_rows[i] = (png_bytep)(us_client->shadow_fb
                    + us_client->vfb_info.rlen * (us_client->rc_dirty.top + i) + us_client->rc_dirty.left * bytes_per_pixel);
        }
    }

    png_write_info (png_ptr, info_ptr);
    png_set_packing (png_ptr);
    png_write_image (png_ptr, pixel_rows);
    png_write_end (png_ptr, info_ptr);

error:
    if (pixel_rows)
        png_free (png_ptr, pixel_rows);
    if (png_ptr)
        png_destroy_write_struct (&png_ptr, &info_ptr);
    if (png_file)
        fclose (png_file);

    return retval;
}

