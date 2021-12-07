/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include <dirent.h>
#include <fcntl.h>

#include "syscall.h"

NETWORK_FILE *DNPopen( const char *URL, int flags, mode_t mode, NETWORK_FILE *file, NetworkPlugin *plugin ) {
	int retval;
	int fd;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];

	retval = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( retval == 0 ) {
		file->usePluginSyscall = usePluginSyscall;
		if ( usePluginSyscall ) {
			if ( plugin->open )
				return file->plugin->open( URL, flags, mode, file );
			else {
				errno = ENOSYS;
				return NULL;
			}
		}

		createRealPath( realPath, URL );
		fd = open( realPath, flags, mode );
		if ( fd < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return NULL;
		}

		file->privateData = int_to_void_cast fd;
		return file;
	}

	errno = retval * -1;
	return NULL;
}

int DNPclose( NETWORK_FILE *file, NetworkPlugin *plugin ) {
	if ( file->usePluginSyscall ) {
		if ( plugin->close )
			return file->plugin->close( file );
		else {
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}
	}

	return close( void_to_int_cast file->privateData );
}

NETWORK_DIR *DNPopendir( const char *URL, unsigned int flags, NETWORK_DIR *dir, NetworkPlugin *plugin ) {
	int retval;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	DIR *tmp;

	retval = scanURL( URL, flags | FULL_RESOLVE, &usePluginSyscall );
	if ( retval == 0 ) {
		dir->usePluginSyscall = usePluginSyscall;
		if ( usePluginSyscall ) {
			if ( plugin->opendir )
				return dir->plugin->opendir( URL, flags, dir );
			else {
				errno = ENOSYS;
				return NULL;
			}
		}

		createRealPath( realPath, URL );
		markSyscallStartTime();
		tmp = opendir( realPath );
		if ( !tmp ) {
			handleSyscallError( URL );
			return NULL;
		}

		dir->privateData = ( void * ) tmp;
		return dir;
	}

	errno = retval * -1;
	return NULL;
}

int DNPclosedir( NETWORK_DIR *dir, NetworkPlugin *plugin ) {
	if ( dir->usePluginSyscall ) {
		if ( plugin->closedir )
			return dir->plugin->closedir( dir );
		else {
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}
	}

	return closedir( ( DIR * ) dir->privateData );
}

int DNPreaddir( NETWORK_DIR *dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result, NetworkPlugin *plugin ) {
	struct dirent *tmp;
	char IPBuffer[ 16 ];
	char *ptr;
	char *IP;
	char URL[ PATH_MAX ];
	char buf[ sizeof( struct dirent ) + ( sizeof( char ) * ( NAME_MAX + 1 ) ) ];
	int retval;
	Service *service;

	if ( dir->usePluginSyscall ) {
		if ( plugin->readdir )
			return dir->plugin->readdir( dir, entry, result );
		else {
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}
	}

	retval = readdir_r( ( DIR * ) dir->privateData, ( struct dirent * ) buf, &tmp );
	if ( retval != 0 || !tmp ) {
		*result = NULL;
		return retval;
	}

	entry->type = tmp->d_type;
	strlcpy( entry->name, tmp->d_name, NAME_MAX + 1 );
	entry->comment[ 0 ] = entry->IP[ 0 ] = '\0';

	if ( dir->URL[ strlen( dir->URL ) -1 ] == '/' )
		sprintf( URL, "%s%s", dir->URL, tmp->d_name );
	else
		sprintf( URL, "%s/%s", dir->URL, tmp->d_name );

	service= findListEntry( browselist, URL );
	if ( service ) {
		if ( service->comment )
			strlcpy( entry->comment, service->comment, NAME_MAX + 1 );

		IP = IPtoString( service->IP, IPBuffer, 16 );
		if ( IP )
			strlcpy( entry->IP, IP, 16 );

		if ( service->host && !service->share ) {
			ptr = strrchr( entry->name, '_' );
			if ( ptr )
				*ptr = '\0';
		}

		putListEntry( browselist, service );
	}

	*result = entry;

	return retval;
}

ssize_t DNPread( NETWORK_FILE *file, void *buf, size_t count, NetworkPlugin *plugin ) {
	if ( file->usePluginSyscall ) {
		if ( file-> plugin->read )
			return plugin->read( file, buf, count );
		else {
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}
	}

	return read( void_to_int_cast file->privateData, buf, count );
}

ssize_t DNPwrite( NETWORK_FILE *file, void *buf, size_t count, NetworkPlugin *plugin ) {
	if ( file->usePluginSyscall ) {
		if ( plugin->write )
			return file->plugin->write( file, buf, count );
		else {
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}
	}

	return write( void_to_int_cast file->privateData, buf, count );
}

