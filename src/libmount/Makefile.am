
#
# This is temporary Makefile! The final version will be based on libtool2
# (see shlibs/blkid/src/ stuff in the master branch).
#

# includes
mountincdir = $(includedir)/mount
mountinc_HEADERS = mount.h

# static library
libmount_a_CFLAGS = -fPIC

mount_IMAGE = libmount.so
mount_LDSCRIPT = $(srcdir)/mount.sym
mount_LIB = $(mount_IMAGE).1.0.0
mount_SONAME = $(mount_IMAGE).0
mount_LINKS = $(mount_IMAGE) $(mount_SONAME)
mount_OTHERLDADD = $(BLKID_LIBS)

EXTRA_DIST = mount.sym
CLEANFILES = $(mount_LIB) $(mount_LINKS)

all-local: $(mount_LIB)

$(mount_LIB): $(lib_LIBRARIES) $(mount_LDSCRIPT)
	$(CC) --shared -o $(mount_LIB) $(AM_LDFLAGS) \
              -Wl,-soname,$(mount_SONAME),--version-script,$(mount_LDSCRIPT) \
	      $(libmount_a_OBJECTS) $(mount_OTHERLDADD)
	for I in $(mount_LINKS); do \
		ln -sf $(mount_LIB) $$I; \
	done

installdirs-local:
	$(MKDIR_P) $(DESTDIR)$(libdir)

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(mountincdir)
	$(INSTALL_PROGRAM) $(mount_LIB) $(DESTDIR)$(libdir)
	for I in $(mount_LINKS); do \
		cd $(DESTDIR)$(libdir) && ln -sf $(mount_LIB) $$I; \
	done

uninstall-hook:
	rm -f $(DESTDIR)$(libdir)/$(mount_LIB)
	for I in $(mount_LINKS); do \
		cd $(DESTDIR)$(libdir) && rm -f $(mount_LIB) $$I; \
	done

