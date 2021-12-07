#include"config.h"
#include "plugin.h"
#include "gtkdialog.h"

#include <stdlib.h>
#include <string.h>
#include <gdk/gdkprivate.h>
#include <X11/Xlib.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>

void *giveGUIPluginInfo( void );
extern int getActiveWindowID( void );

static int GTKdialog( const char *URL, struct auth *auth, const char *processName ) {
	GtkWidget * dlg;
	int argc = 1;
	char **argv;
	char *tmp;
	char path[ PATH_MAX ];
	int winID;

	winID = getActiveWindowID();
	argv = malloc( 1 * sizeof( char * ) );
	argv[ 0 ] = strdup( "networkclient" );
	argv[ 1 ] = NULL;

	gtk_init ( &argc, &argv );
	snprintf( path, PATH_MAX, "%s/share/icons", PREFIX );
	add_pixmap_directory( path );

	dlg = GTKPasswordDialog( processName, URL );
	if ( !dlg ) {
		LNCdebug( LNC_ERROR, 1, "GTKDialog() -> GTKPasswordDialog()" );
		return -EXIT_FAILURE;
	}

	setUsername( auth->username );
	setPassword( auth->password );

	gtk_widget_show( dlg );
	if ( winID > 0 )
		XSetTransientForHint( gdk_x11_get_default_xdisplay(), GDK_WINDOW_XWINDOW( dlg->window ), winID );

	gtk_main();
	if ( isCanceled() )
		return -EXIT_FAILURE;

	auth->username[ 0 ] = auth->password[ 0 ] = '\0';
	tmp = getUsername();
	if ( tmp )
		strlcpy( auth->username, tmp, NAME_MAX + 1 );

	tmp = getPassword();
	if ( tmp )
		strlcpy( auth->password, tmp, NAME_MAX + 1 );


	return EXIT_SUCCESS;
}

static const char GTKtype[] = "GTK";
static const char GTKdescription[] = "This plugin allows you to have GTK styled username/password dialogs";
static const char *GTKdependencies[] = { "lib_global_plugin_Xext.so", NULL };

static GUIPlugin gtk =
    {
        .type = GTKtype,
        .dialog = GTKdialog,
        .description = GTKdescription,
	.dependencies = GTKdependencies,
	.needsX = 1,
        .singleShot = 0,
        .refreshTerminalScreen = 0
    };

void *giveGUIPluginInfo( void ) {
	return &gtk;
}
