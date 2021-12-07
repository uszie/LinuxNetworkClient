if !AMM_ENABLE_FTP
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_network_plugin_ftp.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork
METASOURCES = AUTO
libdir = $(prefix)/lib/LNC
lib_network_plugin_ftp_la_LDFLAGS = -module
lib_network_plugin_ftp_la_SOURCES = ftp.c
lib_network_plugin_ftp_la_LIBADD = -lpthread
