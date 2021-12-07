dnl
dnl CONFIGURE_QT([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl \author J.E. Hoffmann <je-h@gmx.net>
dnl

#have_x=no
#have_gnome=no
#have_gtk=no
#have_qt=no
#have_kde=no
#have_pkgconfig=no
#have_nmblookup=no
#have_smbclient=no
#have_smbmount=no
#have_curses=no
#have_smb=no
#have_nfs=no
#have_ncp=no

X_INCLUDES=""
X_LDFLAGS=""
X_LIBS=""
QT_INCLUDES=""
QT_LDFLAGS=""
QT_LIBS=""
KDE_INCLUDES=""
KDE_LDFLAGS=""
KIO_LIBS=""
GTK_INCLUDES=""
GTK_LDFLAGS=""
GTK_LIBS=""
GNOME_INCLUDES=""
GNOME_LDFLAGS=""
GNOME_LIBS=""


AC_SUBST(X_INCLUDES)
AC_SUBST(X_LDFLAGS)
AC_SUBST(X_LIBS)
AC_SUBST(QT_INCLUDES)
AC_SUBST(QT_LDFLAGS)
AC_SUBST(QT_LIBS)
AC_SUBST(KDE_INCLUDES)
AC_SUBST(KDE_LDFLAGS)
AC_SUBST(KIO_LIBS)
AC_SUBST(GTK_INCLUDES)
AC_SUBST(GTK_LDFLAGS)
AC_SUBST(GTK_LIBS)
AC_SUBST(GNOME_INCLUDES)
AC_SUBST(GNOME_LDFLAGS)
AC_SUBST(GNOME_LIBS)


AC_DEFUN([RUN_TEST],
[
    ac_popdir=`pwd`
    cd ./src/knetworkclientconfig
    if test x$with_kde_prefix = xyes || test x$with_kde_prefix = xno || test x$with_kde_prefix = x; then
	if test x$prefix = xNONE || test x$prefix = x; then
	    with_kde_prefix=/usr/local
	else
	    with_kde_prefix=$prefix
	fi
    fi
    /bin/sh ./configure --prefix=$with_kde_prefix --srcdir=. --cache-file=/dev/null
    cd ../../
])

AC_DEFUN([CONFIGURE_KDE4_PLUGIN],
[
    cd ./src/guiplugins/KDE4
    cmake -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=$(kde4-config --prefix) -DLNC_PLUGIN_DIR=${prefix}/lib/LNC
    prepare_linker_opts
    cd ../../../
])


AC_DEFUN([AUTONET_SUMMARY],
[
#    echo X_INCLUDES ${X_INCLUDES}
#    echo X_LDFLAGS ${X_LDFLAGS}
#    echo X_LIBS ${X_LIBS}
#    echo QT_INCLUDES ${QT_INCLUDES}
#    echo QT_LDFLAGS ${QT_LDFLAGS}
#    echo QT_LIBS ${QT_LIBS}
#    echo KDE_INCLUDES ${KDE_INCLUDES}
#    echo KDE_LDFLAGS ${KDE_LDFLAGS}
#    echo KIO_LIBS ${KIO_LIBS}
#    echo GTK_INCLUDES ${GTK_INCLUDE}
#    echo GTK_LDFLAGS ${GTK_LDFLAGS}
#    echo GTK_LIBS ${GTK_LIBS}

    echo ""
    
    if test x$have_x = xno; then
	AC_MSG_WARN([Could not find the xlib-devel files, check your installation.])
    fi

    if test x$have_qt = xno; then
        AC_MSG_WARN([Could not find the qt-devel files, check your installation.])
    fi

    if test x$have_kde = xno; then
        AC_MSG_WARN([Could not find the kde-devel files, check your installation.])
    fi

    if test x$have_kde4 = xno; then
        AC_MSG_WARN([Could not find the kde4-devel files, check your installation.])
    fi

    if test x$have_gtk = xno; then
        AC_MSG_WARN([Could not find the gtk-devel files, check your installation.])
    fi
    
    if test x$have_gnome = xno; then
        AC_MSG_WARN([Could not find the gnome-devel files, check your installation.])
    fi
		
    if test x$have_curses = xno; then
        AC_MSG_WARN([Could not find the curses-devel files, check your installation.])
    fi
    
    if test x$have_cdk = xno; then
        AC_MSG_WARN([Could not find the cdk-devel files, check your installation.])
    fi
		
    if test x$have_pkgconfig = xno; then
        AC_MSG_WARN([Could not find the pkg-config binary, check your installation.])
    fi

    if test x$have_nfs = xno; then
        AC_MSG_WARN([Could not find the nfs-devel files, check your installation.])
    fi

    if test x$have_ncp = xno; then
        AC_MSG_WARN([Could not find the ncp-devel files, check your installation.])
    fi

    if test x$have_ncpmount = xno; then
        AC_MSG_WARN([Could not find the ncp mount binary (mount.ncpfs), check your installation.])
    fi
		
    if test x$have_smb = xno; then
        AC_MSG_WARN([Could not find some or all of the samba client binaries (nmblookup,smbmount,smbclient), check your installation.])
    fi
    
    if test x$have_sshfs = xno; then
        AC_MSG_WARN([Could not find the sshfs binary, check your installation.])
    fi

    if test x$have_ftpfs = xno; then
        AC_MSG_WARN([Could not find the curlftpfs binary, check your installation.])
    fi

    if test x$have_elektra = xno; then
        AC_MSG_ERROR([Could not find the elektra libraries or devel files, check your installation.])
    fi
		        
    
    AC_MSG_RESULT([
    Linux Network build configuration,

GUI Plugins
    Build GNOME plugin:		$enable_gnome_plugin
    Build GTK plugin:		$enable_gtk_plugin
    Build KDE plugin:		$enable_kde_plugin
    Build QT plugin:		$enable_qt_plugin
    Build Console plugin:	$enable_console_plugin
    Build Curses plugin		$enable_curses_plugin

Network Plugins
    Build SMB  plugin:		$enable_smb_plugin
    Build NFS  plugin:		$enable_nfs_plugin
    Build NCP  plugin:		$enable_ncp_plugin
    Build SSH  plugin:		$enable_ssh_plugin
    Build FTP  plugin:		$enable_ftp_plugin
    Build HTTP plugin:		$enable_http_plugin
    Build shortcut plugin:	$enable_shortcut_plugin

Miscellaneous Plugins
    Build Konqueror plugin:	$enable_konqueror_plugin
    Build Crash manager:	$enable_crashmanager_plugin
    Build X extensions plugin:	$enable_Xextensions_plugin 

Applications
    Build LNC library:		$build_lnc_library
    Build Network client:	$enable_lncd    
    Build KNetworkclientConfig:	$enable_knetworkclientconfig
    ])
])

AC_DEFUN([CHECK_X_QT_KDE],
[
    if test -f .flags; then
        KDE_INCLUDES=`cat .flags | grep KDE_INCLUDES | awk '{print $[3]}'`
        KDE_LDFLAGS=`cat .flags | grep KDE_LDFLAGS | awk '{print $[3]}'`
	KIO_LIBS=`cat .flags | grep KIO_LIBS | awk '{print $[3]}'`
	have_kde=`cat .flags | grep have_kde | awk '{print $[3]}'`
        if test x$have_kde = x; then
	    have_kde=no
        fi
    
	QT_INCLUDES=`cat .flags | grep QT_INCLUDES | awk '{print $[3]}'`
        QT_LDFLAGS=`cat .flags | grep QT_LDFLAGS | awk '{print $[3]}'`
        QT_LIBS=`cat .flags | grep QT_LIBS | awk '{print $[3]}'`
	have_qt=`cat .flags | grep have_qt | awk '{print $[3]}'`
        if test x$have_qt = x; then
	    have_qt=no
        fi

        X_INCLUDES=`cat .flags | grep X_INCLUDES | awk '{print $[3]}'`
        X_LDFLAGS=`cat .flags | grep X_LDFLAGS | awk '{print $[3]}'`
	X_LIBS=`cat .flags | grep X_LIBS | awk '{print $[3]}'`
        have_x=`cat .flags | grep have_x | awk '{print $[3]}'`
        if test x$have_x = x; then
	    have_x=no
        fi
	rm -f .flags
    else
	have_x=no
	have_qt=no
	have_kde=no
    fi
])

AC_DEFUN([KDE_PREFIX],
[
    AC_ARG_ENABLE(kde_prefix, [  --with-kde-prefix=PREFIX	install kde programs in PREFIX. [/usr/local]], with_kde_prefix=$enableval, with_kde_prefix=/usr/local)
])

AC_DEFUN([BUILD_LNC_LIBRARY],
[
    have_elektra=yes
    build_lnc_library=yes
    AC_CHECK_HEADER(kdb.h, , have_elektra=no, )
    AC_CHECK_LIB(elektra, kdbOpen, have_elektra=${have_elektra}, have_elektra=no, -ldl)
    if test x$have_elektra = xno; then
	build_lnc_library=no
    fi
])
					
AC_DEFUN([ENABLE_LNCD],
[
    AC_ARG_ENABLE(lncd, [  --enable-lncd    build LNC network client application. [default=yes]], enable_lncd=$enableval, enable_lncd=yes)
    if test x$enable_lncd = xyes; then
	have_fuse=yes
	AC_CHECK_LIB(fuse, fuse_mount, have_fuse=${have_fuse}, have_fuse=no)
	AC_CHECK_HEADER(fuse/fuse.h, , have_fuse=no, )
        if test x$have_fuse = xno; then
            enable_lncd=no
        fi
    fi
])


AC_DEFUN([ENABLE_KAUTONETCONFIG],
[
    AC_ARG_ENABLE(knetworkclientconfig, [  --enable-knetworkclientconfig	build KDE configuration application. [default=yes]], enable_knetworkclientconfig=$enableval, enable_knetworkclientconfig=yes)
    if test x$enable_knetworkclientconfig = xyes; then
        if test x$have_x = xno || test x$have_qt = xno || test x$have_kde = xno; then
            enable_knetworkclientconfig=no
        fi
    fi
])

AC_DEFUN([ENABLE_GNOME_PLUGIN],
[
    AC_ARG_ENABLE(gnome_plugin, [  --enable-gnome-plugin            build Gnome dialog plugin. [default=yes]], enable_gnome_plugin=$enableval, enable_gnome_plugin=yes)

    if test x$enable_gnome_plugin = xyes; then
        AC_CHECK_PROG(have_pkgconfig, pkg-config, yes, no)
        if test x$have_pkgconfig = xyes && test -n "`pkg-config --list-all 2>/dev/null | grep "libgnomeui-2.0"`"; then
            have_gnome=yes
            GNOME_INCLUDES=`pkg-config --cflags-only-I libgnomeui-2.0 2>/dev/null`
            GNOME_LDFLAGS=`pkg-config --cflags-only-other libgnomeui-2.0``pkg-config --libs-only-L libgnomeui-2.0``pkg-config --libs-only-other libgnomeui-2.0 2>/dev/null`
            GNOME_LIBS=`pkg-config --libs-only-l libgnomeui-2.0 2>/dev/null`
        else
	    have_gnome=no
	fi

        if test x$have_pkgconfig = xyes && test x$have_x = xyes && test x$have_gnome = xyes; then
            enable_gnome_plugin=yes
        else
            enable_gnome_plugin=no
        fi
    fi
])

AC_DEFUN([ENABLE_GTK_PLUGIN],
[
    AC_ARG_ENABLE(gtk_plugin, [  --enable-gtk-plugin		build GTK dialog plugin. [default=yes]], enable_gtk_plugin=$enableval, enable_gtk_plugin=yes)

    if test x$enable_gtk_plugin = xyes; then
        AC_CHECK_PROG(have_pkgconfig, pkg-config, yes, no)
        if test x$have_pkgconfig = xyes && test -n "`pkg-config --list-all 2>/dev/null | grep "gtk+-2.0 "`"; then
    	    have_gtk=yes
	    GTK_INCLUDES=`pkg-config --cflags-only-I gtk+-2.0 2>/dev/null`
	    GTK_LDFLAGS=`pkg-config --cflags-only-other gtk+-2.0``pkg-config --libs-only-L gtk+-2.0``pkg-config --libs-only-other gtk+-2.0 2>/dev/null`
	    GTK_LIBS=`pkg-config --libs-only-l gtk+-2.0 2>/dev/null`
#        elif test x$have_pkgconfig = xyes && test -n "`pkg-config --list-all | grep "gtk+ "`"; then
#	    have_gtk=yes
#            GTK_INCLUDES=`pkg-config --cflags-only-I gtk+`
#            GTK_LDFLAGS=`pkg-config --cflags-only-other gtk+``pkg-config --libs-only-L gtk+``pkg-config --libs-only-other gtk+`
#            GTK_LIBS=`pkg-config --libs-only-l gtk+`

	else
	    have_gtk=no
	fi

	if test x$have_pkgconfig = xyes && test x$have_x = xyes && test x$have_gtk = xyes; then
	    enable_gtk_plugin=yes
	else
	    enable_gtk_plugin=no
	fi
    fi
])

AC_DEFUN([ENABLE_QT_PLUGIN],
[
    AC_ARG_ENABLE(qt_plugin, [  --enable-qt-plugin		build QT dialog plugin. [default=yes]], enable_qt_plugin=$enableval, enable_qt_plugin=yes)

    if test x$enable_qt_plugin = xyes; then
	if test x$have_x = xno || test x$have_qt = xno; then
	    enable_qt_plugin=no
	fi
    fi
])

AC_DEFUN([ENABLE_KDE_PLUGIN],
[
    AC_ARG_ENABLE(kde_plugin, [  --enable-kde-plugin		build KDE dialog plugin. [default=yes]], enable_kde_plugin=$enableval, enable_kde_plugin=yes)

    if test x$enable_kde_plugin = xyes; then
        if test x$have_x = xno || test x$have_qt = xno || test x$have_kde = xno; then
            enable_kde_plugin=no
        fi
    fi
])

AC_DEFUN([ENABLE_CURSES_PLUGIN],
[
    AC_ARG_ENABLE(curses_plugin, [  --enable-curses-plugin	build curses dialog plugin. [default=yes]], enable_curses_plugin=$enableval, enable_curses_plugin=yes)

    if test x$enable_curses_plugin = xyes; then
	have_curses=yes
	have_cdk=yes
        AC_CHECK_HEADER(cdk/cdk.h, , have_cdk=no, )
	AC_CHECK_HEADER(curses.h, , have_curses=no, )
	if test x$have_curses = xno || test x$have_cdk = xno; then
	    enable_curses_plugin=no
	fi
    fi
])

AC_DEFUN([ENABLE_CONSOLE_PLUGIN],
[
    AC_ARG_ENABLE(console_plugin, [  --enable-console-plugin	build console dialog plugin. [default=yes]], enable_console_plugin=$enableval, enable_console_plugin=yes)
])

AC_DEFUN([ENABLE_SMB_PLUGIN],
[
    AC_ARG_ENABLE(smb_plugin, [  --enable-smb-plugin		build SMB network plugin. [default=yes]], enable_smb_plugin=$enableval, enable_smb_plugin=yes)

    if test x$enable_smb_plugin = xyes; then
	have_smb=yes
	AC_CHECK_PROG(have_nmblookup, nmblookup, yes, no)
	AC_CHECK_PROG(have_smbclient, smbclient, yes, no)
#    	AC_CHECK_PROG(have_smbmount, smbmount, yes, no)
	if test x$have_nmblookup = xno || test x$have_smbclient = xno;then ### || test x$have_smbmount = xno; then
	    enable_smb_plugin=no
	    have_smb=no
	fi
    fi
])

AC_DEFUN([ENABLE_NFS_PLUGIN],
[
    AC_ARG_ENABLE(nfs_plugin, [  --enable-nfs-plugin		build NFS network plugin. [default=yes]], enable_nfs_plugin=$enableval, enable_nfs_plugin=yes)

    if test x$enable_nfs_plugin = xyes; then
    	have_nfs=yes
	AC_CHECK_HEADER(rpc/rpc.h, , have_nfs=no, )
        AC_CHECK_HEADER(rpc/pmap_clnt.h, , have_nfs=no, )
	AC_CHECK_HEADER(rpcsvc/mount.h, , have_nfs=no, )
        AC_CHECK_HEADER(rpcsvc/rstat.h, , have_nfs=no, )
        AC_CHECK_HEADER(netdb.h, , have_nfs=no, )
        AC_CHECK_HEADER(sys/socket.h, , have_nfs=no, )
        AC_CHECK_HEADER(netinet/in.h, , have_nfs=no, )
        AC_CHECK_HEADER(arpa/inet.h, , have_nfs=no, )
	if test x$have_nfs = xno; then
	    enable_nfs_plugin=no
	fi
    fi
])

AC_DEFUN([ENABLE_NCP_PLUGIN],
[
    AC_ARG_ENABLE(ncp_plugin, [  --enable-ncp-plugin		build NCP network plugin. [default=yes]], enable_ncp_plugin=$enableval, enable_ncp_plugin=yes)

    if test x$enable_ncp_plugin = xyes; then
        have_ncp=yes
        AC_CHECK_HEADER(ncp/ncplib.h, , have_ncp=no, )
        AC_CHECK_HEADER(ncp/nwcalls.h, , have_ncp=no, )
        AC_CHECK_HEADER(ncp/nwnet.h, , have_ncp=no, )
#	AC_CHECK_PROG(have_mount_ncpfs, mount.ncpfs, yes, no, /bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin)
#	AC_CHECK_PROG(have_ncpmount, ncpmount, yes, no, /bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin)
	AC_CHECK_PROG(have_ncpmount, ncpmount, yes, no)
#	AC_CHECK_PROG(have_mount_ncpfs, mount.ncpfs, yes, no)

	AC_CHECK_LIB(ncp, NWCallsInit, have_ncp=${have_ncp}, have_ncp=no, )
	if test x$have_ncp = xno || ( test x$have_ncpmount = xno && test x$have_mount_ncpfs = xno ); then
            enable_ncp_plugin=no
        fi
    fi
])

AC_DEFUN([ENABLE_SSH_PLUGIN],
[
    AC_ARG_ENABLE(ssh_plugin, [  --enable-ssh-plugin		build SSH network plugin. [default=yes]], enable_ssh_plugin=$enableval, enable_ssh_plugin=yes)

    if test x$enable_ssh_plugin = xyes; then
        AC_CHECK_PROG(have_sshfs, sshfs, yes, no)
        if test x$have_sshfs = xno; then
            enable_ssh_plugin=no
        fi
    fi
])

AC_DEFUN([ENABLE_FTP_PLUGIN],
[
    AC_ARG_ENABLE(ftp_plugin, [  --enable-ftp-plugin            build FTP network plugin. [default=yes]], enable_ftp_plugin=$enableval, enable_ftp_plugin=yes)

    if test x$enable_ftp_plugin = xyes; then
        AC_CHECK_PROG(have_ftpfs, curlftpfs, yes, no)
        if test x$have_ftpfs = xno; then
            enable_ftp_plugin=no
        fi
    fi
])

AC_DEFUN([ENABLE_SHORTCUT_PLUGIN],
[
    AC_ARG_ENABLE(shortcut_plugin, [  --enable-shortcut-plugin	build shortcut network plugin. [default=yes]], enable_shortcut_plugin=$enableval, enable_shortcut_plugin=yes)
])

AC_DEFUN([ENABLE_HTTP_PLUGIN],
[
    AC_ARG_ENABLE(http_plugin, [  --enable-http-plugin<>build HTTP network plugin. [default=yes]], enable_http_plugin=$enableval, enable_http_plugin=yes)
])

AC_DEFUN([ENABLE_CRASHMANAGER_PLUGIN],
[
    AC_ARG_ENABLE(crashmanager_plugin, [  --enable-crashmanager-plugin  build crash manager plugin. [default=yes]], enable_crashmanager_plugin=$enableval, enable_crashmanager_plugin=yes)
])
    
AC_DEFUN([ENABLE_KONQUEROR_PLUGIN],
[
    AC_ARG_ENABLE(konqueror_plugin, [  --enable-konqueror-plugin  build konqueror plugin. [default=yes]], enable_konqueror_plugin=$enableval, enable_konqueror_plugin=yes)

    if test x$enable_konqueror_plugin = xyes; then
        if test x$have_x = xno || test x$have_qt = xno || test x$have_kde = xno; then
            enable_konqueror_plugin=no
        fi
    fi
])

AC_DEFUN([ENABLE_XEXT_PLUGIN],
[
    AC_ARG_ENABLE(Xextensions_plugin, [  --enable-Xextensions-plugin  build X extensions plugin. [default=yes]], enable_Xextensions_plugin=$enableval, enable_Xextensions_plugin=yes)

    if test x$enable_Xextensions_plugin = xyes; then
        if test x$have_x = xno; then
            enable_Xextensions_plugin=no
        fi
    fi
])

dnl ------------------------------------------------------------------------
dnl Find a file (or one of more files in a list of dirs)
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN([AC_FIND_FILE],
[
$3=NO
for i in $2;
do
  for j in $1;
  do
    if test -r "$i/$j"; then
      $3=$i
      break 2
    fi
  done
done
])

AC_DEFUN([QT_MOC_ERROR_MESSAGE],
[
    AC_MSG_ERROR([No valid Qt meta object compiler (moc) found!
Please check whether you installed Qt correctly.
You need to have a running moc binary.
configure tried to run $ac_cv_path_moc and the test did not
succeed. If configure should not have tried this one, set
the environment variable MOC to the right one before running
configure.])
])

AC_DEFUN([QT_UIC_ERROR_MESSAGE],
[
    AC_MSG_WARN([No valid Qt ui compiler (uic) found!
Please check whether you installed Qt correctly.
You need to have a running uic binary.
configure tried to run $ac_cv_path_uic and the test did not
succeed. If configure should not have tried this one, set
the environment variable UIC to the right one before running
configure.])
])

dnl ------------------------------------------------------------------------
dnl Find the meta object compiler in the PATH, in $QTDIR/bin, and some
dnl more usual places
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN([AC_PATH_QT_MOC_UIC],
[
dnl usually qt is installed in such a way it might help to
dnl try this binpath first
    cutlib_binpath=`echo "$ac_qt_libraries" | sed 's%/[[^/]]*$%%'`
    lc_qt_binpath="$ac_qt_binpath:$cutlib_binpath/bin:$PATH:/usr/local/bin:/usr/local/qt3/bin:/usr/local/qt2/bin:/usr/local/qt/bin:/usr/lib/qt3/bin:/usr/lib/qt2/bin:/usr/lib/qt/bin:/usr/bin:/usr/X11R6/bin"
    AC_PATH_PROGS(MOC, moc moc3 moc2, , $lc_qt_binpath)
    dnl AC_PATH_PROGS(UIC, uic, , $lc_qt_binpath)

    if test -z "$MOC"; then
        if test -n "$ac_cv_path_moc"; then
            output=`eval "$ac_cv_path_moc --help 2>&1 | sed -e '1q' | grep Qt"`
        fi
        echo "configure:__oline__: tried to call $ac_cv_path_moc --help 2>&1 | sed -e '1q' | grep Qt" >&AC_FD_CC
        echo "configure:__oline__: moc output: $output" >&AC_FD_CC

        if test -z "$output"; then
            QT_MOC_ERROR_MESSAGE
        fi
    fi

    AC_SUBST(MOC)

    dnl We do not need UIC
    dnl [QT_UIC_ERROR_MESSAGE])
    dnl AC_SUBST(UIC)
])


dnl AC_PATH_QT([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl ------------------------------------------------------------------------
dnl Try to find the Qt headers and libraries.
dnl $(QT_LIBS) will be -Lqtliblocation (if needed) -lqt_library_name
dnl and $(QT_CFLAGS) will be -Iqthdrlocation (if needed)
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN([AC_PATH_QT],
[
AC_REQUIRE([AC_PATH_XTRA])

ac_have_qt=yes
ac_qt_binpath=""
ac_qt_includes=""
ac_qt_libraries=""
ac_qt_notfound=""
qt_libraries=""
qt_includes=""
min_qt_version=ifelse([$1], ,100, $1)

AC_ARG_WITH(qt-dir, [  --with-qt-dir           where the root of Qt is installed.],
    [  ac_qt_includes="$withval"/include
       ac_qt_libraries="$withval"/lib
       ac_qt_binpath="$withval"/bin
    ])

test -n "$QTDIR" && ac_qt_binpath="$ac_qt_binpath:$QTDIR/bin:$QTDIR"

AC_ARG_WITH(qt-includes, [  --with-qt-includes      where the Qt includes are.],
    [ ac_qt_includes=$withval ])

AC_ARG_WITH(qt-libraries, [  --with-qt-libraries     where the Qt library is installed.],
    [ ac_qt_libraries=$withval ])

if test -z "$ac_qt_includes" || test -z "$ac_qt_libraries" ; then
    AC_CACHE_VAL(ac_cv_have_qt,
    [ #try to guess Qt locations
	qt_incdirs="$QTINC
            /usr/local/lib/qt3/include
            /usr/local/lib/qt2/include
            /usr/local/qt/include
            /usr/local/include
            /usr/lib/qt3/include
            /usr/lib/qt-2.3.1/include
            /usr/lib/qt2/include
            /usr/include/qt
            /usr/lib/qt/include
            $x_includes/qt3
            $x_includes/qt2
            $x_includes/X11/qt
            $x_includes
            /usr/include"
	qt_libdirs="$QTLIB
            /usr/local/lib/qt3/lib
            /usr/local/lib/qt2/lib
            /usr/local/qt/lib
            /usr/local/lib
            /usr/lib/qt3/lib
            /usr/lib/qt-2.3.1/lib
            /usr/lib/qt2/lib
            /usr/lib/qt/lib
            /usr/lib
            /usr/lib/qt
            $x_libraries/qt3
            $x_libraries/qt2
            $x_libraries"

	if test -n "$QTDIR" ; then
            qt_incdirs="$QTDIR/include $QTDIR $qt_incdirs"
	    qt_libdirs="$QTDIR/lib $QTDIR $qt_libdirs"
        fi

	qt_incdirs="$ac_qt_includes $qt_incdirs"
	qt_libdirs="$ac_qt_libraries $qt_libdirs"

	AC_FIND_FILE(qmovie.h, $qt_incdirs, qt_incdir)
	ac_qt_includes="$qt_incdir"

	qt_libdir=NONE
	for dir in $qt_libdirs ; do
            for qtname in qt3 qt2 qt ; do
                try="ls -1 $dir/lib$qtname.* $dir/lib$qtname-mt.*"
                if test -n "`$try 2> /dev/null`"; then
                      test -z "$QTNAME" && QTNAME=$qtname
                      qt_libdir=$dir
                      break
                fi
            done
            if test x$qt_libdir != xNONE ; then
                break
            else
		echo "tried $dir" >&AC_FD_CC
            fi
        done

        ac_qt_libraries=$qt_libdir

	test -z "$QTNAME" && QTNAME=qt
        ac_QTNAME=$QTNAME

	ac_save_QTLIBS=$LIBS
	LIBS="-L$ac_qt_libraries $LIBS $PTHREAD_LIBS"
        AC_CHECK_LIB($QTNAME-mt, main, ac_QTNAME=$QTNAME-mt)
	LIBS=$ac_save_QTLIBS

	ac_cv_have_qt="ac_have_qt=yes ac_qt_includes=$ac_qt_includes ac_qt_libraries=$ac_qt_libraries ac_QTNAME=$ac_QTNAME"

        if test "$ac_qt_includes" = NO || test "$ac_qt_libraries" = NONE ; then
            ac_cv_have_qt="ac_have_qt=no"
            ac_qt_notfound="(headers)"
            if test "$ac_qt_includes" = NO; then
                if test "$ac_qt_libraries" = NONE; then
                    ac_qt_notfound="(headers and libraries)"
                fi
            else
                ac_qt_notfound="(libraries)";
            fi
        fi
    ])
    eval "$ac_cv_have_qt"
else
    for qtname in qt3 qt2 qt ; do
        try="ls -1 $ac_qt_libraries/lib$qtname.* $ac_qt_libraries/lib$qtname-mt.*"
        if test -n "`$try 2> /dev/null`"; then
            test -z "$QTNAME" && QTNAME=$qtname
            break
        fi
    done
    ac_QTNAME=$QTNAME
    ac_save_QTLIBS=$LIBS
    LIBS="-L$ac_qt_libraries $LIBS $PTHREAD_LIBS"
    AC_CHECK_LIB($QTNAME-mt, main, ac_QTNAME=$QTNAME-mt)
    LIBS=$ac_save_QTLIBS
fi

if test x$ac_have_qt = xyes ; then
    AC_MSG_CHECKING([for Qt library (version >= $min_qt_version)])
    AC_CACHE_VAL(ac_cv_qt_version,
    [
	AC_LANG_SAVE
	AC_LANG_CPLUSPLUS

	ac_save_QTCXXFLAGS=$CXXFLAGS
	ac_save_QTLIBS=$LIBS
	CXXFLAGS="$CXXFLAGS -I$ac_qt_includes"
	LIBS="-L$ac_qt_libraries -l$ac_QTNAME $X_LIBS $LIBS $PTHREAD_LIBS"
	AC_TRY_RUN([
	    /*#include <qapplication.h>*/
	    /*#include <qmovie.h>*/
	    #include <qstring.h>
	    #include <qglobal.h>
	    #include <stdio.h>

	    int main(int argc, char* argv[]) {
		/*QApplication a( argc, argv );*/
		/*QMovie m; int s = m.speed();*/
		QString qa("test");
		unsigned int v = QT_VERSION;
		FILE* f = fopen("conf.qttest", "w");
		if (v > 400) v = (((v >> 16) & 0xff) * 100) + (((v >> 8) & 0xff) * 10) + (v & 0xff);
		if (f) fprintf(f, "%d\n", v);
		return 0;
	    }
	],
	[ AC_MSG_RESULT(yes); ac_cv_qt_version=`cat conf.qttest`; rm -f conf.qttest ],
	[ AC_MSG_RESULT(no); ac_have_qt=no; ac_cv_qt_version=ERROR ],
	[ echo $ac_n "cross compiling; assumed OK... $ac_c" ])

	if test x$ac_cv_qt_version = xERROR ; then
	    AC_MSG_WARN([
*** Could not run Qt test program, checking why...
*** Configure discovered/uses these settings:
*** Qt libraries: $ac_qt_libraries
*** Qt headers: $ac_qt_includes
*** Note:
***    Compilation of Qt utilities also might be turned off (if not wanted).
***    If you are experiencing problems which will not be described
***    bellow please report then on 'avifile@prak.org' mailing list
***    (i.e. some misdetection or omitted path)]
             )
             AC_TRY_LINK([ #include <qstring.h> ],
		     [ QString qa("test") ],
                     [ AC_MSG_ERROR([
*** Qt test program compiled, but did not run. This usually means
*** that the run-time linker is not finding Qt library or finding the wrong
*** version of Qt. If it is not finding Qt, you will need to set your
*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point
*** to the installed location  Also, make sure you have run ldconfig if that
*** is required on your system.
***
*** If you have an old version installed, it is best to remove it, although
*** you may also be able to get things to work by modifying LD_LIBRARY_PATH
***
*** i.e. bash> export LD_LIBRARY_PATH=$ac_qt_libraries:\$LD_LIBRARY_PATH]) ],
                     [ AC_MSG_ERROR([
*** The test program failed to compile or link. See the file config.log for the
*** exact error that occured. This usually means Qt was incorrectly installed
*** or that you have moved Qt since it was installed. In the latter case, you
*** may want to set QTDIR shell variable
***
*** Another possibility is you are try to link Qt libraries compiled with
*** different version of g++. Unfortunately you can not mix C++ libraries
*** and object files created with different C++ compiler
*** i.e. g++-2.96 libraries and g++-2.95 objects
*** The most common case: Some users seems to be downgrading their
*** compiler without thinking about consequencies...]) ]
	    )
	fi

	CXXFLAGS=$ac_save_QTCXXFLAGS
	LIBS=$ac_save_QTLIBS
	AC_LANG_RESTORE
     ])
     qt_version=$ac_cv_qt_version
fi

if test x$ac_have_qt != xyes; then
    AC_MSG_WARN([
*** Could not find usable Qt $ac_qt_notfound on your system!
*** If it _is_ installed, delete ./config.cache and re-run ./configure,
*** specifying path to Qt headers and libraries in configure options.
*** Switching off Qt compilation!])
    ac_have_qt=no
else
    AC_MSG_RESULT([found $ac_QTNAME version $qt_version, libraries $ac_qt_libraries, headers $ac_qt_includes])
    if test $min_qt_version -le $qt_version ; then
        qt_libraries=$ac_qt_libraries
        qt_includes=$ac_qt_includes

        if test "$qt_includes" = "$x_includes" -o -z "$qt_includes" ; then
            QT_CFLAGS=
        else
            QT_CFLAGS="-I$qt_includes"
            all_includes="$QT_CFLAGS $all_includes"
        fi

        if test "$qt_libraries" = NONE -o "$qt_libraries" = "$x_libraries" -o -z "$qt_libraries" -o "$qt_libraries" = "/usr/lib" ; then
            QT_LIBS=
        else
            QT_LIBS="-L$qt_libraries"
            all_libraries="$QT_LIBS $all_libraries"
        fi

        QT_LIBS="$QT_LIBS -l$ac_QTNAME"

        if test x$ac_have_qt = xyes ; then
            AC_PATH_QT_MOC_UIC
        fi

        AC_SUBST(qt_version)
        AC_SUBST(QT_CFLAGS)
        AC_SUBST(QT_LIBS)
    else
        AC_MSG_WARN([
*** Unsupported old version of Qt library found. Please upgrade.])
        ac_have_qt=no
    fi
fi

if test x$ac_have_qt = xyes ; then
    ifelse([$2], , :, [$2])
else
    ifelse([$3], , :, [$3])
fi

])
