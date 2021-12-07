if !AMM_ENABLE_CRASHMANAGER
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_global_plugin_crash.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork -I$(top_srcdir)/src/test
METASOURCES = AUTO
lib_global_plugin_crash_la_LDFLAGS = -module

lib_global_plugin_crash_la_SOURCES = crashmanager.c
libdir = $(prefix)/lib/LNC
lib_global_plugin_crash_la_LIBADD = -lpthread
