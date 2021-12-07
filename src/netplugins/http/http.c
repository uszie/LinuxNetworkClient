/***************************************************************************
 *   Copyright (C) 2003 by Usarin Heininga                                 *
 *   usarin@heininga.nl                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "networkclient.h"
#include <libintl.h>

static const char indexHTML [ BUFSIZ ] = "<html>\n<head>\n<meta HTTP-EQUIV=\"REFRESH\" content=\"0; url=$TEMPLATE_URL\">\n</head>\n";
static const char HTTPdescription[] = "This plugin allows you to browse networks that rely on the FTP protocol";
static const char HTTPprotocol[] = "HTTP";
static char HTTPnetworkName[ NAME_MAX + 1 ];


NETWORK_DIR *HTTPopendir( const char *URL, unsigned int flags, NETWORK_DIR *dir ) {
	Service *found;

	( void ) flags;

	found = findListEntry( browselist, URL );
	if ( !found ) {
		errno = ENOENT;
		LNCdebug( LNC_ERROR, 1, "HTTPopendir( %s )", URL );
		return NULL;
	}

	putListEntry( browselist, found );

	return dir;
}

int HTTPclosedir( NETWORK_DIR *dir ) {
	( void ) dir;

	return EXIT_SUCCESS;
}

int HTTPreaddir( NETWORK_DIR *dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result ){
	if ( ( void_to_int_cast dir->privateData ) ) {
		errno = 0;
		*result = NULL;
		return EXIT_SUCCESS;
	}

	dir->privateData = int_to_void_cast 1;
	entry->type = DT_REG;
	strlcpy( entry->name, "index.html", NAME_MAX + 1 );
	strlcpy( entry->comment, "Homepage", NAME_MAX + 1 );
	entry->IP[ 0 ] = '\0';
	*result = entry;

	return EXIT_SUCCESS;
}

NETWORK_FILE *HTTPopen( const char *URL, int flags, mode_t mode, NETWORK_FILE *file  ) {
	Service *found;
	char tmp[ PATH_MAX ];
	char webURL[ PATH_MAX ];
	char buffer[ BUFSIZ ];
	char *ptr;

	snprintf( tmp, PATH_MAX, "%s", URL );
	ptr = strcasestr( tmp, "index.html" );
	if ( !ptr ) {
		errno = ENOENT;
		LNCdebug( LNC_ERROR, 1, "HTTPopen( %s, %d, %d)", URL, flags, mode );
		return NULL;
	}

	*( ptr-1 ) = '\0';

	found = findListEntry( browselist, tmp );
	if ( !found ) {
		errno = ENOENT;
		LNCdebug( LNC_ERROR, 1, "HTTPopen( %s, %d, %d )", URL, flags, mode );
		return NULL;
	}

	snprintf( buffer, BUFSIZ, "%s", indexHTML );
	if ( found->domain )
		snprintf( webURL, PATH_MAX, "http://%s.%s", found->host, found->domain );
	else
		snprintf( webURL, PATH_MAX, "http://%s", found->host );

	putListEntry( browselist, found );
	replaceString( buffer, "$TEMPLATE_URL", webURL );
	file->privateData = ( void * ) strdup( buffer );

	return file;

}

int HTTPclose( NETWORK_FILE *file ) {
	if ( file->privateData ) {
		free( file->privateData );
		file->privateData = NULL;
	}

	return EXIT_SUCCESS;
}

ssize_t HTTPread( NETWORK_FILE *file, void *buf, size_t count ) {
	size_t size;
	char *httpFile;

	httpFile = ( char * ) file->privateData;
	if ( !httpFile )
		return EXIT_SUCCESS;

	size = ( strlen( httpFile ) + 1 ) * sizeof( char );
	if ( count < size ) {
		errno = EINVAL;
		LNCdebug( LNC_ERROR, 1, "HTTPread( %s, %d )", file->URL, count );
		return -EXIT_FAILURE;
	}

	snprintf( buf, count, "%s", httpFile );

	free( file->privateData );
	file->privateData = NULL;

	return size;
}

static int HTTPstat( const char *URL, struct stat *buf ) {
	struct timeval time;
	Service *found;
	char *ptr;
	char tmp[ PATH_MAX ];
	char webURL[ PATH_MAX ];
	char buffer[ BUFSIZ ];

	snprintf( tmp, PATH_MAX, "%s", URL );
	ptr = strcasestr( tmp, "index.html" );
	if ( !ptr ) {
		found = findListEntry( browselist, URL );
		if ( !found ) {
			errno = ENOENT;
			LNCdebug( LNC_ERROR, 1, "HTTPstat( %s )", URL );
			return -EXIT_FAILURE;
		}

		gettimeofday( &time, NULL );
		buf->st_nlink = 3;
		buf->st_size = 3 * 24;
		buf->st_mode = S_IFDIR | 0755;
		buf->st_uid = getuid();
		buf->st_gid = getgid();
		buf->st_mtime = buf->st_ctime =  LNCStart.tv_sec;
		buf->st_atime = time.tv_sec;
		putListEntry( browselist, found );

		return EXIT_SUCCESS;
	} else if ( *( ptr+10 ) == '\0' ) {
		*(ptr-1) = '\0';
		found = findListEntry( browselist, tmp );
		if ( !found ) {
			errno = ENOENT;
			LNCdebug( LNC_ERROR, 1, "HTTPstat( %s )", URL );
			return -EXIT_FAILURE;
		}

		snprintf( buffer, BUFSIZ, "%s", indexHTML );
		if ( found->domain )
			snprintf( webURL, PATH_MAX, "http://%s.%s", found->host, found->domain );
		else
			snprintf( webURL, PATH_MAX, "http://%s", found->host );

		putListEntry( browselist, found );
		replaceString( buffer, "$TEMPLATE_URL", webURL );

		gettimeofday( &time, NULL );
		buf->st_size = ( strlen( buffer ) + 1 ) * sizeof( char );
		buf->st_mode = S_IFREG | 0644;
		buf->st_uid = getuid();
		buf->st_gid = getgid();
		buf->st_mtime = buf->st_ctime =  LNCStart.tv_sec ;
		buf->st_atime = time.tv_sec;

		return EXIT_SUCCESS;
	}

	LNCdebug( LNC_ERROR, 1, "HTTPstat( %s )", URL );
	errno = ENOENT;
	return -EXIT_FAILURE;
}

static NetworkPlugin http =
{
	.type = USE_BROWSELIST,
 	.portNumber = 80,
 	.supportsAuthentication = 0,
 	.minimumTouchDepth = 5,
// 	.cleanUp = ShortcutcleanUp,
 	.networkName = HTTPnetworkName,
 	.description = HTTPdescription,
 	.protocol = HTTPprotocol,
        .open = HTTPopen,
	.close = HTTPclose,
 	.read = HTTPread,
	.opendir = HTTPopendir,
 	.readdir = HTTPreaddir,
 	.closedir = HTTPclosedir,
 	.stat = HTTPstat,
// 	.readlink = Shortcutreadlink,
//        .mknod = Shortcutmknod,
//        .unlink = Shortcutunlink,
//        .truncate = Shortcuttruncate,
//        .rmdir = Shortcutrmdir,
//        .mkdir = Shortcutmkdir,
//        .rename = Shortcutrename,
//        .chown = Shortcutchown,
//        .chmod = Shortcutchmod,
//        .utime = Shortcututime,
};

void *giveNetworkPluginInfo( void ) {
	strlcpy( HTTPnetworkName, gettext( "HTTP Network" ), NAME_MAX + 1 );
	return &http;
}