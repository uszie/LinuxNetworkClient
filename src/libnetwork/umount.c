
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

int main ( int argc, char *argv[] )
{
	pid_t pid;
	int retval;
	int i;
	int status;
	int validNetworkType = 0;

	( void ) argc;
	( void ) argv;

	retval = setuid ( 0 );
	if ( retval < 0 )
	{
		perror ( "networkumount" );
		return errno;
	}

	for ( i = 0; argv[i]; ++i )
	{
		if ( ( strcmp ( argv[ i ], "smbfs" ) == 0 ||
		        strcmp ( argv[ i ], "nfs" ) == 0 ||
		        strcmp ( argv[ i ], "cifs" ) == 0 ||
		        strcmp ( argv[ i ], "ncpfs" ) == 0 ||
		        strcmp ( argv[ i ], "ftpfs" ) == 0 ||
		        strcmp ( argv[ i ], "sshfs" ) == 0 ) && strcmp ( argv[ i - 1 ], "-t" ) == 0 )
		{
			validNetworkType = 1;
		}
	}

	if ( !validNetworkType )
	{
		fprintf ( stderr, "networkumount: Invalid filesystem type\n" );
		return EINVAL;
	}

	argv[ 0 ] = strdup ( "umount" );
	retval = posix_spawnp ( &pid, argv[0], NULL, NULL, argv, environ );
	waitpid ( pid, &status, 0 );
	free ( argv[ 0 ] );

	if ( retval < 0 )
	{
		perror ( "networkumount" );
		return retval * -1;
	}

	if ( WIFEXITED ( status ) )
		return WEXITSTATUS ( status );

	return 1;
}

