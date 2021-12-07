if !AMM_ENABLE_NFS
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_network_plugin_nfs.la
endif

lib_network_plugin_nfs_la_SOURCES = nfs.c mount_xdr.c
libdir = $(prefix)/lib/LNC
INCLUDES = -I$(top_srcdir)/src/libnetwork

lib_network_plugin_nfs_la_LDFLAGS = -module
lib_network_plugin_nfs_la_LIBADD = -lpthread
