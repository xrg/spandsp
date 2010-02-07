/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oss.c - OSS interface routines for testing stuff
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: oss.c,v 1.6 2005/09/28 17:11:49 steveu Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(HAVE_SYS_SOUNDCARD_H)

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include "spandsp/telephony.h"
#include "spandsp/oss.h"

static int audio_fd = -1;

int oss_init (int mode)
{
    int p;
    int block_size;
    audio_buf_info info;

    audio_fd = open("/dev/dsp", O_RDWR);
    //audio_fd = open("/dev/dsp", O_RDWR | O_NONBLOCK);
    if (audio_fd == -1)
    	return  audio_fd;
    
    if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) < 0)
    {
        oss_release();
        return  audio_fd;
    }
    p = 16;
    if (ioctl(audio_fd, SNDCTL_DSP_SAMPLESIZE, &p) < 0)
    {
        oss_release();
        return  audio_fd;
    }
    p = 1;
    if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &p) < 0)
    {
        oss_release();
        return  audio_fd;
    }
    p = SAMPLE_RATE;
    if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &p) < 0)
    {
        oss_release();
        return  audio_fd;
    }

    if (ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &block_size) < 0)
    {
        oss_release();
        return  audio_fd;
    }
    if ((mode & 1))
    {
        p = 0x00100004;
        if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &p) < 0)
        {
            oss_release();
            return  audio_fd;
        }
        if (ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info) < 0)
        {
            oss_release();
            return  audio_fd;
        }
        printf ("Result %d %d %d %d\n", info.fragments, info.fragstotal, info.fragsize, info.bytes);
#if defined(XYZZY)
        p = 4;
        if (ioctl(audio_fd, SNDCTL_DSP_SUBDIVIDE, &p) < 0)
        {
            oss_release();
            return  audio_fd;
        }
#endif
        if (ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &block_size) < 0)
        {
            oss_release();
            return  audio_fd;
        }
        printf ("Result %d\n", block_size);
    }

    return  audio_fd;
}
/*- End of function --------------------------------------------------------*/

int oss_get (int16_t amp[], int samples)
{
    int len;
    
    len = read (audio_fd, amp, samples*sizeof(int16_t));
    if (len < 0)
    	return  len;
    return  len/sizeof(int16_t);
}
/*- End of function --------------------------------------------------------*/

int oss_put (int16_t amp[], int samples)
{
    int len;
    
    len = write (audio_fd, amp, samples*sizeof(int16_t));
    if (len < 0)
    	return  len;
    return  len/sizeof(int16_t);
}
/*- End of function --------------------------------------------------------*/

int oss_release (void)
{
    if (audio_fd >= 0)
        close (audio_fd);
    audio_fd = -1;
    return  0;
}
/*- End of function --------------------------------------------------------*/

#if defined(XYZZY)
int oss_set_source(char source)
{
    int i;
    int p;
    int mix_fd;
  
    if (source == 'c')
    	p = 1 << SOUND_MIXER_CD;
    if (source == 'l')
    	p = 1 << SOUND_MIXER_LINE;
    if (source == 'm')
    	p = 1 << SOUND_MIXER_MIC;
  
    mix_fd = open("/dev/mixer", O_WRONLY, 0);
    i = ioctl(mix_fd, SOUND_MIXER_WRITE_RECSRC, &p);
    close(mix_fd);
    return(i);
}
/*- End of function --------------------------------------------------------*/

int oss_set_in_level(int a)
{
    int p;
    int mix_fd;
    
    p = (((int) a) << 8 | (int) a);
    mix_fd = open("/dev/mixer", O_RDWR, 0);
    ioctl(mix_fd, MIXER_WRITE(SOUND_MIXER_RECLEV), &p);
    close(mix_fd);
    return(0);
}
/*- End of function --------------------------------------------------------*/

int oss_set_out_level(int a)
{
    int p;
    int mix_fd;

    p = (((int) a) << 8 | (int) a);
    mix_fd = open("/dev/mixer", O_WRONLY, 0);
    ioctl(mix_fd, MIXER_WRITE(SOUND_MIXER_PCM), &p);
    close(mix_fd);
    return(0); 
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(TESTBED)
int main(int argc, char *argv[])
{
    int i, p;
    short int buf[100000];
    int len;

    if (oss_init(0) < 0)
    {
        printf("Cannot open audio device - is it busy?\n");
        return(-1);
    }

    printf ("Go\n");
    for (i = 0;  i < 100000;  i++)
    {
    	len = read(audio_fd, buf, 256);
    	write(audio_fd, buf, len);
    }
    printf ("Stop\n");
    oss_release ();
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/
#endif