int DNPstat( const char *URL, struct stat *buf, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->stat )
				return plugin->stat( URL, buf );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		markSyscallStartTime();
		retval = /*l*/stat( realPath, buf );
		if ( retval < 0 ) {
			handleSyscallError( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

off_t DNPlseek( NETWORK_FILE *file, off_t offset, int whence, NetworkPlugin *plugin ) {
	if ( file->usePluginSyscall ) {
		if ( plugin->lseek )
			return plugin->lseek( file, offset, whence );
		else {
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}
	}

	return lseek( void_to_int_cast file->privateData, offset, whence );
}

ssize_t DNPreadlink( const char *URL, char *buf, size_t bufsize, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	ssize_t retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->readlink )
				return plugin->readlink( URL, buf, bufsize );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = readlink( realPath, buf, bufsize - 1 );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
		buf[ retval ] = '\0';
	}

	errno = valid * -1;
	return -errno;
}

int DNPmknod( const char *URL, mode_t mode, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->mknod )
				return plugin->mknod( URL, mode );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = mknod( realPath, mode, 0 );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPunlink( const char *URL, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->unlink )
				return plugin->unlink( URL );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = unlink( realPath );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPtruncate( const char *URL, off_t size, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->truncate )
				return plugin->truncate( URL, size );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = truncate( realPath, size );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPrmdir( const char *URL, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->rmdir )
				return plugin->rmdir( URL );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = rmdir( realPath );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPmkdir( const char *URL, mode_t mode, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->mkdir )
				return plugin->mkdir( URL, mode );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = mkdir( realPath, mode );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPrename( const char *fromURL, const char *toURL, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char fromRealPath[ PATH_MAX ];
	char toRealPath[ PATH_MAX ];
	int retval;

	valid = scanURL( fromURL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->rename )
				return plugin->rename( fromURL, toURL );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( fromRealPath, fromURL );
		createRealPath( toRealPath, toURL );
		retval = rename( fromRealPath, toRealPath );
		if ( retval < 0 ) {
			/*			if ( errno == ENODEV || errno == EIO || errno == EHOSTDOWN || errno == ETIMEDOUT )
							markAsBadURL( URL );*/
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPsymlink( const char *fromURL, const char *toURL, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char fromRealPath[ PATH_MAX ];
	char toRealPath[ PATH_MAX ];
	int retval;

	valid = scanURL( fromURL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->symlink )
				return plugin->symlink( fromURL, toURL );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( fromRealPath, fromURL );
		createRealPath( toRealPath, toURL );
		retval = symlink( fromRealPath, toRealPath );
		if ( retval < 0 ) {
			/*			if ( errno == ENODEV || errno == EIO || errno == EHOSTDOWN || errno == ETIMEDOUT )
			markAsBadURL( URL );*/
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

/*int DNPlink( const char *fromURL, const char *toURL, NetworkPlugin *plugin ) {
	int valid;
	char fromRealPath[ PATH_MAX ];
	char toRealPath[ PATH_MAX ];
	int retval;

	valid = scanURL( fromURL, flags | PARTIAL_RESOLVE );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->link )
				return plugin->link( fromURL, toURL );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( fromRealPath, fromURL );
		createRealPath( toRealPath, toURL );
		retval = link( fromRealPath, toRealPath );
		if ( retval < 0 ) {
			LNCdebug( LNC_INFO, 1, "DNPlink( %s, %s ) -> link( %s, %s )", fromURL, toURL, fromRealPath, toRealPath );
			if ( errno == ENODEV || errno == EIO || errno == EHOSTDOWN || errno == ETIMEDOUT ) {
				LNCdebug( LNC_ERROR, 0, "One of the hosts might be down, rescanning URL ( %s )", fromURL );
				scanURL( fromURL, FORCE_FULL_RESCAN );
			}
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}*/

int DNPchown( const char *URL, uid_t owner, gid_t group, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->chown )
				return plugin->chown( URL, owner, group );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = chown( realPath, owner, group );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPchmod( const char *URL, mode_t mode, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->chmod )
				return plugin->chmod( URL, mode );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = chmod( realPath, mode );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}

int DNPutime( const char *URL, struct utimbuf *buf, NetworkPlugin *plugin ) {
	int valid;
	int usePluginSyscall;
	char realPath[ PATH_MAX ];
	int retval;

	return 0;
	valid = scanURL( URL, PARTIAL_RESOLVE, &usePluginSyscall );
	if ( valid == 0 ) {
		if ( usePluginSyscall ) {
			if ( plugin->utime )
				return plugin->utime( URL, buf );
			else {
				errno = ENOSYS;
				return -EXIT_FAILURE;
			}
		}

		createRealPath( realPath, URL );
		retval = utime( realPath, buf );
		if ( retval < 0 ) {
			//			if ( errno == ENODEV || /*errno == EIO || */errno == EHOSTDOWN || errno == ETIMEDOUT )
			//				markAsBadURL( URL );
			return retval;
		}
	}

	errno = valid * -1;
	return -errno;
}
