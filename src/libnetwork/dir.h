/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_DIR_H__
#define __LNC_DIR_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

	char *shortcutList[ 10 ];

	int buildPath( const char * path, mode_t mode );
	/*	void makeServiceDir( Service * service );
		void removeServiceDir( Service * service );
		void touchServiceDir( Service * service );

		void makeShortcutTargets( void );
		void makeShortcutToMount( Service * service );
		void addToShortcutList( Service * service );
		void readShortcutListFromFile( void );
		void writeShortcutListToFile( void );
		void makeShortcut( char * shortcut );
		void removeShortcut( char * shortcut );

		void makeServiceTree( List * head );
		void removeServiceTree( List * head );
		void makeShortcutTree( void );
		void removeShortcutTree( void );*/
#ifdef __cplusplus
}
#endif
#endif
