/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2022 Nicolas <<user@hostname.org>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_multifocus_H__
#define __GST_multifocus_H__

#include <gst/gst.h>

#include "multifocusControl.h"

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_multifocus \
    (gst_multifocus_get_type())
#define GST_multifocus(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_multifocus, Gstmultifocus))
#define GST_multifocus_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_multifocus, GstmultifocusClass))
#define GST_IS_multifocus(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_multifocus))
#define GST_IS_multifocus_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_multifocus))

typedef struct _Gstmultifocus Gstmultifocus;
typedef struct _GstmultifocusClass GstmultifocusClass;

typedef enum
{
    PENDING,
    IN_PROGRESS,
    WAITING,
    COMPLETED
} multifocusStatus;
int all_focus[50];
int indice_next = 0;
int frame = 0;
int step1 = 0;
int step2 = 0;
int current_focus = 0;
int plans = 0;
int sharpness_of_plans[100];
int start = 0;
int searching_plans = 0;
int i2c_err = 0;
int plans_int[50];
struct _Gstmultifocus
{
    GstElement element;

    GstPad *sinkpad, *srcpad;
    gboolean work;
    gboolean reset;
    gint number_of_plans;
    gint latency;
    gint wait_after_start;
    gint space_between_switch;
    gint ROI1x;
    gint ROI1y;
    gint ROI2x;
    gint ROI2y;
    gboolean auto_detect_plans;
    gboolean next;
    gchar* plans;
    
};

struct _GstmultifocusClass
{
    GstElementClass parent_class;
};

GType gst_multifocus_get_type(void);

G_END_DECLS

#endif /* __GST_multifocus_H__ */
