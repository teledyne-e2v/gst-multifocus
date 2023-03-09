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

/**
 * SECTION:element-multifocus
 *
 * FIXME:Describe multifocus here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! multifocus ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <gst/gst.h>
#include <unistd.h>
#include <stdio.h>

#include "gstmultifocus.h"
#include "i2c_control.h"

GST_DEBUG_CATEGORY_STATIC(gst_multifocus_debug);
#define GST_CAT_DEFAULT gst_multifocus_debug

/* Filter signals and args */
enum
{
    /* FILL ME */
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_WORK,
    PROP_LATENCY,
    PROP_NUMBER_OF_PLANS,
    PROP_WAIT_AFTER_START,
    PROP_RESET,
    PROP_SPACE_BETWEEN_SWITCH
};

I2CDevice device;
I2CDevice devicepda;
int bus;

ROI roi;
multifocusConf conf;

int listen = 1;
int frameCount = 0;

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink",
                                                                   GST_PAD_SINK,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS("ANY"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src",
                                                                  GST_PAD_SRC,
                                                                  GST_PAD_ALWAYS,
                                                                  GST_STATIC_CAPS("ANY"));

#define gst_multifocus_parent_class parent_class
G_DEFINE_TYPE(Gstmultifocus, gst_multifocus, GST_TYPE_ELEMENT)

static void gst_multifocus_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec);
static void gst_multifocus_get_property(GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec);

static GstFlowReturn gst_multifocus_chain(GstPad *pad, GstObject *parent, GstBuffer *buf);

static void gst_multifocus_finalize(GObject *object);

#define TYPE_multifocus_STATUS (multifocus_status_get_type())
static GType
multifocus_status_get_type(void)
{
    static GType multifocus_status = 0;

    if (!multifocus_status)
    {
        static const GEnumValue status[] =
            {
                {PENDING, "Pending", "pending"},
                {IN_PROGRESS, "In progress", "in_progress"},
                {COMPLETED, "Completed", "completed"},
                {0, NULL, NULL}
            };

        multifocus_status = g_enum_register_static("multifocusStatus", status);
    }

    return multifocus_status;
}

#define TYPE_DEBUG_LEVEL (debug_level_get_type())
static GType
debug_level_get_type(void)
{
    static GType debug_level = 0;

    if (!debug_level)
    {
        static const GEnumValue status[] =
            {
                {NONE, "None", "none"},
                {MINIMAL, "Minimal", "minimal"},
                {FULL, "Full", "full"},
                {0, NULL, NULL}
            };

        debug_level = g_enum_register_static("DebugLevel", status);
    }

    return debug_level;
}


/**
 * @brief Prevent the ROI from protuding from the image
 */
static void checkRoi()
{
    // Prevent the ROI from being to close to the very end of the frame as it migth crash when calculating the sharpness
    if (roi.x > 1916)
    {
        roi.x = 1916;
    }

    if (roi.y > 1076)
    {
        roi.y = 1076;
    }

    // Prevent the ROI from going outsides the bounds of the image
    if (roi.x + roi.width >= 1920)
    {
        roi.width -= ((roi.x + roi.width) - 1920);
    }

    if (roi.y + roi.height > 1080)
    {
        roi.height -= ((roi.y + roi.height) - 1080);
    }
}



/* GObject vmethod implementations */

