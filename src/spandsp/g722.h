/*
 * libg722 - a library for the G.722 codec.
 *
 * g722.h
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
 * Based on a single channel G.722 codec which is:
 *
 *****    Copyright (c) CMU    1993      *****
 * Computer Science, Speech Group
 * Chengxiang Lu and Alex Hauptmann
 *
 * $Id: g722.h,v 1.2 2005/09/04 07:40:03 steveu Exp $
 */

typedef struct
{
    int rate;
    /* storage for signal passing */
    int x[24];
    /* even and odd tap accumulators */
    int sumeven;
    int sumodd;

    int sl;
    int spl;
    int szl;
    int rlt[3];
    int al[3];
    int plt[3];
    int dlt[7];
    int bl[7];
    int sgl[7];
    int nbl;

    int sh;
    int sph;
    int szh;
    int rh[3];
    int ah[3];
    int ph[3];
    int dh[7];
    int bh[7];
    int sgh[7];
    int nbh;

    int slow;
    int detlow;
    int shigh;
    int dethigh;

    int k;
    int ilow;
    int ihigh;

} g722_encode_state_t;

typedef struct
{
    int rate;
    int k;
    int xd[12];
    int xs[12];

    int dlt[7];
    int bl[7];
    int bpl[7];
    int sg[7];

    int sg0;
    int sg1;
    int sg2;
    int plt0;
    int plt1;
    int plt2;
    int rlt0;
    int rlt1;
    int rlt2;

    int sgh0;
    int sgh1;
    int sgh2;
    int sgh[7];
    int nbh;

    int rh0;
    int rh1;
    int rh2;
    int ah1;
    int ah2;
    int ph0;
    int ph1;
    int ph2;

    int sl;
    int spl;
    int szl;

    int sh;
    int sph;
    int szh;

    int al1;
    int al2;

    int dh[7];
    int bh[7];
    int bph[7];

    int slow;
    int detlow;
    int shigh;
    int dethigh;
} g722_decode_state_t;

int g722_encode_init(g722_encode_state_t *s, int rate);
int g722_encode(g722_encode_state_t *s, uint16_t *outbuf, const int16_t *buf, int len);

int g722_decode_init(g722_decode_state_t *s, int rate);
int g722_decode(g722_decode_state_t *s, int16_t *outbuf, const uint16_t *buf, int len);
