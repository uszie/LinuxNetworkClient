
METASOURCES = AUTO
lib_LTLIBRARIES =  libnetwork.la
libnetwork_la_SOURCES = plugin.c list.c util.c nconfig.c user.c dir.c \
		networkclient.c pluginhelper.c syscall.c
#libnetwork_la_LIBADD = /usr/lib/libfuse.a
#noinst_HEADERS = plugin.h list.h util.h nconfig.h user.h dir.h pluginhelper.h

LNCincludedir = $(includedir)/LNC
LNCinclude_HEADERS = networkclient.h plugin.h list.h util.h nconfig.h user.h dir.h pluginhelper.h

bin_PROGRAMS = networkmount networkumount netscan
networkmount_SOURCES =  mount.c

#bin_PROGRAMS =  networkumount
networkumount_SOURCES =  umount.c

install-exec-hook:
	-chown root $(DESTDIR)$(bindir)/networkmount
	-chmod u+s $(DESTDIR)$(bindir)/networkmount
	-chown root $(DESTDIR)$(bindir)/networkumount
	-chmod u+s $(DESTDIR)$(bindir)/networkumount
	-chown root $(DESTDIR)$(bindir)/netscan
	-chmod u+s $(DESTDIR)$(bindir)/netscan

#.PHONY: install
#install:
#	echo "installing mount scripts"
#	cp ./networkmount @prefix@/bin/
#	cp ./networkumount @prefix@/bin/
#	chown root:root @prefix@/bin/networkmount
#	chown root:root @prefix@/bin/networkumount
#	chmod 04755 @prefix@/bin/networkmount
#	chmod 04755 @prefix@/bin/networkumount
#			
#	cp ./.libs/libnetwork.so.0.0.0 @prefix@/lib/
#	cp ./.libs/libnetwork.a @prefix@/lib/

noinst_HEADERS = syscall.h
netscan_SOURCES = netscan.c

libnetwork_la_LIBADD = -lelektra -lfam -ldl -lpthread
