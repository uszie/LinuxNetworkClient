if !AMM_ENABLE_XEXT
    lib_LTLIBRARIES =
else
    lib_LTLIBRARIES = lib_global_plugin_Xext.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork -I$(X_INCLUDES)
METASOURCES = AUTO
lib_global_plugin_Xext_la_LDFLAGS = -module $(X_LDFLAGS)
lib_global_plugin_Xext_la_SOURCES = xext.c
libdir = $(prefix)/lib/LNC
lib_global_plugin_Xext_la_LIBADD = -lpthread $(X_LIBS)
