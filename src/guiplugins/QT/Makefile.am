if !AMM_ENABLE_QT
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_gui_plugin_qt.la
endif

lib_gui_plugin_qt_la_SOURCES =  qt.cpp qtdialog.cpp qtdialog.h


INCLUDES = -I$(top_srcdir)/src/libnetwork $(QT_INCLUDES)
libdir = $(prefix)/lib/LNC

qt_DATA = keys.png
qtdir = $(prefix)/share/apps/networkclient

AM_CFLAGS = 
AM_CXXFLAGS = 
AM_FFLAGS = 

lib_gui_plugin_qt_la_LIBADD = -lpthread $(QT_LIBS)

lib_gui_plugin_qt_la_LDFLAGS = -module $(QT_LDFLAGS)
