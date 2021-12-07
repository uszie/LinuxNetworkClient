/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "cursesdialog.h"
#include "util.h"

int init;
int entryCB( EObjectType cdktype, void *object, void *clientData, chtype key ) {
	static int init = 0;
	CDKBUTTONBOX *buttonbox = ( CDKBUTTONBOX * ) clientData;

	( void ) cdktype;
	( void ) object;
	( void ) key;

	if ( init ) {
		init = 0;
		buttonbox->currentButton = -1;
		return -1;
	}
	init += 1;

	return 0;
}

WINDOW *create_newwin( WINDOW *window, int height, int width ) {
	WINDOW * mainWin, *boxWin;
	chtype *holder = NULL;
	int j1, j2;
	int x, y;

	if ( height > LINES )
		height = LINES;

	if ( width > COLS )
		width = COLS;

	if ( ( COLS - width ) < 2 )
		x = 0;
	else
		x = ( ( COLS - width ) / 2 );

	if ( ( LINES - height ) < 2 )
		y = 0;
	else
		y = ( ( LINES - height ) / 2 ) + 1;

	mainWin = subwin( window, height, width, y - 2, x );
	if ( !mainWin ) {
		LNCdebug( LNC_ERROR, 2, "create_newwin() -> newwin()" );
		return NULL;
	}

	boxWin = subwin( mainWin, height - 2, width - 2, y - 1, x + 1 );
	if ( !boxWin ) {
		LNCdebug( LNC_ERROR, 1, "create_newwin() -> subwin()" );
		delwin( mainWin );
		return NULL;
	}

	box( boxWin, 0 , 0 );
	holder = char2Chtype( "</57>", &j1, &j2 );
	wbkgd( mainWin, holder[ 0 ] );
	freeChtype( holder );
	wrefresh( mainWin );

	return boxWin;
}

char *username;
char *password;

