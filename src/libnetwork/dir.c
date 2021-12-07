/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "dir.h"
#include "nconfig.h"
#include "plugin.h"
#include "util.h"
#include "pluginhelper.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

int buildPath( const char *path, mode_t mode ) {
	char cpy[ PATH_MAX ];
	char *ptr = cpy;
	int final = 0;
	int retval;
	DIR *dir;

	snprintf( cpy, PATH_MAX, "%s", path );

	do {
		ptr = strchr( ptr + 1, '/' );
		if ( !ptr )
			final = 1;
		else
			*ptr = '\0';

		retval = mkdir( cpy, mode );
		if ( retval < 0 ) {
			if ( errno == EEXIST ) {
				dir = opendir( cpy );
				if ( !dir ) {
					errno = EEXIST;
					LNCdebug( LNC_ERROR, 1, "buildPath( %s ) -> mkdir( %s, %d )", path, cpy, mode );
					return -EXIT_FAILURE;
				} else
					closedir( dir );

			} else {
				LNCdebug( LNC_ERROR, 1, "buildPath( %s ) -> mkdir( %s, %d )", path, cpy, mode );
				return -EXIT_FAILURE;
			}
		}

		if ( final )
			break;

		*ptr = '/';
	} while ( 1 );

	return EXIT_SUCCESS;
}
