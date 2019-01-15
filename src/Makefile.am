# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstdektec.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

#liggstdektec_la_CPPFLAGS = $(GST_CXXFLAGS)
DEKTEC_INCLUDES = -I../DTAPI

DEKTEC_CXXFLAGS = -g -O2 -I$(INCLUDEDIR) -fexceptions \
	   -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
	   
DEKTEC_LIBCLASSES = ../DTAPI/DTAPI64.o
		
# sources used to compile this plug-in
libgstdektec_la_SOURCES =  gstdta2152src.cpp gstdta2152sink.cpp gstdta2160src.cpp gstdta2160sink.cpp

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstdektec_la_CXXFLAGS = $(GST_CFLAGS) $(DEKTEC_INCLUDES) $(DEKTEC_CXXFLAGS)
libgstdektec_la_LIBADD = $(GST_LIBS) $(DEKTEC_LIBCLASSES)

libgstdektec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdektec_la_LIBTOOLFLAGS = --tag=disable-static
libgstdektec_la_CXXFLAGS += -g

# headers we need but don't want installed
noinst_HEADERS = gstdta2152src.h gstdta2152sink.h gstdta2160src.h gstdta2160sink.h
