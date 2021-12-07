if !AMM_ENABLE_GTK
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_gui_plugin_gtk.la
endif

lib_gui_plugin_gtk_la_SOURCES =  gtkdialog.c gtkdialog.h gtk.c


INCLUDES = -I$(top_srcdir)/src/libnetwork $(GTK_INCLUDES)
libdir = $(prefix)/lib/LNC

gtk_DATA = keys.png
gtkdir = $(prefix)/share/apps/networkclient

AM_CFLAGS = 
AM_CXXFLAGS = 
AM_FFLAGS = 

lib_gui_plugin_gtk_la_LIBADD = -lpthread $(GTK_LIBS)
lib_gui_plugin_gtk_la_LDFLAGS = -module $(GTK_LDFLAGS)
