if !AMM_ENABLE_CURSES
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_gui_plugin_curses.la
endif

lib_gui_plugin_curses_la_SOURCES = curses.c cursesdialog.c cursesdialog.h
AM_CFLAGS = 
AM_CXXFLAGS = 
AM_FFLAGS = 

libdir = $(prefix)/lib/LNC
INCLUDES = -I$(top_srcdir)/src/libnetwork
lib_gui_plugin_curses_la_LIBADD = -lcdk -lm -lncurses -lslang -lpthread
lib_gui_plugin_curses_la_LDFLAGS = -module
