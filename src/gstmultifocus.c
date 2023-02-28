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
    PROP_STRATEGY,
    PROP_multifocus_STATUS,
    PROP_STEP_SMALL,
    PROP_STEP_BIG,
    PROP_PDA_MIN,
    PROP_PDA_MAX,
    PROP_DEC_MAX,
    PROP_X,
    PROP_Y,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_OFFSET,
    PROP_CONTINUOUS,
    PROP_CONTINUOUS_UPDATE_INTERVAL,
    PROP_CONTINUOUS_TIMEOUT,
    PROP_CONTINUOUS_THRESHOLD,
    PROP_LISTEN,
    PROP_multifocus_LOST,
    PROP_SHARPNESS,
    PROP_CALIBRATING,
    PROP_DEBUG_LOG,
    PROP_DEBUG_LEVEL,
    PROP_PDA_HOLD_CMD
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

void printHelp()
{
    g_print("Help:\n");
    g_print("\ta: start multifocus\n");
    g_print("\ts: change multifocus strategy\n");
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

void *multifocusHandler(void *multifocus)
{
    Gstmultifocus *focus = (Gstmultifocus *)multifocus;

    if (focus != NULL)
    {
        while (listen)
        {
            char input;
            scanf(" %c", &input);

            if (input == 'a' && focus->multifocusStatus == COMPLETED)
            {
                focus->multifocusStatus = PENDING;
            }
            else if (input == 's' && focus->multifocusStatus == COMPLETED)
            {
                int newStrat;
                g_print("Choose an other multifocus strategy: ");
                scanf(" %d", &newStrat);
                if (newStrat < 0 || newStrat > 1)
                {
                    g_print("\tError: unknown strategy\n");
                }
                else
                {
                    focus->strategy = newStrat;
                    g_print("\tChanging multifocus strategy\n");
                }
            }
            else if (input == 'c')
            {
                focus->calibrating = TRUE;
                frameCount = 0;
                g_print("Calibrating multifocus...\n");
            }
            else
            {
                g_print("Unknown option or multifocus in progress\n");
                printHelp();
            }
        }
    }
    else
    {
        g_print("Error: multifocus is null\n");
    }

    pthread_exit(NULL);
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

    g_object_class_install_property(gobject_class, PROP_STRATEGY,
                                    g_param_spec_int("strategy", "Strategy",
                                                     "Set which algorithm is used to do the multifocus\n\t- 0 is the naive algorimth\n\t- 1 is the two pass algorimth",
                                                     0, 1, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_X,
                                    g_param_spec_int("x", "X", "The top left X coordinates of the ROI",
                                                     0, 1920, 0, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_Y,
                                    g_param_spec_int("y", "Y", "The top left Y coordinates of the ROI",
                                                     0, 1080, 0, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_WIDTH,
                                    g_param_spec_int("width", "Width", "The width of the ROI",
                                                     0, 1920, 1920, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_HEIGHT,
                                    g_param_spec_int("height", "Height", "The height of the ROI",
                                                     0, 1080, 1080, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_STEP_SMALL,
                                    g_param_spec_int("step_small", "Step_small", "The step of the PDA for the naive algorithm",
                                                     1, 700, 8, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_STEP_BIG,
                                    g_param_spec_int("step_big", "Step_big", "The step of the PDA for the two pass algorithm",
                                                     1, 700, 68, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PDA_MIN,
                                    g_param_spec_int("pda_min", "pda_min", "The minimal PDA value used for the multifocus algorithm",
                                                     0, 750, 200, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PDA_MAX,
                                    g_param_spec_int("pda_max", "pda_max", "The maximal PDA value used for the multifocus algorithm",
                                                     0, 750, 750, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DEC_MAX,
                                    g_param_spec_int("dec", "Dec", "The number of consecutive blurrier frames before stopping the multifocus",
                                                     0, 20, 3, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_multifocus_STATUS,
                                    g_param_spec_enum("multifocusStatus", "multifocusStatus", "The state of the multifocus:\n\tPENDING: the multifocus is about to start\n\tIN_PROGRESS: The multifocus is running\n\tCOMPLETED: The multifocus is done",
                                                      TYPE_multifocus_STATUS, COMPLETED, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_OFFSET,
                                    g_param_spec_int("offset", "Offset", "The frame offset between a pda command and the arrival of the corresponding frame in the plugin",
                                                     0, 100, 3, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTINUOUS_UPDATE_INTERVAL,
                                    g_param_spec_int("continuous_update_interval", "update", "How often should the sharness be calculated",
                                                     1, 120, 30, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTINUOUS_TIMEOUT,
                                    g_param_spec_int("continuous_timeout", "timeout", "The response time in frame",
                                                     1, 100, 4, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTINUOUS_THRESHOLD,
                                    g_param_spec_float("continuous_threshold", "threshold", "The threshold to determine if the image is blurrier and if the multifocus should be relaunched",
                                                       1.0f, 100.0f, 25.0f, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTINUOUS,
                                    g_param_spec_boolean("continuous", "Continuous",
                                                         "How many times should the sharpness calculated be under the threshold before relaunching the multifocus algorthim\nThis parameter has no effect if the parameter continuous is set to false",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_LISTEN,
                                    g_param_spec_boolean("listen", "Listen", "Listen for user input in the terminal.",
                                                         TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_multifocus_LOST,
                                    g_param_spec_boolean("focus_lost", "Focus lost", "Whether or not the focus has been lost",
                                                         TRUE, G_PARAM_READABLE));

    g_object_class_install_property(gobject_class, PROP_SHARPNESS,
                                    g_param_spec_long("sharpness", "Sharpness", "The sharpness of the frame",
                                                      0, G_MAXINT64, 0, G_PARAM_READABLE));

    g_object_class_install_property(gobject_class, PROP_CALIBRATING,
                                    g_param_spec_boolean("calibrating", "Calibrating", "Whether or not the plugin is calculating the response time",
                                                         FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DEBUG_LOG,
                                    g_param_spec_string("debug", "Debug", "Hold debug information about the last run of the multifocus",
                                                        "\0", G_PARAM_READABLE));

    g_object_class_install_property(gobject_class, PROP_DEBUG_LEVEL,
                                    g_param_spec_enum("debug_level", "Debug level", "The debugging level:\n\tnone(0): nothing is logged\n\tminimal(1): Only log the step, pda range and best focus found\n\tfull(2): Add on top of the minimal level information about each step of the algorithm",
                                                        TYPE_DEBUG_LEVEL, MINIMAL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_PDA_HOLD_CMD,
                                    g_param_spec_int("pda_hold_cmd", "pda_hold_cmd", "The number of frame between each command sent",
                                                     0, 1024, 0, G_PARAM_READWRITE));

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

    multifocus->multifocusStatus = COMPLETED;
    multifocus->strategy = TWO_PHASES;

    multifocus->listen = TRUE;

    multifocus->continuous = FALSE;
    multifocus->continuousUpdateInterval = 30;
    multifocus->continuousTimeout = 4;
    multifocus->continuousThreshold = 25.0f;

    multifocus->calibrating = FALSE;

    multifocus->multifocusLost = FALSE;

    multifocus->sharpness = 0;

    multifocus->debugInfo = NULL;
    multifocus->debugLvl  = MINIMAL;

    multifocus->pdaHoldCmd = 0;

    roi.x = 0;
    roi.y = 0;
    roi.width = 1920;
    roi.height = 1080;

    conf.pdaMin = 200;
    conf.pdaMax = 750;
    conf.pdaSmallStep = 8;
    conf.pdaBigStep = 63;
    conf.maxDec = 3;
    conf.offset = 3;
    conf.phase = PHASE_1;
    conf.debugLvl = MINIMAL;

    i2cInit(&device, &devicepda, &bus);

    pthread_t thread;

    int rc;
    if ((rc = pthread_create(&thread, NULL, multifocusHandler, (void *)multifocus)))
    {
        g_print("Error: unable to create thread, %d\n", rc);
        exit(-1);
    }
}

static void gst_multifocus_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec)
{
    Gstmultifocus *multifocus = GST_multifocus(object);

    switch (prop_id)
    {
    case PROP_STRATEGY:
        multifocus->strategy = g_value_get_int(value);
        break;
    case PROP_multifocus_STATUS:
    {
        multifocusStatus tmp = g_value_get_enum(value);

        // Prevent the multifocus from being restarted while it is in progress
        if (multifocus->multifocusStatus == COMPLETED && tmp == PENDING)
        {
            multifocus->multifocusStatus = tmp;
        }
        break;
    }
    case PROP_STEP_SMALL:
        conf.pdaSmallStep = g_value_get_int(value);
        break;
    case PROP_STEP_BIG:
        conf.pdaBigStep = g_value_get_int(value);
        break;
    case PROP_PDA_MIN:
        conf.pdaMin = g_value_get_int(value);
        break;
    case PROP_PDA_MAX:
        conf.pdaMax = g_value_get_int(value);
        break;
    case PROP_DEC_MAX:
        conf.maxDec = g_value_get_int(value);
        break;
    case PROP_X:
        roi.x = g_value_get_int(value);
        checkRoi();
        break;
    case PROP_Y:
        roi.y = g_value_get_int(value);
        checkRoi();
        break;
    case PROP_WIDTH:
        roi.width = g_value_get_int(value);
        checkRoi();
        break;
    case PROP_HEIGHT:
        roi.height = g_value_get_int(value);
        checkRoi();
        break;
    case PROP_OFFSET:
        conf.offset = g_value_get_int(value);
        break;
    case PROP_CONTINUOUS:
        multifocus->continuous = g_value_get_boolean(value);
        break;
    case PROP_CONTINUOUS_UPDATE_INTERVAL:
        multifocus->continuousUpdateInterval = g_value_get_int(value);
        break;
    case PROP_CONTINUOUS_TIMEOUT:
        multifocus->continuousTimeout = g_value_get_int(value);
        break;
    case PROP_CONTINUOUS_THRESHOLD:
        multifocus->continuousThreshold = g_value_get_float(value);
        break;
    case PROP_LISTEN:
        multifocus->listen = g_value_get_boolean(value);
        listen = multifocus->listen;
        break;
    case PROP_SHARPNESS:
        multifocus->sharpness = g_value_get_long(value);
        break;
    case PROP_CALIBRATING:
    {
        multifocus->calibrating = g_value_get_boolean(value);

        if (multifocus->calibrating == TRUE)
        {
            frameCount = 0;
            g_print("Calibrating multifocus...\n");
        }

        break;
    }
    case PROP_DEBUG_LEVEL:
        multifocus->debugLvl = g_value_get_enum(value);
        conf.debugLvl = multifocus->debugLvl;
        break;
    case PROP_PDA_HOLD_CMD:
        multifocus->pdaHoldCmd = g_value_get_int(value);
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
    case PROP_STRATEGY:
        g_value_set_int(value, multifocus->strategy);
        break;
    case PROP_multifocus_STATUS:
        g_value_set_enum(value, multifocus->multifocusStatus);
        break;
    case PROP_STEP_SMALL:
        g_value_set_int(value, conf.pdaSmallStep);
        break;
    case PROP_STEP_BIG:
        g_value_set_int(value, conf.pdaBigStep);
        break;
    case PROP_PDA_MIN:
        g_value_set_int(value, conf.pdaMin);
        break;
    case PROP_PDA_MAX:
        g_value_set_int(value, conf.pdaMax);
        break;
    case PROP_DEC_MAX:
        g_value_set_int(value, conf.maxDec);
        break;
    case PROP_X:
        g_value_set_int(value, roi.x);
        break;
    case PROP_Y:
        g_value_set_int(value, roi.y);
        break;
    case PROP_WIDTH:
        g_value_set_int(value, roi.width);
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, roi.height);
        break;
    case PROP_OFFSET:
        g_value_set_int(value, conf.offset);
        break;
    case PROP_CONTINUOUS:
        g_value_set_boolean(value, multifocus->continuous);
        break;
    case PROP_CONTINUOUS_UPDATE_INTERVAL:
        g_value_set_int(value, multifocus->continuousUpdateInterval);
        break;
    case PROP_CONTINUOUS_TIMEOUT:
        g_value_set_int(value, multifocus->continuousTimeout);
        break;
    case PROP_CONTINUOUS_THRESHOLD:
        g_value_set_float(value, multifocus->continuousThreshold);
        break;
    case PROP_LISTEN:
        g_value_set_boolean(value, multifocus->listen);
        break;
    case PROP_multifocus_LOST:
        g_value_set_boolean(value, multifocus->multifocusLost);
        break;
    case PROP_SHARPNESS:
        g_value_set_long(value, multifocus->sharpness);
        break;
    case PROP_CALIBRATING:
        g_value_set_boolean(value, multifocus->calibrating);
        break;
    case PROP_DEBUG_LOG:
        g_value_set_string(value, multifocus->debugInfo);
        break;
    case PROP_DEBUG_LEVEL:
        g_value_set_enum(value, multifocus->debugLvl);
        break;
    case PROP_PDA_HOLD_CMD:
        g_value_set_int(value, multifocus->pdaHoldCmd);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_multifocus_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    Gstmultifocus *multifocus = GST_multifocus(parent);

    int number_of_focus_points=5;
    int space_between_switch=10;
    int all_focus[5];
    multifocus->sharpness = getSharpness(pad, buf, roi);
    write_VdacPda(devicepda, bus, 500);

    all_focus[0]=100;
    all_focus[1]=200;
    all_focus[2]=300;
    all_focus[3]=400;
    all_focus[4]=500;
    frame++;
    if(frame==(space_between_switch+1))
    {
        write_VdacPda(devicepda, bus, all_focus[current_focus]);
        current_focus++;
        if(current_focus==5)
        {
            current_focus=0;
        }
    }

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
        if (nbFrame >= multifocus->continuousUpdateInterval)
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
