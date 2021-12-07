if !AMM_ENABLE_KONQUEROR
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_global_plugin_konqext.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork -I$(top_srcdir)/src/test \
	$(KDE_INCLUDES) $(QT_INCLUDES)
METASOURCES = AUTO
lib_global_plugin_konqext_la_SOURCES = konqext.c konq_dcop.cpp
libdir = $(prefix)/lib/LNC
lib_global_plugin_konqext_la_LDFLAGS = -module $(KDE_LDFLAGS)
noinst_HEADERS = konq_dcop.h
lib_global_plugin_konqext_la_LIBADD = -lpthread $(KIO_LIBS) -lkdecore

if AMM_ENABLE_KONQUEROR
networkclient_DATA = yast_samba_client.png yast_nfs.png nfs_mount.png network.png mycomputer.png fileshare.png share.directory host.directory domain.directory network.directory SMB.directory NFS.directory NCP.directory SSH.directory FTP.directory HTTP.directory
networkclientdir = $(prefix)/share/apps/networkclient

install-exec-hook:
	cp konqLoginAs $(prefix)/bin/konqLoginAs
	cp konqRDC $(prefix)/bin/konqRDC
	chmod 0777 $(prefix)/bin/konqLoginAs
	chmod 0777 $(prefix)/bin/konqRDC

uninstall-hook:
	rm -f /$(prefix)/bin/konqLoginAs
	rm -f /$(prefix)/bin/konqRDC
else
endif