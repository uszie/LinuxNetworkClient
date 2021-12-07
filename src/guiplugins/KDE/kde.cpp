#include "config.h"
#include "plugin.h"

#include <kapplication.h>
#include <kglobal.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kconfig.h>
#include <kio/passdlg.h>

#include <X11/Xlib.h>
#include <stdlib.h>

using namespace KIO;

extern "C"
{
	void *giveGUIPluginInfo ( void );
	extern int getActiveWindowID ( void );
}

static int KDEdialog ( const char *URL, struct auth *auth, const char *processName )
{
	int winID = getActiveWindowID();
	int argc = 2;
	const char *argv[] = { "Networkclient KDE plugin", "--nocrashhandler", NULL };

	( void ) processName;

	if ( !auth )
		return -EXIT_FAILURE;

	KAboutData about ( "LinuxNetworkClient", I18N_NOOP ( "Networkclient KDE plugin" ), VERSION );
	KCmdLineArgs::init ( argc, ( char ** ) argv, &about );
	KApplication app;

	PasswordDialog dlg ( NULL, auth->username );
	QString prompt ( i18n ( "You need to supply a username and a password" ) + "\n" + URL );
	dlg.setPlainCaption ( i18n ( "Authorization Dialog" ) );
	dlg.setPrompt ( prompt );

	if ( winID > 0 )
		XSetTransientForHint ( dlg.x11Display(), dlg.winId(), winID );


	int retval = dlg.exec();
	if ( retval != KIO::PasswordDialog::Accepted )
		return -EXIT_FAILURE;

	strlcpy ( auth->username, dlg.username().ascii(), NAME_MAX + 1 );
	strlcpy ( auth->password, dlg.password().ascii(), NAME_MAX + 1 );

	return EXIT_SUCCESS;
}

static const char KDEtype[] = "KDE";
static const char KDEdescription[] = "This plugin allows you to have KDE styled username/password dialogs";
static const char *KDEdependencies[] = { "lib_global_plugin_Xext.so", NULL };

static GUIPlugin kde =
{
	NULL,
	NULL,
	NULL,
	KDEtype,
	KDEdialog,
	NULL,
	KDEdescription,
	NULL,
	KDEdependencies,
 	{0},
 	0,
	1,
	0,
	0,
	0,
	{ NULL, NULL, NULL, { 0 } }
};

void *giveGUIPluginInfo ( void )
{
	return &kde;
}
