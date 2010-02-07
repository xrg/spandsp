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
 * $Id: echo_monitor.cpp,v 1.3 2005/09/03 10:37:56 steveu Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)

#include <inttypes.h>
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
Fl_Group *c_can;
Fl_Group *c_line_model;

Ca_Canvas *canvas_can;
Ca_X_Axis *can_x;
Ca_Y_Axis *can_y;
Ca_Line *can_re = NULL;
double can_re_plot[512];

Ca_Canvas *canvas_line_model;
Ca_X_Axis *line_model_x;
Ca_Y_Axis *line_model_y;
Ca_Line *line_model_re = NULL;
double line_model_re_plot[512];

static int skip = 0;

int echo_can_monitor_can_update(const int16_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (can_re)
        delete can_re;

    canvas_can->current(canvas_can);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        can_re_plot[2*i] = i;
        can_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    can_y->maximum((max == min)  ?  max + 0.2  :  max);
    can_y->minimum(min);
    can_re = new Ca_Line(len, can_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}

int echo_can_monitor_line_model_update(const int32_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (line_model_re)
        delete line_model_re;

    canvas_line_model->current(canvas_line_model);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        line_model_re_plot[2*i] = i;
        line_model_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    line_model_y->maximum((max == min)  ?  max + 0.2  :  max);
    line_model_y->minimum(min);
    line_model_re = new Ca_Line(len, line_model_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
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

    c_can = new Fl_Group(380, 0, 415, 200);
    c_can->box(FL_DOWN_BOX);
    c_can->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_can->current();

    canvas_can = new Ca_Canvas(460, 35, 300, 100, "Canceller coefficients");
    canvas_can->box(FL_PLASTIC_DOWN_BOX);
    canvas_can->color(7);
    canvas_can->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_can);
    canvas_can->border(15);

    can_x = new Ca_X_Axis(465, 135, 290, 30, "Tap");
    can_x->align(FL_ALIGN_BOTTOM);
    can_x->minimum(0.0);
    can_x->maximum((float) len);
    can_x->label_format("%g");
    can_x->minor_grid_color(fl_gray_ramp(20));
    can_x->major_grid_color(fl_gray_ramp(15));
    can_x->label_grid_color(fl_gray_ramp(10));
    can_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    can_x->minor_grid_style(FL_DOT);
    can_x->major_step(5);
    can_x->label_step(1);
    can_x->axis_align(CA_BOTTOM | CA_LINE);
    can_x->axis_color(FL_BLACK);
    can_x->current();

    can_y = new Ca_Y_Axis(420, 40, 40, 90, "Amp");
    can_y->align(FL_ALIGN_LEFT);
    can_y->minimum(-0.1);
    can_y->maximum(0.1);
    can_y->minor_grid_color(fl_gray_ramp(20));
    can_y->major_grid_color(fl_gray_ramp(15));
    can_y->label_grid_color(fl_gray_ramp(10));
    can_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    can_y->minor_grid_style(FL_DOT);
    can_y->major_step(5);
    can_y->label_step(1);
    can_y->axis_color(FL_BLACK);
    can_y->current();

    c_can->end();

    c_line_model = new Fl_Group(380, 200, 415, 200);
    c_line_model->box(FL_DOWN_BOX);
    c_line_model->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_line_model->current();

    canvas_line_model = new Ca_Canvas(460, 235, 300, 100, "Line impulse response model");
    canvas_line_model->box(FL_PLASTIC_DOWN_BOX);
    canvas_line_model->color(7);
    canvas_line_model->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_line_model);
    canvas_line_model->border(15);

    line_model_x = new Ca_X_Axis(465, 335, 290, 30, "Tap");
    line_model_x->align(FL_ALIGN_BOTTOM);
    line_model_x->minimum(0.0);
    line_model_x->maximum((float) len);
    line_model_x->label_format("%g");
    line_model_x->minor_grid_color(fl_gray_ramp(20));
    line_model_x->major_grid_color(fl_gray_ramp(15));
    line_model_x->label_grid_color(fl_gray_ramp(10));
    line_model_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    line_model_x->minor_grid_style(FL_DOT);
    line_model_x->major_step(5);
    line_model_x->label_step(1);
    line_model_x->axis_align(CA_BOTTOM | CA_LINE);
    line_model_x->axis_color(FL_BLACK);
    line_model_x->current();

    line_model_y = new Ca_Y_Axis(420, 240, 40, 90, "Amp");
    line_model_y->align(FL_ALIGN_LEFT);
    line_model_y->minimum(-0.1);
    line_model_y->maximum(0.1);
    line_model_y->minor_grid_color(fl_gray_ramp(20));
    line_model_y->major_grid_color(fl_gray_ramp(15));
    line_model_y->label_grid_color(fl_gray_ramp(10));
    line_model_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    line_model_y->minor_grid_style(FL_DOT);
    line_model_y->major_step(5);
    line_model_y->label_step(1);
    line_model_y->axis_color(FL_BLACK);
    line_model_y->current();

    c_line_model->end();

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
