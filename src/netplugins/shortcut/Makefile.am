if !AMM_ENABLE_SHORTCUT
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_network_plugin_shortcut.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork
METASOURCES = AUTO

lib_network_plugin_shortcut_la_SOURCES = shortcut.c
libdir = $(prefix)/lib/LNC
lib_network_plugin_shortcut_la_LDFLAGS = -module
lib_network_plugin_shortcut_la_LIBADD = -lpthread
