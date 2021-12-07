if !AMM_ENABLE_GNOME
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_gui_plugin_gnome.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork $(GNOME_INCLUDES)
METASOURCES = AUTO
lib_gui_plugin_gnome_la_LDFLAGS = -module $(GNOME_LDFLAGS)
lib_gui_plugin_gnome_la_SOURCES = gnome.c
libdir = $(prefix)/lib/LNC
lib_gui_plugin_gnome_la_LIBADD = -lpthread $(GNOME_LIBS)