/* initialize the multifocus's class */
static void gst_multifocus_class_init(GstmultifocusClass *klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass *)klass;
    gstelement_class = (GstElementClass *)klass;

    gobject_class->set_property = gst_multifocus_set_property;
    gobject_class->get_property = gst_multifocus_get_property;
    gobject_class->finalize     = gst_multifocus_finalize;

    g_object_class_install_property(gobject_class, PROP_LATENCY,
                                    g_param_spec_int("latency", "Latency", "Latency between command and command effect on gstreamer",
                                                     1, 120, 3, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUMBER_OF_PLANS,
                                    g_param_spec_int("number_of_plans", "Number_of_plans", 
                                    "Number of plans to focus on",
                                                     1, 50, 4, G_PARAM_READWRITE));;
    g_object_class_install_property(gobject_class, PROP_WAIT_AFTER_START,
                                    g_param_spec_int("wait_after_start", "Wait_after_start", "Latency between command and command effect on gstreamer",
                                                     1, 120, 30, G_PARAM_READWRITE));                                          

    g_object_class_install_property(gobject_class, PROP_WORK,
                                    g_param_spec_boolean("work", "Work",
                                                         "Set plugin to work",
                                                         TRUE, G_PARAM_READWRITE));
g_object_class_install_property(gobject_class, PROP_SPACE_BETWEEN_SWITCH,
                                    g_param_spec_int("space_between_switch", "Space_between_switch",
                                                         "number of images separating, switch",
                                                         1, 120, 30, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_RESET,
                                    g_param_spec_boolean("reset", "Reset",
                                                         "Reset the Multifocus plans",
                                                         TRUE, G_PARAM_READWRITE));
    gst_element_class_set_details_simple(gstelement_class,
                                         "multifocus",
                                         "FIXME:Generic",
                                         "multifocus of snappy2M module",
                                         "Esisar-PI2022 <<user@hostname.org>>");

    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(gstelement_class,
                                       gst_static_pad_template_get(&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_multifocus_init(Gstmultifocus *multifocus)
{
    multifocus->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_chain_function(multifocus->sinkpad,
                               GST_DEBUG_FUNCPTR(gst_multifocus_chain));
    GST_PAD_SET_PROXY_CAPS(multifocus->sinkpad);
    gst_element_add_pad(GST_ELEMENT(multifocus), multifocus->sinkpad);

    multifocus->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    GST_PAD_SET_PROXY_CAPS(multifocus->srcpad);
    gst_element_add_pad(GST_ELEMENT(multifocus), multifocus->srcpad);

    multifocus->work = TRUE;
    multifocus->latency = 3;
    multifocus->number_of_plans = 4;
    multifocus->wait_after_start=15;
    multifocus->space_between_switch=30;
    roi.x = 0;
    roi.y = 0;
    roi.width = 1920;
    roi.height = 1080;
    multifocus->reset=false;

    i2cInit(&device, &devicepda, &bus);
	
    for(int i=0;i<100;i++){    sharpness_of_plans[i]=0;}


}

static void gst_multifocus_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec)
{
    Gstmultifocus *multifocus = GST_multifocus(object);

    switch (prop_id)
    {
    case PROP_WORK:
        multifocus->work = g_value_get_boolean(value);
        break;
    case PROP_LATENCY:
        multifocus->latency = g_value_get_int(value);
        break;
    case PROP_NUMBER_OF_PLANS:
        multifocus->number_of_plans = g_value_get_int(value);
        break;
    case PROP_WAIT_AFTER_START:
        multifocus->wait_after_start = g_value_get_int(value);
        break;
    case PROP_SPACE_BETWEEN_SWITCH:
        multifocus->space_between_switch = g_value_get_int(value);
        break;
    case PROP_RESET:
        multifocus->reset = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_multifocus_get_property(GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec)
{
    Gstmultifocus *multifocus = GST_multifocus(object);

    switch (prop_id)
    {
    case PROP_WORK:
        g_value_set_boolean(value, multifocus->work);
        break;
    case PROP_LATENCY:
        g_value_set_int(value, multifocus->latency);
        break;
    case PROP_NUMBER_OF_PLANS:
        g_value_set_int(value, multifocus->number_of_plans);
        break;
    case PROP_WAIT_AFTER_START:
        g_value_set_int(value, multifocus->wait_after_start);
        break;
    case PROP_RESET:
        g_value_set_boolean(value, multifocus->reset);
        break;
    case PROP_SPACE_BETWEEN_SWITCH:
        g_value_set_int(value, multifocus->space_between_switch);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

int maximum_and_zero(int *tab, int *spot,int number_of_spot){
    int max=0;
    int indice=0;
    for(int i=0;i<number_of_spot;i++)
    {
        if(tab[spot[i]]>max)
        {
            indice=spot[i];
            max=tab[spot[i]];
        }
    }
    tab[indice]=-1;
    return indice;
}


void find_best_plans(GstPad *pad,GstBuffer *buf,int number_of_focus,int latency)
{

	if(frame>latency){

		sharpness_of_plans[frame-latency] = getSharpness(pad, buf, roi);
    }
		//g_print("sharp : %d\n",sharpness_of_plans[frame-latency]);}
	if(frame<70)
		{
		write_VdacPda(devicepda, bus, (frame)*10);
		g_print("frame : %d\n",frame);
		}

	else{

	int derivate[99];
        for(int i=0;i<99;i++)
        {
            derivate[i]=sharpness_of_plans[i+1]-sharpness_of_plans[i];
        }
        int spot[50];
        int spot_number=0;
        for(int i=0;i<99;i++)
        {
            if(derivate[i]>0 && derivate[i+1]<0)
            {
                spot[spot_number]=i;
                spot_number++;
            }
        }
        if(number_of_focus>spot_number)
        {
            for(int i=0;i<spot_number;i++)
            {
                all_focus[i]=spot[i]*10;
            }
        }
        else{

            for(int i=0;i<number_of_focus;i++)
            {
		
		int indice=maximum_and_zero(sharpness_of_plans,spot,spot_number);
                all_focus[i]=indice*10;
            }
	g_print(" best plans :%d, %d, %d\n",all_focus[0],all_focus[1],all_focus[2]);
        }
        

	}

}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_multifocus_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{

    Gstmultifocus *multifocus = GST_multifocus(parent);

    int number_of_focus_points=3;


    if(start==0 && frame>multifocus->wait_after_start)
    {
        frame=0;
        start=1;
    }


    if(multifocus->work==true && start==1)
{
    if(frame<71)
{
	find_best_plans(pad,buf,number_of_focus_points,multifocus->latency);

}
else{
    
    

    if(multifocus->reset==true)
    {
	multifocus->wait_after_start=0;
        frame=multifocus->wait_after_start;

        multifocus->reset=false;

    }
    else if(frame%(multifocus->space_between_switch+1)==0)
    {
        write_VdacPda(devicepda, bus, all_focus[current_focus]);
        current_focus++;
        if(current_focus==number_of_focus_points)
        {
            current_focus=0;
        }
    }
    
}
}
frame++;


    /*
    static struct timeval start, end;

    static long sharpness = -1;
    static int nbFrame = 0;
    static int lostFocusCount = 0;
    static short int buffering = 8;
    static int waitRemaining = 0;


    multifocus->sharpness = getSharpness(pad, buf, roi);

    // g_print("%ld\n", multifocus->sharpness);

    if (multifocus->calibrating == TRUE && multifocus->multifocusStatus == COMPLETED)
    {
        static long int prevSharpness = 0;
        static int offset = 0;
        
        if (frameCount == 0)
        {
            write_VdacPda(devicepda, bus, 0); // return to PDA 0
        }
        else if (frameCount == 5) // Wait for 5 frame, then send a command
        {
            write_VdacPda(devicepda, bus, 500); // go to PDA 500
        }
        else if (frameCount >= 5)
        {
            double relativeDiff = ((prevSharpness - multifocus->sharpness) / (double)multifocus->sharpness) * 100.0f;
            
            offset++;
            
            if (relativeDiff >= 25.0f || relativeDiff <= -25.0f)
            {
                conf.offset = offset - multifocus->pdaHoldCmd;
                if (conf.offset < 0)
                    conf.offset = 0;
                
                multifocus->calibrating = FALSE;

                g_print("Calibration complete: the new offset is %d\n", conf.offset);
                prevSharpness = 0;
                offset = 0;
            }
        }
        if (frameCount >= 60)
        {
            g_print("The calibration is too long, aborting\n");
            multifocus->calibrating = FALSE;
            prevSharpness = 0;
            offset = 0;
        }
        
        prevSharpness = multifocus->sharpness;

        frameCount++;
    }
    else if (multifocus->multifocusStatus == PENDING) // Get the time at the start of multifocus
    {
        resetDebugInfo();
        g_print("Starting the multifocus\n\n");
        
        resetmultifocus(multifocus->strategy, &conf, &devicepda, bus);

        waitRemaining = multifocus->pdaHoldCmd;

        multifocus->multifocusStatus = (waitRemaining == 0) ? IN_PROGRESS : WAITING;
        
        gettimeofday(&start, NULL);
    }
    else if (multifocus->multifocusStatus == WAITING)
    {
        waitRemaining--;

        if ((waitRemaining) <= 0)
            multifocus->multifocusStatus = IN_PROGRESS;
    }
    else if (multifocus->multifocusStatus == IN_PROGRESS)
    {
        if (multifocus->strategy == NAIVE)
            sharpness = naivemultifocus(&devicepda, bus, multifocus->sharpness);
        else if (multifocus->strategy == TWO_PHASES)
            sharpness = twoPhasemultifocus(&devicepda, bus, multifocus->sharpness);
        else
        {
            g_print("Error: Unknown multifocus strategy!\n");

            sharpness = -1;
            multifocus->multifocusStatus = COMPLETED;
        }

        if (sharpness != -1)
        {
            gettimeofday(&end, NULL); // Get the time when the multifocus ended

            double elapsed =
                ((end.tv_sec * 1000000 + end.tv_usec) -
                 (start.tv_sec * 1000000 + start.tv_usec)) /
                1000000.0f;

            logmultifocusTime(elapsed);
            
            char *tmp;
            size_t logLen = 0;
            tmp = getDebugInfo(&logLen);

            if (logLen != 0)
            {
                multifocus->debugInfo = (char*)realloc(multifocus->debugInfo, sizeof(char) * (logLen + 1));
                multifocus->debugInfo = strncpy(multifocus->debugInfo, tmp, logLen);
                multifocus->debugInfo[logLen] = '\0';
                free(tmp);
            }

            multifocus->multifocusStatus = COMPLETED;
            
            buffering = conf.offset * 2; // Prevent the continuous multifocus from starting before the first sharp frame arrive
        }
        else if (multifocus->pdaHoldCmd > 0)
        {
            waitRemaining = multifocus->pdaHoldCmd;
            multifocus->multifocusStatus = WAITING;
        }
    }
    else if (sharpness != -1 && buffering == 0) // When the multifocus has finish check if the frame is still sharp after a little while
    {
        if (nbFrame >= multifocus->latency)
        {
            double relativeDiff = ((sharpness - multifocus->sharpness) / (double)multifocus->sharpness) * 100.0f;

            if (relativeDiff > multifocus->continuousThreshold || relativeDiff < -multifocus->continuousThreshold)
            {
                g_print("Warning: focus has been lost (may be); %ld\n", multifocus->sharpness);

                multifocus->multifocusLost = TRUE;
                lostFocusCount++;
            }
            else
            {
                multifocus->multifocusLost = FALSE;
                lostFocusCount = 0;
            }

            if (lostFocusCount > multifocus->continuousTimeout && multifocus->continuous == TRUE)
            {
                resetmultifocus(multifocus->strategy, &conf, &devicepda, bus);
                multifocus->multifocusStatus = PENDING;
                lostFocusCount = 0;
                g_print("Trying to refocus the frame...\n");
            }

            nbFrame = 0;
        }

        nbFrame++;
    }
    else
    {
        buffering--;
    }
    */
    /* just push out the incoming buffer */
    return gst_pad_push(multifocus->srcpad, buf);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean multifocus_init(GstPlugin *multifocus)
{
    /* debug category for fltering log messages
     *
     * exchange the string 'Template multifocus' with your description
     */
    GST_DEBUG_CATEGORY_INIT(gst_multifocus_debug, "multifocus",
                            0, "Template multifocus");

    return gst_element_register(multifocus, "multifocus", GST_RANK_NONE,
                                GST_TYPE_multifocus);
}

static void gst_multifocus_finalize(GObject *object)
{
    disable_VdacPda(devicepda, bus);
    i2c_close(bus);
    g_print("Bus closed\n");
    freeDebugInfo();
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstmultifocus"
#endif

/* gstreamer looks for this structure to register multifocuss
 *
 * exchange the string 'Template multifocus' with your multifocus description
 */
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    multifocus,
    "Template multifocus",
    multifocus_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/")
