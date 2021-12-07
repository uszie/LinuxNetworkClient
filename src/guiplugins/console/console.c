/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int consoleDialog( const char *URL, struct auth *auth, const char *processName );
void *giveGUIPluginInfo( void );

int consoleDialog( const char *URL, struct auth *auth, const char *processName ) {
	char password[ BUFSIZ ] = "";
	char username[ BUFSIZ ] = "";
	char message[ BUFSIZ ];
	int len;

	( void ) processName;

	signal( SIGQUIT, SIG_DFL );
	signal( SIGINT, SIG_DFL );
	signal( SIGTERM, SIG_DFL );

	snprintf( message, BUFSIZ, "You need to supply a username and a password to access this service ( %s )\nUsername: ", URL );
	write( 1, message, strlen( message ) );

	fgets( username, BUFSIZ, stdin );
	len = strlen( username );
	if ( username[ len - 1 ] == '\n' )
		username[ len - 1 ] = '\0';

	snprintf( message, BUFSIZ, "Password: " );
	write( 1, message, strlen( message ) );

	fgets( password, BUFSIZ, stdin );
	len = strlen( password );
	if ( password[ len - 1 ] == '\n' )
		password[ len - 1 ] = '\0';


	auth->username[ 0 ] = auth->password[ 0 ] = '\0';
	strlcpy( auth->username, username, NAME_MAX + 1 );
	strlcpy( auth->password, password, NAME_MAX + 1 );


	return EXIT_SUCCESS;
}

static const char consoleType[] = "console";
static const char consoleDescription[] = "This plugin allows you to have console styled username/password dialogs";

static GUIPlugin console =
    {
        .type = consoleType,
        .dialog = consoleDialog,
        .description = consoleDescription,
        .needsX = 0,
        .singleShot = 1,
        .refreshTerminalScreen = 0
    };

void *giveGUIPluginInfo( void ) {
	return &console;
}
