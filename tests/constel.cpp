#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <FL/Fl.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Light_Button.H>
#include <Fl/Fl_Cartesian.H>
#include <FL/fl_draw.H>

#include <../src/spandsp/complex.h>

Ca_X_Axis *sig_i;
Ca_Y_Axis *sig_q;
Ca_Y_Axis *current;
Ca_Y_Axis *phase;

Ca_Canvas *canvas;

Fl_Double_Window *w = new Fl_Double_Window(580, 480, "QAM space");
Fl_Group *c = new Fl_Group(0, 35, 580, 445);

Ca_Line *rel = NULL;
Ca_Line *iml = NULL;
double re_plot[100];
double im_plot[100];

extern "C"
{
    int start_qam_monitor(void);
    int update_qam_monitor(complex_t *pt);
    int update_qam_equalizer_monitor(complex_t *coeffs, int len);
};

int skip = 0;

int update_qam_monitor(complex_t *pt)
{
    int i;

    new Ca_Point(pt->re, pt->im, FL_BLACK);
    if (++skip >= 48)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}

int update_qam_equalizer_monitor(complex_t *coeffs, int len)
{
    int i;

    if (rel)
        delete rel;
    if (iml)
        delete iml;

    for (i = 0;  i < len;  i++)
    {
        re_plot[2*i] = i/2.0;
        im_plot[2*i] = i/2.0;
        re_plot[2*i + 1] = coeffs[i].re;
        im_plot[2*i + 1] = coeffs[i].im;
    }
    rel = new Ca_Line(len, re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    iml = new Ca_Line(len, im_plot, 0, 0, FL_RED, CA_NO_POINT);
    Fl::check();
    return 0;
}

int start_qam_monitor(void) 
{
    char buf[132 + 1];
    float x;
    float y;
    int skip;
    FILE *fin;

    c->box(FL_DOWN_BOX);
    c->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);

    canvas = new Ca_Canvas(180, 75, 300, 300, "QAM space");
    canvas->box(FL_PLASTIC_DOWN_BOX);
    canvas->color(7);
    canvas->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas);
    // w->resizable(canvas);
    canvas->border(15);

    sig_i = new Ca_X_Axis(185, 377, 295, 30, "I");
    sig_i->align(FL_ALIGN_BOTTOM);
    sig_i->minimum(-6.0);
    sig_i->maximum(6.0);
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

    sig_q = new Ca_Y_Axis(100, 70, 78, 295, "Q");
    sig_q->align(FL_ALIGN_LEFT);
    sig_q->minimum(-6.0);
    sig_q->maximum(6.0);
    sig_q->minor_grid_color(fl_gray_ramp(20));
    sig_q->major_grid_color(fl_gray_ramp(15));
    sig_q->label_grid_color(fl_gray_ramp(10));
    //sig_q->grid_visible(CA_MINOR_TICK | CA_MAJOR_TICK | CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sig_q->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sig_q->minor_grid_style(FL_DOT);
    sig_q->major_step(5);
    sig_q->label_step(1);

    sig_q->current();

    c->end();

    Fl_Group::current()->resizable(c);
    w->end();
    w->show();

    Fl::check();
    return 0;
}
