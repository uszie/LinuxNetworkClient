/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_NETWORKCLIENT_H__
#define __LNC_NETWORKCLIENT_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "user.h"
#include "nconfig.h"

#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>

#define PROTOCOLS_MAX 10

#define forEachProtocol( protocol ) forEachListEntry( networkPluginList, protocol )
#define lockProtocolList() lockList( networkPluginList, READ_LOCK )
#define unlockProtocolList() unlockList( networkPluginList )

	typedef struct network_file NETWORK_FILE;
	typedef struct network_dir NETWORK_DIR;
	typedef struct network_dirent NETWORK_DIRENT;

	extern struct timeval LNCStart;
	extern char *enabledPlugins;
	extern int networkPluginsOnly;
	extern int ( *LNCauthorizationCallback ) ( const char * URL, struct auth * auth );

	int LNCinit( int loadNetworkPluginsOnly, const char *pluginsToLoad  );
	void LNCcleanUp( void );

	int LNCaddManualHost( const char *URL, const char *name );
	int LNCdelManualHost( const char *URL );
// 	void setConnectionParameters( unsigned int flags );
// 	unsigned int getConnectionParameters( void );
// 	void clearConnectionParameters( void );

	int getDepth( const char * URL );
	char *getProtocolType( const char *URL, char *buffer, size_t size );

	NETWORK_FILE *LNCopen( const char * URL, unsigned int flags, mode_t mode );
	int LNCclose( NETWORK_FILE * file );
	NETWORK_DIR *LNCopendir( const char * URL, unsigned int flags );
	int LNCclosedir( NETWORK_DIR * dir );
	int LNCreaddir( NETWORK_DIR * dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result );
	ssize_t LNCread( NETWORK_FILE * file, void * buf, size_t count );
	ssize_t LNCwrite( NETWORK_FILE * file, void * buf, size_t count );
	int LNCstat( const char * URL, struct stat * buf );
	off_t LNClseek( NETWORK_FILE * file, off_t offset, int whence );
	ssize_t LNCreadlink( const char * URL, char * buf, size_t bufsize );
	int LNCmknod( const char * URL, mode_t mode );
	int LNCunlink( const char * URL );
	int LNCtruncate( const char * URL, off_t size );
	int LNCrmdir( const char * URL );
	int LNCmkdir( const char * URL, mode_t mode );
	int LNCrename( const char * oldURL, const char * newURL );
	 int LNCsymlink( const char * fromURL, const char * toURL );
/*		 int LNClink( const char * fromURL, const char * toURL );*/
	int LNCchown( const char * URL, uid_t owner, gid_t group );
	int LNCchmod( const char * URL, mode_t mode );
	int LNCutime( const char * URL, struct utimbuf * buf );

#include "plugin.h"

	struct network_file {
		char URL[ PATH_MAX ];
		void *privateData;
		NetworkPlugin *plugin;
		int depth;
		int usePluginSyscall;
	};

	struct network_dir {
		char URL[ PATH_MAX ];
		void *privateData;
		NetworkPlugin *plugin;
		int depth;
		int usePluginSyscall;
	};

	struct network_dirent {
		char name[ NAME_MAX + 1 ];
		char comment[ NAME_MAX + 1 ];
		char IP[ 16 ];
		int type;
//		int size;
	};

#ifdef __cplusplus
}
#endif
#endif
