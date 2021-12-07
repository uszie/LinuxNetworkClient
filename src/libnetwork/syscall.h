/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

/***************************************************************************
 *   Copyright (C) 2003 by Usarin Heininga                                 *
 *   usarin@heininga.nl                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef __LNC_SYSCALL_H__
#define __LNC_SYSCALL_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "networkclient.h"

	NETWORK_FILE *DNPopen( const char * URL, int flags, mode_t mode, NETWORK_FILE * file, NetworkPlugin *plugin );
	 int DNPclose( NETWORK_FILE * file, NetworkPlugin *plugin );
	 NETWORK_DIR *DNPopendir( const char * URL, unsigned int flags, NETWORK_DIR * dir, NetworkPlugin *plugin );
	 int DNPclosedir( NETWORK_DIR * dir, NetworkPlugin *plugin );
	 int DNPreaddir( NETWORK_DIR * dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result, NetworkPlugin *plugin );
	 ssize_t DNPread( NETWORK_FILE * file, void * buf, size_t count, NetworkPlugin *plugin );
	 ssize_t DNPwrite( NETWORK_FILE * file, void * buf, size_t count, NetworkPlugin *plugin );
	int DNPstat( const char * URL, struct stat * buf, NetworkPlugin *plugin );
	off_t DNPlseek( NETWORK_FILE * file, off_t offset, int whence, NetworkPlugin *plugin );
	ssize_t DNPreadlink( const char * URL, char * buf, size_t bufsize, NetworkPlugin *plugin );
	int DNPmknod( const char * URL, mode_t mode, NetworkPlugin *plugin );
	int DNPunlink( const char * URL, NetworkPlugin *plugin );
	int DNPtruncate( const char * URL, off_t size, NetworkPlugin *plugin );
	int DNPrmdir( const char * URL, NetworkPlugin *plugin );
	int DNPmkdir( const char * URL, mode_t mode, NetworkPlugin *plugin );
	int DNPrename( const char * fromURL, const char * toURL, NetworkPlugin *plugin );
	int DNPsymlink( const char *fromURL, const char *toURL, NetworkPlugin *plugin );
	/*int DNPlink( const char *fromURL, const char *toURL, NetworkPlugin *plugin );*/
	int DNPchown( const char * URL, uid_t owner, gid_t group, NetworkPlugin *plugin );
	int DNPchmod( const char * URL, mode_t mode, NetworkPlugin *plugin );
	int DNPutime( const char * URL, struct utimbuf * buf, NetworkPlugin *plugin );

#ifdef __cplusplus
}
#endif
#endif
