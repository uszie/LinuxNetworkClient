/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/
#include"config.h"
#include "plugin.h"

#include <stdlib.h>
#include <string.h>

#include <gnome.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>

static int GNOMEDialog( const char *URL, struct auth *auth, const char *processName );
void *giveGUIPluginInfo( void );
extern int getActiveWindowID( void );

struct callbackData {
	int windowID;
	char *username;
	char *password;
};

static void dialog_show_callback( GtkWidget *widget, gpointer callback_data ) {
	struct callbackData * data = ( struct callbackData * ) callback_data;

	XSetTransientForHint( GDK_WINDOW_XDISPLAY( widget->window ), GDK_WINDOW_XID( widget->window ), data->windowID );
	gnome_password_dialog_set_username( GNOME_PASSWORD_DIALOG( widget ), data->username );
	gnome_password_dialog_set_password( GNOME_PASSWORD_DIALOG( widget ), data->password );
}

static int GNOMEDialog( const char *URL, struct auth *auth, const char *processName ) {
	int argc = 3;
	char *argv[] = { "Networkclient GNOME plugin", "--disable-crash-dialog", "--sm-disable" };
	char *tmp;
	char *message;
	int windowID;
	struct callbackData *data;
	GnomePasswordDialog *dialog;
	gboolean dialog_result;

	( void ) processName;

	windowID = getActiveWindowID();
	data = malloc( sizeof( struct callbackData ) );
	if ( !data ) {
		LNCdebug( LNC_ERROR, 1, "GNOMEDialog( %s ) -> malloc()", URL );
		return -EXIT_FAILURE;
	}

	data->windowID = windowID;
	data->username = auth->username;
	data->password = auth->password;

	textdomain( "GNOMEDialog" );
	bindtextdomain( "GNOMEDialog", LOCALE_DIR );

	gnome_program_init( "GNOMEDialog", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL, NULL, NULL );
	message = g_strdup_printf ( "%s:\n\n%s\n", _( "You must log in to access" ), URL );

	dialog = GNOME_PASSWORD_DIALOG ( gnome_password_dialog_new ( _( "Authentication Required" ), message, "", "", FALSE ) );
	g_free ( message );
	g_signal_connect_after ( dialog, "show", G_CALLBACK( dialog_show_callback ), ( gpointer ) data );

	dialog_result = gnome_password_dialog_run_and_block ( dialog );
	if ( !dialog_result ) {
		free( data );
		gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
		return -EXIT_FAILURE;
	}

	auth->username[ 0 ] = auth->password[ 0 ] = '\0';
	tmp = gnome_password_dialog_get_username( dialog );
	if ( tmp )
		strlcpy( auth->username, tmp, NAME_MAX + 1 );

	tmp = gnome_password_dialog_get_password( dialog );
	if ( tmp )
		strlcpy( auth->password, tmp, NAME_MAX + 1 );

	free( data );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return EXIT_SUCCESS;
}

static const char GNOMEType[] = "GNOME";
static const char GNOMEDescription[] = "This plugin allows you to have GNOME styled username/password dialogs";
static const char *GNOMEDependencies[] = { "lib_global_plugin_Xext.so", NULL
                                         };

static GUIPlugin gnome =
    {
        .type = GNOMEType,
        .dialog = GNOMEDialog,
        .description = GNOMEDescription,
        .dependencies = GNOMEDependencies,
	.needsX = 1,
        .singleShot = 0,
        .refreshTerminalScreen = 0
    };

void *giveGUIPluginInfo( void ) {
	return & gnome;
}
