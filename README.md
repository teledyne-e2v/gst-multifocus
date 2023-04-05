# Version 1.0

# About

This plugin implements a basic multifocus algorithm.

The principle of multifocus is to change focus at regular intervals, it can be used for example to decode barcodes located on different focal planes or also to reconstruct a clear image at all planes.

It require **OPTIMOM 2M** driver installed on the system.

# Compilation

First you must make sure that your device's clock is correctly setup.
Otherwise the compilation will fail.

In the multifocusPlugin folder do:
    - bash autogen.sh
    - make

# Install

- sudo make install

To test if the plugin has been correctly install, do:
    - export GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0/
    - gst-inspect-1.0 multifocus

If the plugin failed to install the following message will be displayed: "No such element or plugin 'multifocus'"

# Uninstall

- sudo rm /usr/local/lib/gstreamer-1.0/libgstmultifocus.*

# Usage

By default the plugin is installed in /usr/local/lib/gstreamer-1.0. 
It is then required to tell gstreamer where to find it with the command:
- export GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0/

Before using the plugin the Topaz2M must be set in compatible sensor mode : GRAY8 format ; Y10 isn't supported by the v4l2src

The plugin can be used in any gstreamer pipeline by adding "multifocus", the name of the plugin.

## Pipeline examples:

### Without NVIDIA plugins

Simple test :

	gst-launch-1.0 v4l2src ! multifocus ! queue ! videoconvert ! queue ! xvimagesink sync=false

Multifocus between 3 plans with PDA=0/200/400 :

	gst-launch-1.0 v4l2src ! multifocus plan1=0 plan2=200 plan3=400 auto-detect-plans=false ! queue ! videoconvert ! queue ! xvimagesink sync=false

Multifocus between 3 plans with PDA=0/200/400 with 5 frame between each switch of plans :

	gst-launch-1.0 v4l2src ! multifocus plan1=0 plan2=200 plan3=400 auto-detect-plans=false space-between-switch=5 ! queue ! videoconvert ! queue ! xvimagesink sync=false

### With NVIDIA plugins

Note : You should have update the nvvidconv plugin to support GRAY8, if not the image will be grayed out.

Simple test :

	gst-launch-1.0 v4l2src ! 'video/x-raw,width=1920,height=1080,format=GRAY8' ! multifocus ! nvvidconv ! 'video/x-raw(memory:NVMM),format=I420' ! nv3dsink sync=0

Multifocus between 3 plans with PDA=0/200/400 :

	gst-launch-1.0 v4l2src ! 'video/x-raw,width=1920,height=1080,format=GRAY8' ! multifocus plan1=0 plan2=200 plan3=400 auto-detect-plans=false ! autoexposure ! nvvidconv ! 'video/x-raw(memory:NVMM),format=I420' ! nv3dsink sync=0

Multifocus between 3 plans with PDA=0/200/400 with 5 frame between each switch of plans :

	gst-launch-1.0 v4l2src ! 'video/x-raw,width=1920,height=1080,format=GRAY8' ! multifocus plan1=0 plan2=200 plan3=400 auto-detect-plans=false space-between-switch=5 ! autoexposure ! nvvidconv ! 'video/x-raw(memory:NVMM),format=I420' ! nv3dsink sync=0


# Plugin parameters (gst-inspect-1.0 multifocus)

-  name                : "The name of the object
                        flags: readable, writable
                        String. Default: "multifocus0""

-  parent              : The parent of the object
                        flags: readable, writable
                        Object of type "GstObject"

-  work                : activate/desactivate plugin (usefull only for applications)
                        flags: readable, writable
                        Boolean. Default: true

-  latency             : Latency between command and command effect on gstreamer
                        flags: readable, writable
                        Integer. Range: 1 - 120 Default: 3 

-  number-of-plans     : Not implemented yet, please do not use
                        flags: readable, writable
                        Integer. Range: 1 - 50 Default: 4 

-  wait-after-start    : number of frames we are waiting before launching the multifocus
                        flags: readable, writable
                        Integer. Range: 1 - 120 Default: 15 

 - reset               : Reset the Multifocus plans (usefull only for applications)
                        flags: readable, writable
                        Boolean. Default: false

-  space-between-switch: number of images separating each PDA switch
                        flags: readable, writable
                        Integer. Range: 1 - 120 Default: 30 

-  roi1x               : Roi coordinates
                        flags: readable, writable
                        Integer. Range: 0 - 1920 Default: 0 

-  roi1y               : Roi coordinates
                        flags: readable, writable
                        Integer. Range: 0 - 1080 Default: 0 

-  roi2x               : Roi coordinates
                        flags: readable, writable
                        Integer. Range: 0 - 1920 Default: 1920 

-  roi2y               : Roi coordinates
                        flags: readable, writable
                        Integer. Range: 0 - 1080 Default: 1080 

-  auto-detect-plans   : auto detection of plans
                        flags: readable, writable
                        Boolean. Default: true

-  next                : Research of next plan (usefull only for applications)
                        flags: readable, writable
                        Boolean. Default: false

-  plan1               : Initialize focus plan 1 with PDA value
                        flags: readable, writable
                        Integer. Range: -90 - 700 Default: 0 

-  plan2               : Initialize focus plan 2 with PDA value
                        flags: readable, writable
                        Integer. Range: -90 - 700 Default: 0 

-  plan3               : Initialize focus plan 3 with PDA value
                        flags: readable, writable
                        Integer. Range: -90 - 700 Default: 0 


