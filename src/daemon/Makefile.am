
METASOURCES = AUTO
bin_PROGRAMS = networkclient clearTTY networkclient_login createnetworkshortcut \
			path_to_network_url syscall LNCtest lnc_url_to_ip


INCLUDES = -I$(top_srcdir)/src/libnetwork $(QT_INCLUDES) $(KDE_INCLUDES) \
	$(X_INCLUDES) -I/usr/include/fuse

networkclient_LDADD = $(top_builddir)/src/libnetwork/libnetwork.la -lelektra \
	-lfuse
networkclient_SOURCES = session.c fusefs.c daemon.c display.c
noinst_HEADERS = session.h fusefs.h daemon.h display.h

#networkclient_DATA = yast_samba_client.png yast_nfs.png nfs_mount.png network.png mycomputer.png fileshare.png share.directory host.directory domain.directory network.directory SMB.directory NFS.directory NCP.directory SSH.directory FTP.directory HTTP.directory
#networkclientdir = $(prefix)/share/apps/networkclient

#scripts_DATA = konqLoginAs konqRDC
#scriptsdir = $(prefix)/bin

#udev_DATA = fuse.rules
#udevdir = /etc/udev/rules.d

#networkclient_sh_DATA = networkclient.sh
#networkclient_shdir = /etc/rc.d/init.d

clearTTY_SOURCES = clearTTY.c

install-exec-hook:
	-chown root $(prefix)/bin/clearTTY
	-chmod u+s $(prefix)/bin/clearTTY
	-cp networkclient.sh /etc/rc.d/init.d/networkclient
	-chmod 0755 /etc/rc.d/init.d/networkclient
	-chkconfig --add networkclient
	-cp konqLoginAs $(prefix)/bin/konqLoginAs
	-cp konqRDC $(prefix)/bin/konqRDC
	-chmod 0777 $(prefix)/bin/konqLoginAs
	-chmod 0777 $(prefix)/bin/konqRDC
#	-chown root $(DESTDIR)$(bindir)/networkclient
#	-chmod u+s $(DESTDIR)$(bindir)/networkclient

uninstall-hook:
	-chkconfig --del networkclient
	-rm -f /etc/rc.d/init.d/networkclient
	-rm -f /$(prefix)/bin/konqLoginAs
	-rm -f /$(prefix)/bin/konqRDC

networkclient_login_SOURCES = networkclient_login.c



createnetworkshortcut_SOURCES = createnetworkshortcut.c
createnetworkshortcut_LDADD = $(top_builddir)/src/libnetwork/libnetwork.la \
	-lelektra
path_to_network_url_SOURCES = main.c

path_to_network_url_LDADD = $(top_builddir)/src/libnetwork/libnetwork.la -ldl \
	-lelektra
syscall_SOURCES = syscall.c
LNCtest_SOURCES = LNCtest.c

lnc_url_to_ip_SOURCES = lnc_url_to_ip.c
lnc_url_to_ip_LDADD = $(top_builddir)/src/libnetwork/libnetwork.la
syscall_LDADD = $(top_builddir)/src/libnetwork/libnetwork.la -lfuse
networkclient_LDFLAGS = -rdynamic
