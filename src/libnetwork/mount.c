
#include "pluginhelper.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <mntent.h>

int removeMtabEntry( const char *mountpoint ) {
	int fd;
	FILE *oldMtab;
	FILE *newMtab;
	struct mntent *entry;
	int retval;

	fd = open( MOUNTED"~", O_RDWR|O_CREAT|O_EXCL, 0600 );
	if ( fd < 0 ) {
		perror( "networkmount" );
		return -EXIT_FAILURE;
	}

	close(fd);

	oldMtab= setmntent(MOUNTED, "r");
	if ( !oldMtab) {
		perror( "networkmount" );
		return -EXIT_FAILURE;
	}

	newMtab = setmntent( MOUNTED".tmp", "w" );
	if ( !newMtab) {
		perror( "networkmount" );
		endmntent( oldMtab );
		return -EXIT_FAILURE;
	}

	while ( ( entry = getmntent( oldMtab ) ) ) {
		if ( strcmp( entry->mnt_dir, mountpoint ) != 0 )
			addmntent( newMtab, entry );
	}

	retval = fchmod( fileno( newMtab ), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if ( retval < 0 ) {
		perror( "networkmount" );
		endmntent( oldMtab );
		endmntent( newMtab );
		unlink( MOUNTED".tmp" );
		return -EXIT_FAILURE;
	}

	endmntent( oldMtab );
	endmntent( newMtab );

	retval = rename( MOUNTED".tmp", MOUNTED );
	if ( retval < 0) {
		perror( "networkmount" );
		return -EXIT_FAILURE;
	}

	retval = unlink( MOUNTED"~" );
	if ( retval < 0 )
		perror( "networkmount" );

	return EXIT_SUCCESS;
}

int main ( int argc, char *argv[] )
{
	pid_t pid;
	int retval;
	int i;
	int len;
	int status;
	int validNetworkType = 0;
	int ftpfs;
	char buffer[ PATH_MAX ];

	( void ) argc;
	( void ) argv;

	if ( argc < 5 )
	{
		printf ( "usage: networkmount -t TYPE SERVICE DIRECTORY\n" );
		return EINVAL;
	}

	retval = setuid ( 0 );
	if ( retval < 0 )
	{
		printf("returning \n");
		perror ( "networkmount" );
		return errno;
	}

	ftpfs = 0;
	for ( i = 0; argv[ i ]; ++i )
	{
		if ( ( strcmp ( argv[ i ], "sshfs" ) == 0 ||
		        strcmp ( argv[ i ], "smbfs" ) == 0 ||
		        strcmp ( argv[ i ], "nfs" ) == 0 ||
		        strcmp ( argv[ i ], "cifs" ) == 0 ||
		        strcmp ( argv[ i ], "ftpfs" ) == 0 ||
		        strcmp ( argv[ i ], "ncpfs" ) == 0 ) && strcmp ( argv[ i - 1 ], "-t" ) == 0 )
		{
			validNetworkType = 1;
			if ( strcmp ( argv[ i ], "ftpfs" ) == 0 )
			{
				argv[ i ] = strdup ( "fuse" );
				ftpfs = 1;
			}
		}
	}

	for ( i = 2; argv[ i ]; ++i )
	{
		if ( ftpfs && argv[ i - 1][ 0 ] != '-' )
		{
			snprintf ( buffer, PATH_MAX, "curlftpfs#%s", argv[ i  ] );
			argv[ i ] = strdup ( buffer );
			break;
		}
		else if ( !ftpfs )
			break;
	}

	if ( !validNetworkType )
	{
		fprintf ( stderr, "networkmount: Invalid filesystem type\n" );
		return EINVAL;
	}

	argv[ 0 ] = strdup ( "mount" );
	retval = posix_spawnp ( &pid, argv[ 0 ], NULL, NULL, argv, environ );
	waitpid ( pid, &status, 0 );

	free ( argv[ 0 ] );

	if ( retval < 0 )
	{
		perror ( "networkmount" );
		return retval * -1;
	}

	for ( i = 2; argv[ i ]; ++i ) {
		len = strlen( LNC_ROOT_DIR );
		if ( strncmp( argv[ i ], LNC_ROOT_DIR, len ) == 0 ) {
			removeMtabEntry( argv[ i ] );
			break;
		}
	}

	if ( WIFEXITED ( status ) )
		return WEXITSTATUS ( status );

	return 1;
}

