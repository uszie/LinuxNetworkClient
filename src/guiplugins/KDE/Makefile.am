if !AMM_ENABLE_KDE
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_gui_plugin_kde.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork $(KDE_INCLUDES) $(QT_INCLUDES)
METASOURCES = AUTO

lib_gui_plugin_kde_la_SOURCES = kde.cpp
lib_gui_plugin_kde_la_LIBADD = -lpthread -lkdecore $(KIO_LIBS)
lib_gui_plugin_kde_la_LDFLAGS = -module $(KDE_LDFLAGS)
libdir = $(prefix)/lib/LNC
AM_CFLAGS = -nostartfiles -nostdlib
