if !AMM_ENABLE_SSH
lib_LTLIBRARIES =
else
lib_LTLIBRARIES = lib_network_plugin_ssh.la
endif

INCLUDES = -I$(top_srcdir)/src/libnetwork
METASOURCES = AUTO
lib_network_plugin_ssh_la_LDFLAGS = -module
lib_network_plugin_ssh_la_SOURCES = ssh.c
libdir = $(prefix)/lib/LNC
bin_PROGRAMS = mount.sshfs
mount_sshfs_SOURCES = mount.sshfs.c
bindir=/sbin
mount_sshfs_LDADD = -lutil
lib_network_plugin_ssh_la_LIBADD = -lpthread
