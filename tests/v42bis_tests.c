/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: v42bis_tests.c,v 1.5 2005/09/01 17:06:46 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <tgmath.h>
#include <math.h>
#include <assert.h>
#include <tiffio.h>

#include "spandsp.h"

int in_fd;
int v42bis_fd;
int out_fd;

void frame_handler(void *user_data, const uint8_t *buf, int len)
{
    int ret;
    
    if ((ret = write(v42bis_fd, buf, len)) != len)
        fprintf(stderr, "Write error %d/%d\n", ret, errno);
}

void data_handler(void *user_data, const uint8_t *buf, int len)
{
    int ret;
    
    if ((ret = write(out_fd, buf, len)) != len)
        fprintf(stderr, "Write error %d/%d\n", ret, errno);
}

int main(int argc, char *argv[])
{
    int len;
    v42bis_state_t state;
    int i;
    int octet;
    uint8_t buf[1024];

    v42bis_init(&state, 3, 512, 6, frame_handler, NULL, data_handler, NULL);

    /* Get the file name, open it up, and open up the lzw output file. */
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(2);
    }
    if ((in_fd = open(argv[1], O_RDONLY)) < 0)
    {
        fprintf(stderr, "Error opening files.\n");
        exit(2);
    };
    if ((v42bis_fd = open("v42bis_tests.v42bis", O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
    {
        fprintf(stderr, "Error opening files.\n");
        exit(2);
    };

    while ((len = read(in_fd, buf, 1024)) > 0)
        v42bis_compress(&state, buf, len);

    close(in_fd);
    close(v42bis_fd);

    /* Now open the files for the decompression. */
    if ((v42bis_fd = open("v42bis_tests.v42bis", O_RDONLY)) < 0)
    {
        fprintf(stderr, "Error opening files.\n");
        exit(2);
    };
    if ((out_fd = open("v42bis_tests.out", O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
    {
        fprintf(stderr, "Error opening files.\n");
        exit(2);
    };

    while ((len = read(v42bis_fd, buf, 1024)) > 0)
        v42bis_decompress(&state, buf, len);

    close(v42bis_fd);
    close(out_fd);

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
