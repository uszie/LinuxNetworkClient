#include "plugin.h"

#include <qapplication.h>
#include "qtdialog.h"
#include <X11/Xlib.h>

extern "C" {
	void *giveGUIPluginInfo( void );
	extern int getActiveWindowID( void );
}

int argc = 1;
static const char *argv[] = { "networkclient", NULL
                            };
static int QTdialog( const char *URL, struct auth *auth, const char *processName ) {
	int winID = getActiveWindowID();
	char message[ BUFSIZ ];

	if ( !auth )
		return -EXIT_FAILURE;

	QApplication app( argc, ( char ** ) argv );

	sprintf( message, "You need to supply a username and\na password to access this service\n\n%s", URL );

	QTPasswordDialog dlg;
	dlg.setCaption( processName );
	dlg.promptLabel->setText( message );
	dlg.userEdit->setText( auth->username );
	dlg.passEdit->setText( auth->password );

	if ( winID > 0 )
		XSetTransientForHint( dlg.x11Display(), dlg.winId(), winID );


	int retval = dlg.exec();
	if ( retval <= 0 )
		return -EXIT_FAILURE;

	strlcpy( auth->password, dlg.getPassword(), NAME_MAX + 1 );
	strlcpy( auth->username, dlg.getUsername(), NAME_MAX + 1 );

	return EXIT_SUCCESS;
}

static const char QTtype[] = "QT";
static const char QTdescription[] = "This plugin allows you to have QT styled username/password dialogs";
static const char *QTdependencies[] = { "lib_global_plugin_Xext.so", NULL };

static GUIPlugin qt =
    {
        NULL,
        NULL,
	NULL,
        QTtype,
        QTdialog,
        NULL,
        QTdescription,
	NULL,
	QTdependencies,
 	{0},
 	0,
 	1,
        0,
        0,
	0,
	{ NULL, NULL, NULL, {0} }
    };

void *giveGUIPluginInfo( void ) {
	return &qt;
}
