/*
 * SpanDSP - a series of DSP components for telephony
 *
 * constel.cpp - Display QAM constellations, using the FLTK toolkit.
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
 * $Id: constel.cpp,v 1.12 2004/12/16 15:33:55 steveu Exp $
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
#include "constel.h"

Fl_Double_Window *w;
Fl_Group *c_const;
Fl_Group *c_right;
Fl_Group *c_eq;
Fl_Group *c_symbol_track;

Ca_Canvas *canvas_const;
Ca_Canvas *canvas_eq;
Ca_Canvas *canvas_track;

Ca_X_Axis *sig_i;
Ca_Y_Axis *sig_q;

Ca_X_Axis *eq_x;
Ca_Y_Axis *eq_y;

Ca_X_Axis *track_x;
Ca_Y_Axis *symbol_track_y;
Ca_Y_Axis *carrier_y;
int first_carrier_sample = true;

Ca_Line *eq_re = NULL;
Ca_Line *eq_im = NULL;

double eq_re_plot[100];
double eq_im_plot[100];

Ca_Line *symbol_track = NULL;
Ca_Line *carrier = NULL;

int symbol_tracker[100000];
double symbol_track_plot[100000];
int symbol_track_points;
int symbol_track_window;

float carrier_tracker[100000];
double carrier_plot[100000];
int carrier_points;
int carrier_window;

Ca_Point *constel_point[100000];
int constel_window;
int constel_points;
static int skip = 0;

int update_qam_monitor(const complex_t *pt)
{
    int i;

    canvas_const->current(canvas_const);
    if (constel_point[constel_points])
        delete constel_point[constel_points];
    constel_point[constel_points++] = new Ca_Point(pt->re, pt->im, FL_BLACK);
    if (constel_points >= constel_window)
        constel_points = 0;
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}

int update_qam_equalizer_monitor(const complex_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (eq_re)
        delete eq_re;
    if (eq_im)
        delete eq_im;

    canvas_eq->current(canvas_eq);
    i = 0;
    min = coeffs[i].re;
    if (min > coeffs[i].im)
        min = coeffs[i].im;
    max = coeffs[i].re;
    if (max < coeffs[i].im)
        max = coeffs[i].im;
    for (i = 0;  i < len;  i++)
    {
        eq_re_plot[2*i] = (i - len/2)/2.0;
        eq_im_plot[2*i] = (i - len/2)/2.0;
        eq_re_plot[2*i + 1] = coeffs[i].re;
        if (min > coeffs[i].re)
            min = coeffs[i].re;
        if (max < coeffs[i].re)
            max = coeffs[i].re;
        eq_im_plot[2*i + 1] = coeffs[i].im;
        if (min > coeffs[i].im)
            min = coeffs[i].im;
        if (max < coeffs[i].im)
            max = coeffs[i].im;
    }
    
    eq_y->maximum((max == min)  ?  max + 0.2  :  max);
    eq_y->minimum(min);
    eq_re = new Ca_Line(len, eq_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    eq_im = new Ca_Line(len, eq_im_plot, 0, 0, FL_RED, CA_NO_POINT);
    Fl::check();
    return 0;
}

int update_qam_symbol_tracking(int total_correction)
{
    int i;
    int min;
    int max;
#define SYMBOL_TRACKER_POINTS   12000

    memcpy(&symbol_tracker[0], &symbol_tracker[1], sizeof(symbol_tracker[0])*(SYMBOL_TRACKER_POINTS - 1));
    symbol_tracker[SYMBOL_TRACKER_POINTS - 1] = total_correction;
    
    canvas_track->current(canvas_track);
    if (symbol_track)
        delete symbol_track;
    track_x->current();
    symbol_track_y->current();

    min =
    max = symbol_tracker[0];
    for (i = 0;  i < SYMBOL_TRACKER_POINTS;  i++)
    {
        symbol_track_plot[2*i] = i;
        symbol_track_plot[2*i + 1] = symbol_tracker[i];
        if (min > symbol_tracker[i])
            min = symbol_tracker[i];
        if (max < symbol_tracker[i])
            max = symbol_tracker[i];
    }
    symbol_track_y->maximum((max == min)  ?  max + 0.2  :  max);
    symbol_track_y->minimum(min);

    symbol_track = new Ca_Line(SYMBOL_TRACKER_POINTS, symbol_track_plot, 0, 0, FL_RED, CA_NO_POINT);
    //Fl::check();
    return 0;
}

int update_qam_carrier_tracking(float carrier_freq)
{
    int i;
    float min;
    float max;
#define CARRIER_TRACKER_POINTS  12000

    if (first_carrier_sample)
    {
        first_carrier_sample = false;
        for (i = 0;  i < CARRIER_TRACKER_POINTS;  i++)
            carrier_tracker[i] = carrier_freq;
    }
    memcpy(&carrier_tracker[0], &carrier_tracker[1], sizeof(carrier_tracker[0])*(CARRIER_TRACKER_POINTS - 1));
    carrier_tracker[CARRIER_TRACKER_POINTS - 1] = carrier_freq;
    
    canvas_track->current(canvas_track);
    if (carrier)
        delete carrier;
    track_x->current();
    carrier_y->current();

    min =
    max = carrier_tracker[0];
    for (i = 0;  i < CARRIER_TRACKER_POINTS;  i++)
    {
        carrier_plot[2*i] = i;
        carrier_plot[2*i + 1] = carrier_tracker[i];
        if (min > carrier_tracker[i])
            min = carrier_tracker[i];
        if (max < carrier_tracker[i])
            max = carrier_tracker[i];
    }

    carrier_y->maximum((max == min)  ?  max + 0.2  :  max);
    carrier_y->minimum(min);

    carrier = new Ca_Line(CARRIER_TRACKER_POINTS, carrier_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    //Fl::check();
    return 0;
}

int start_qam_monitor(float constel_width) 
{
    char buf[132 + 1];
    float x;
    float y;

    w = new Fl_Double_Window(905, 400, "QAM monitor");

    c_const = new Fl_Group(0, 0, 380, 400);
    c_const->box(FL_DOWN_BOX);
    c_const->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);

    canvas_const = new Ca_Canvas(60, 30, 300, 300, "Constellation");
    canvas_const->box(FL_PLASTIC_DOWN_BOX);
    canvas_const->color(7);
    canvas_const->align(FL_ALIGN_TOP);
    canvas_const->border(15);

    sig_i = new Ca_X_Axis(65, 330, 290, 30, "I");
    sig_i->align(FL_ALIGN_BOTTOM);
    sig_i->minimum(-constel_width);
    sig_i->maximum(constel_width);
    sig_i->label_format("%g");
    sig_i->minor_grid_color(fl_gray_ramp(20));
    sig_i->major_grid_color(fl_gray_ramp(15));
    sig_i->label_grid_color(fl_gray_ramp(10));
    sig_i->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sig_i->minor_grid_style(FL_DOT);
    sig_i->major_step(5);
    sig_i->label_step(1);
    sig_i->axis_color(FL_BLACK);
    sig_i->axis_align(CA_BOTTOM | CA_LINE);

    sig_q = new Ca_Y_Axis(20, 35, 40, 290, "Q");
    sig_q->align(FL_ALIGN_LEFT);
    sig_q->minimum(-constel_width);
    sig_q->maximum(constel_width);
    sig_q->minor_grid_color(fl_gray_ramp(20));
    sig_q->major_grid_color(fl_gray_ramp(15));
    sig_q->label_grid_color(fl_gray_ramp(10));
    //sig_q->grid_visible(CA_MINOR_TICK | CA_MAJOR_TICK | CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sig_q->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sig_q->minor_grid_style(FL_DOT);
    sig_q->major_step(5);
    sig_q->label_step(1);
    sig_q->axis_color(FL_BLACK);

    sig_q->current();

    c_const->end();

    c_right = new Fl_Group(440, 0, 465, 405);

    c_eq = new Fl_Group(380, 0, 265, 200);
    c_eq->box(FL_DOWN_BOX);
    c_eq->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_eq->current();
    canvas_eq = new Ca_Canvas(460, 35, 150, 100, "Equalizer");
    canvas_eq->box(FL_PLASTIC_DOWN_BOX);
    canvas_eq->color(7);
    canvas_eq->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_eq);
    canvas_eq->border(15);

    eq_x = new Ca_X_Axis(465, 135, 140, 30, "Symbol");
    eq_x->align(FL_ALIGN_BOTTOM);
    eq_x->minimum(-4.0);
    eq_x->maximum(4.0);
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

    c_symbol_track = new Fl_Group(380, 200, 525, 200);
    c_symbol_track->box(FL_DOWN_BOX);
    c_symbol_track->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_symbol_track->current();

    canvas_track = new Ca_Canvas(490, 235, 300, 100, "Symbol and carrier tracking");
    canvas_track->box(FL_PLASTIC_DOWN_BOX);
    canvas_track->color(7);
    canvas_track->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_track);
    canvas_track->border(15);

    track_x = new Ca_X_Axis(495, 335, 290, 30, "Time (symbols)");
    track_x->align(FL_ALIGN_BOTTOM);
    track_x->minimum(0.0);
    track_x->maximum(2400.0*5.0);
    track_x->label_format("%g");
    track_x->minor_grid_color(fl_gray_ramp(20));
    track_x->major_grid_color(fl_gray_ramp(15));
    track_x->label_grid_color(fl_gray_ramp(10));
    track_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    track_x->minor_grid_style(FL_DOT);
    track_x->major_step(5);
    track_x->label_step(1);
    track_x->axis_align(CA_BOTTOM | CA_LINE);
    track_x->axis_color(FL_BLACK);
    track_x->current();

    symbol_track_y = new Ca_Y_Axis(420, 240, 70, 90, "Cor");
    symbol_track_y->align(FL_ALIGN_LEFT);
    symbol_track_y->minimum(-0.1);
    symbol_track_y->maximum(0.1);
    symbol_track_y->minor_grid_color(fl_gray_ramp(20));
    symbol_track_y->major_grid_color(fl_gray_ramp(15));
    symbol_track_y->label_grid_color(fl_gray_ramp(10));
    symbol_track_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    symbol_track_y->minor_grid_style(FL_DOT);
    symbol_track_y->major_step(5);
    symbol_track_y->label_step(1);
    symbol_track_y->axis_color(FL_RED);
    symbol_track_y->current();

    carrier_y = new Ca_Y_Axis(790, 240, 70, 90, "Freq");
    carrier_y->align(FL_ALIGN_RIGHT);
    carrier_y->minimum(-0.1);
    carrier_y->maximum(0.1);
    carrier_y->minor_grid_color(fl_gray_ramp(20));
    carrier_y->major_grid_color(fl_gray_ramp(15));
    carrier_y->label_grid_color(fl_gray_ramp(10));
    carrier_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    carrier_y->minor_grid_style(FL_DOT);
    carrier_y->major_step(5);
    carrier_y->label_step(1);
    carrier_y->axis_align(CA_RIGHT);
    carrier_y->axis_color(FL_BLUE);
    carrier_y->current();

    c_symbol_track->end();

    c_right->end();

    Fl_Group::current()->resizable(c_right);
    w->end();
    w->show();

    constel_points = 0;
    constel_window = 10000;

    symbol_track_points = 0;
    symbol_track_window = 10000;
    Fl::check();
    return 0;
}

void qam_wait_to_end(void) 
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
