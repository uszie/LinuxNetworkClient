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
#include "cursesdialog.h"

#include <term.h>

int cursesDialog( const char *URL, struct auth *auth, const char *processName );
void *giveGUIPluginInfo( void );
/*static int putch( int c );

static int putch( int c )
{
	return putchar( c );
}*/

/*int cursesGuess( struct comm_data *pending )
{
	char *name;
	int cmp;

	name = getProcessName( pending->ppid );
	LNCLNCLNCdebug( LNC_INFO, 0, "parent process name = \"%s\", parent pid = \"%d\"", name, pending->ppid );

	cmp = strcmp( name, "mc" );
	free( name );
	if ( cmp == 0 )
		return 1;

	return 0;
}*/

int cursesDialog( const char *URL, struct auth *auth, const char *processName ) {
	int retval;

	cursesSetUsername( auth->username );

	retval = cursesPasswordDialog( processName, URL );
	if ( retval < 0 ) {
		LNCdebug( LNC_INFO, 0, "cursesDialog() -> cursesPasswordDialog(): user cancelled" );
		return -EXIT_FAILURE;
	}

	strlcpy( auth->username, cursesGetUsername(), NAME_MAX + 1 );
	strlcpy( auth->password, cursesGetPassword(), NAME_MAX + 1 );

	return EXIT_SUCCESS;
}

static const char cursesType[] = "curses";
static const char cursesDescription[] = "This plugin allows you to have Curses/MC styled username/password dialogs";

static GUIPlugin curses =
    {
        .type = cursesType,
        .dialog = cursesDialog,
        .description = cursesDescription,
        .needsX = 0,
        .singleShot = 0,
        .refreshTerminalScreen = 1
    };

void *giveGUIPluginInfo( void ) {
	return & curses;
}
