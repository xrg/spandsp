/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo_monitor.cpp - Display echo canceller status, using the FLTK toolkit.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: echo_monitor.cpp,v 1.1 2004/12/16 15:33:55 steveu Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include <FL/Fl.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Light_Button.H>
#include <Fl/Fl_Cartesian.H>
#include <FL/fl_draw.H>

#include "../src/spandsp/complex.h"
//#include "spandsp.h"
#include "echo_monitor.h"

Fl_Double_Window *w;
Fl_Group *c_right;
Fl_Group *c_eq;

Ca_Canvas *canvas_eq;

Ca_X_Axis *eq_x;
Ca_Y_Axis *eq_y;

Ca_Line *eq_re = NULL;

double eq_re_plot[512];

static int skip = 0;

int echo_can_monitor_update(const int16_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (eq_re)
        delete eq_re;

    canvas_eq->current(canvas_eq);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        eq_re_plot[2*i] = i;
        eq_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    eq_y->maximum((max == min)  ?  max + 0.2  :  max);
    eq_y->minimum(min);
    eq_re = new Ca_Line(len, eq_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}

int start_echo_can_monitor(int len)
{
    char buf[132 + 1];
    float x;
    float y;

    w = new Fl_Double_Window(905, 400, "Echo canceller monitor");

    c_right = new Fl_Group(440, 0, 465, 405);

    c_eq = new Fl_Group(380, 0, 415, 200);
    c_eq->box(FL_DOWN_BOX);
    c_eq->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_eq->current();

    canvas_eq = new Ca_Canvas(460, 35, 300, 100, "FIR coeffs");
    canvas_eq->box(FL_PLASTIC_DOWN_BOX);
    canvas_eq->color(7);
    canvas_eq->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_eq);
    canvas_eq->border(15);

    eq_x = new Ca_X_Axis(465, 135, 290, 30, "Tap");
    eq_x->align(FL_ALIGN_BOTTOM);
    eq_x->minimum(0.0);
    eq_x->maximum((float) len);
    eq_x->label_format("%g");
    eq_x->minor_grid_color(fl_gray_ramp(20));
    eq_x->major_grid_color(fl_gray_ramp(15));
    eq_x->label_grid_color(fl_gray_ramp(10));
    eq_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    eq_x->minor_grid_style(FL_DOT);
    eq_x->major_step(5);
    eq_x->label_step(1);
    eq_x->axis_align(CA_BOTTOM | CA_LINE);
    eq_x->axis_color(FL_BLACK);
    eq_x->current();

    eq_y = new Ca_Y_Axis(420, 40, 40, 90, "Amp");
    eq_y->align(FL_ALIGN_LEFT);
    eq_y->minimum(-0.1);
    eq_y->maximum(0.1);
    eq_y->minor_grid_color(fl_gray_ramp(20));
    eq_y->major_grid_color(fl_gray_ramp(15));
    eq_y->label_grid_color(fl_gray_ramp(10));
    eq_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    eq_y->minor_grid_style(FL_DOT);
    eq_y->major_step(5);
    eq_y->label_step(1);
    eq_y->axis_color(FL_BLACK);
    eq_y->current();

    c_eq->end();

    c_right->end();

    Fl_Group::current()->resizable(c_right);
    w->end();
    w->show();

    Fl::check();
    return 0;
}

void echo_can_monitor_wait_to_end(void) 
{
    fd_set rfds;
    int res;
    struct timeval tv;

    fprintf(stderr, "Processing complete.  Press the <enter> key to end\n");
    do
    {
        usleep(100000);
        Fl::check();
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        tv.tv_usec = 100000;
        tv.tv_sec = 0;
        res = select(1, &rfds, NULL, NULL, &tv);
    }
    while (res <= 0);
}
#endif