int cursesPasswordDialog( const char *caption, const char *URL ) {
	CDKSCREEN * cdkscreen = NULL;
	CDKENTRY *usernameEntry = NULL;
	CDKENTRY *passwordEntry = NULL;
	CDKBUTTONBOX *buttonWidget = NULL;
	CDKLABEL *captionLabel = NULL;
	CDKLABEL *messageLabel = NULL;
	WINDOW *cursesWin = NULL;
	WINDOW *boxWin = NULL;
	chtype *holder = NULL;
	char *captionString[ 1 ];
	char *messageString[ 4 ];
	char *buttons[] = { "[< </33>O</57>k >]", "[ </33>C</57>ancel ]", NULL };
	int retval;
	int j1, j2;
	int width;
	int height;
	int exitStatus = -1;

	cursesWin = initscr();
	if ( !cursesWin ) {
		LNCdebug( LNC_ERROR, 1, "cursesPasswordDialog() -> initscr()" );
		goto cleanup;
	}

	leaveok( cursesWin, TRUE );
	cdkscreen = initCDKScreen( cursesWin );
	if ( !cdkscreen ) {
		LNCdebug( LNC_ERROR, 1, "cursesPasswordDialog() -> initCDKScreen()" );
		goto cleanup;
	}

	initCDKColor();

	holder = char2Chtype( "</5>", &j1, &j2 );
	if ( !holder ) {
		LNCdebug( LNC_ERROR, 1, "cursesPasswordDialog() -> char2Chtype()" );
		goto cleanup;
	}

	wbkgd( cdkscreen->window, holder[ 0 ] );
	wrefresh( cdkscreen->window );
	freeChtype( holder );


	holder = char2Chtype( "</63>", &j1, &j2 );
	if ( !holder ) {
		LNCdebug( LNC_ERROR, 1, "cursesPasswordDialog() -> char2Chtype()" );
		goto cleanup;
	}

	usernameEntry = newCDKEntry( cdkscreen, CENTER, CENTER , "", "Username: ", A_NORMAL, ' ', vMIXED, 34, 0, 256, FALSE, FALSE );
	setCDKEntryBackgroundColor( usernameEntry, "</57>" );
	wbkgd( usernameEntry->fieldWin, holder[ 0 ] );
	if ( username )
		setCDKEntryValue( usernameEntry, username );

	passwordEntry = newCDKEntry( cdkscreen, WIN_XPOS( usernameEntry->win ), WIN_YPOS( usernameEntry->win ) + usernameEntry->boxHeight + 1, "", "Password: ", A_NORMAL, ' ', vHMIXED, 34, 0, 256, FALSE, FALSE );
	setCDKEntryBackgroundColor( passwordEntry, "</57>" );
	if ( password )
		setCDKEntryValue( passwordEntry, password );
	wbkgd( passwordEntry->fieldWin, holder[ 0 ] );
	setCDKEntryHiddenChar( passwordEntry, '*' );

	buttonWidget = newCDKButtonbox( cdkscreen, WIN_XPOS( passwordEntry->win ), WIN_YPOS( passwordEntry->win ) + passwordEntry->boxHeight + 0, 1, passwordEntry->boxWidth + 0, NULL, 1, 2, buttons, 2, A_NORMAL | COLOR_PAIR( 56 ), FALSE, FALSE );
	setCDKButtonboxBackgroundColor( buttonWidget, "</57>" );
	buttonWidget->boxHeight = 2;
	wresize( buttonWidget->win, 2, buttonWidget->boxWidth );
	buttonWidget->columnWidths[ 0 ] = WIN_WIDTH( usernameEntry->win ) - 15;
	buttonWidget->colAdjust = 3;
	buttonWidget->currentButton = -1;
	freeChtype( holder );


	captionString[ 0 ] = malloc( sizeof( char ) * ( strlen( caption ) + 6 ) );
	sprintf( captionString[ 0 ], "</33>%s", caption );
	captionLabel = newCDKLabel( cdkscreen, CENTER, WIN_YPOS( usernameEntry->win ) - 7, ( char ** ) captionString, 1, FALSE, FALSE );
	setCDKLabelBackgroundColor( captionLabel, "</57>" );

	messageString[ 0 ] = strdup( "</57>You need to supply a username and a password," );
	messageString[ 1 ] = strdup( "</57>to access this service:" );
	messageString[ 2 ] = strdup( "" );
	messageString[ 3 ] = malloc( sizeof( char ) * ( strlen( URL) + 6 ) );
	sprintf( messageString[ 3 ], "</33>%s", URL );
	messageLabel = newCDKLabel( cdkscreen, CENTER, WIN_YPOS( usernameEntry->win ) - 5, ( char ** ) messageString, 4, FALSE, FALSE );
	setCDKLabelBackgroundColor( messageLabel, "</57>" );

	height = WIN_HEIGHT( usernameEntry->win ) + 14;
	width = WIN_WIDTH( usernameEntry->win ) + 6;
	boxWin = create_newwin( cdkscreen->window, height, width );
	if ( !boxWin ) {
		LNCdebug( LNC_ERROR, 1, "cursesPasswordDialog() -> create_newwin()" );
		goto cleanup;
	}

	bindCDKObject ( vBUTTONBOX, buttonWidget, KEY_TAB, entryCB, buttonWidget );

	drawCDKEntry( usernameEntry, FALSE );
	drawCDKEntry( passwordEntry, FALSE );
	drawCDKButtonbox ( buttonWidget, FALSE );
	drawCDKLabel( captionLabel, FALSE );
	drawCDKLabel( messageLabel, FALSE );

	while ( 1 ) {
		username = copyChar( activateCDKEntry( usernameEntry, NULL ) );
		if ( usernameEntry->exitType == vESCAPE_HIT ) {
			if ( username )
				freeChar( username );
			username = NULL;
			break;
		}

		password = copyChar( activateCDKEntry( passwordEntry, NULL ) );
		if ( passwordEntry->exitType == vESCAPE_HIT ) {
			if ( username )
				freeChar( username );
			if ( password )
				freeChar( password );
			username = password = NULL;
			break;
		}

		setCDKButtonboxHighlight( buttonWidget, A_STANDOUT | COLOR_PAIR( 56 ) );
		buttonWidget->currentButton = 0;
		retval = activateCDKButtonbox( buttonWidget, NULL );
		setCDKButtonboxHighlight( buttonWidget, A_NORMAL );
		drawCDKButtonbox( buttonWidget, FALSE );

		exitStatus = 0;
		if ( retval == 0 && buttonWidget->exitType == vNORMAL )
			break;

		if ( username ) {
			freeChar( username );
			username = NULL;
		}

		if ( password ) {
			freeChar( password );
			password = NULL;
		}

		exitStatus = -1;
		if ( retval == 1 && buttonWidget->exitType == vNORMAL )
			break;
	}

cleanup:
	if ( buttonWidget )
		destroyCDKButtonbox( buttonWidget );
	if ( passwordEntry )
		destroyCDKEntry( passwordEntry );
	if ( usernameEntry )
		destroyCDKEntry( usernameEntry );
	if ( captionLabel )
		destroyCDKLabel( captionLabel );
	if ( messageLabel )
		destroyCDKLabel( messageLabel );
	if ( cdkscreen )
		destroyCDKScreen( cdkscreen );
	if ( boxWin ) {
		if ( boxWin->_parent )
			delwin( boxWin->_parent );
		delwin( boxWin );
	}

	endCDK();
	delwin( cursesWin );
/*#include  <term.h>
	retval = setupterm((char *) 0, STDOUT_FILENO, (int *) 0);
	if ( retval <  0 )
		LNCdebug( LNC_ERROR, 1, "cursesdialog() -> setupterm()" );

	retval = tputs(clear_screen, lines > 0 ? lines : 1, putchar );
	if ( retval <  0 )
		LNCdebug( LNC_ERROR, 1, "cursesdialog() -> tputs()" );

	if ( exitStatus == 0 )
		printf( "Username = %s\nPassword = %s\n", username, password );
	else
		printf( "User canceled or program failure\n" );*/

	return ( exitStatus );
}

char *cursesGetUsername() {
	return username;
}

char *cursesGetPassword() {
	return password;
}

void cursesSetUsername( char *user ) {
	username = user;
}

void cursesSetPassword( char *pass ) {
	password = pass;
}
