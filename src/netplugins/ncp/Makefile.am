if !AMM_ENABLE_NCP
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_network_plugin_ncp.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork
METASOURCES = AUTO

lib_network_plugin_ncp_la_SOURCES = ncp.c
lib_network_plugin_ncp_la_LIBADD = -lpthread -lncp
libdir = $(prefix)/lib/LNC
lib_network_plugin_ncp_la_LDFLAGS = -module
