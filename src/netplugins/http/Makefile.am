if !AMM_ENABLE_HTTP
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_network_plugin_http.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork
METASOURCES = AUTO
lib_network_plugin_http_la_LDFLAGS = -module
lib_network_plugin_http_la_SOURCES = http.c
libdir = $(prefix)/lib/LNC
lib_network_plugin_http_la_LIBADD = -lpthread
