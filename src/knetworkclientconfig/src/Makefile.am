INCLUDES = -I/home/usarin/Documents/Projects/LinuxNetworkClient/src/libnetwork \
	$(all_includes)
METASOURCES = AUTO

kde_module_LTLIBRARIES = kcm_knetworkclientconfig.la

kcm_knetworkclientconfig_la_SOURCES = knetworkclientconfig.cpp options.ui options.ui.h knetworkclientconfigui.ui knetworkclientconfigui.ui.h
kcm_knetworkclientconfig_la_LIBADD = $(LIB_KIO) -lnetwork
kcm_knetworkclientconfig_la_LDFLAGS = -module $(all_libraries)

knetworkclientconfig_DATA = knetworkclientconfig.desktop
knetworkclientconfigdir = $(kde_appsdir)/Settings/Network

mandrakeconfig_DATA = kdenetwork-knetworkclientconfig
mandrakeconfigdir = $(prefix)/lib/menu

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/knetworkclientconfig.pot
