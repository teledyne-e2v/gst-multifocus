# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstmultifocus.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

## Plugin 1

# sources used to compile this plug-in
libgstmultifocus_la_SOURCES = gstmultifocus.c gstmultifocus.h multifocusControl.c multifocusControl.h i2c_control.c i2c_control.h i2c.c i2c.h logger.c logger.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstmultifocus_la_CFLAGS = $(GST_CFLAGS) -Wall -Wextra -Wpedantic -O3
libgstmultifocus_la_LIBADD = $(GST_LIBS)
libgstmultifocus_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstmultifocus_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstmultifocus.h
