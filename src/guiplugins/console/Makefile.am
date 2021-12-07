if !AMM_ENABLE_CONSOLE
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_gui_plugin_console.la
endif

lib_gui_plugin_console_la_SOURCES =  console.c
libdir = $(prefix)/lib/LNC
INCLUDES = -I$(top_srcdir)/src/libnetwork

lib_gui_plugin_console_la_LDFLAGS = -module
lib_gui_plugin_console_la_LIBADD = -lpthread
