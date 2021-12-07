/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <signal.h>
#include <libintl.h>
#include <pty.h>
#include <wait.h>
#include <time.h>

#define SSH_CONNECTION_TIMEOUT 3000000

int main ( int argc, char *argv[] )
{
	int PID;
	int status;
	int retval;
	int master;
	int i;
	int len;
	int hostarg = 0;
	char username[ BUFSIZ ];
	char password[ BUFSIZ ];
	char buffer[ BUFSIZ ];
	char *ptr;
	char *sep;
	char replyString[ BUFSIZ ];
	char *begin, *end;
	struct timeval time;
	fd_set fdset;

	( void ) argc;

	for ( i = 0; argv[ i ]; ++i )
	{
		if ( !hostarg )
		{
			ptr = strstr ( argv[ i ], ":/" );
			if ( ptr )
				hostarg = i;
		}

		while ( ( ptr = strstr ( argv[ i ], "username=" ) ) )
		{
			snprintf ( username, BUFSIZ, "%s", ptr+9 );
			sep = strchr ( username, ',' );
			if ( sep )
				*sep = '\0';

			if ( hostarg && username[ 0 ] != '\0' )
			{
				sep = strchr ( argv[ hostarg ], '@' );
				if ( sep )
					++sep;
				snprintf ( buffer, BUFSIZ, "%s@%s", username, sep ? sep : argv[ hostarg ] );
				argv[ hostarg ] = strdup ( buffer );
			}

			len = 9 + strlen ( username );
			if ( * ( ptr-1 ) == ',' )
			{
				--ptr;
				++len;
			}
			memmove ( ptr, ptr+len, ( strlen ( ptr+len ) + 1 ) * sizeof ( char ) );
		}

		while ( ( ptr = strstr ( argv[ i ], "password=" ) ) )
		{
			snprintf ( password, BUFSIZ, "%s", ptr+9 );
			sep = strchr ( password, ',' );
			if ( sep )
				*sep = '\0';

			len = 9 + strlen ( password );
			if ( * ( ptr-1 ) == ',' )
			{
				--ptr;
				++len;
			}
			memmove ( ptr, ptr+len, ( strlen ( ptr+len ) + 1 ) * sizeof ( char ) );
		}

	}

	PID = forkpty ( &master, buffer, NULL, NULL );
	if ( PID < 0 )
	{
		perror ( "forkpty()" );
		return ENOMSG;
	}
	else if ( PID == 0 )
	{
		setenv ( "SSH_ASKPASS", "", 1 );
		signal ( SIGHUP, SIG_IGN );

		argv[ 0 ] = "sshfs";
		execvp ( "sshfs", argv );
		perror ( "execvp()" );
		_exit ( EXIT_FAILURE );
	}
	else
	{
		char c;
		int flags;

		flags = fcntl ( master, F_GETFL );
		if ( flags < 0 )
		{
			perror ( "fcntl()" );
			retval = kill ( PID, SIGKILL );
			if ( retval < 0 )
				perror ( "kill()" );
			else
				waitpid ( PID, &status, 0 );
			return EXIT_FAILURE;
		}

		retval = fcntl ( master, F_SETFL, flags | O_NONBLOCK );
		if ( retval < 0 )
		{
			perror ( "fcntl()" );
			retval = kill ( PID, SIGKILL );
			if ( retval < 0 )
				perror ( "kill()" );
			else
				waitpid ( PID, &status, 0 );
			return EXIT_FAILURE;
		}


		FD_ZERO( &fdset );
		FD_SET( master, &fdset );
		time.tv_sec = SSH_CONNECTION_TIMEOUT;
		time.tv_usec = 0;
		memset ( buffer, 0, sizeof ( buffer ) );
		end = ptr = buffer;
		i =0;
		while ( 1 )
		{
			retval = select( master+1, &fdset, NULL, NULL, &time );
			if ( retval < 0 ) {
				if ( errno == EINTR )
					continue;

				goto killAndExit;
			} else if ( retval == 0 ) {
				goto killAndExit;
			}

			retval = read ( master, &c, 1 );
			if ( retval < 0 ) {
				if ( errno == EAGAIN )
					continue;

				goto killAndExit;
			}

			if ( ( time.tv_sec == 0 ) && ( time.tv_usec == 0 ) )
				goto killAndExit;

			if ( c != '\0' )
				*ptr++ = c;
			else
				continue;

			if ( ( begin = strcasestr ( end, "(yes/no)?" ) ) )
			{
				end = begin+10;
				snprintf ( replyString, BUFSIZ, "yes\n" );
				write ( master, replyString, strlen ( replyString ) );
			}

			if ( ( begin = strcasestr ( end, "Password:" ) ) )
			{
				if ( i > 0 )
					goto killAndExit;

				end = begin+10;
				snprintf ( replyString, BUFSIZ, "%s\n", password );
				write ( master, replyString, strlen ( replyString ) );
				++i;
			}
		}

		return EXIT_SUCCESS;
	}

killAndExit:
	retval = kill ( PID, SIGKILL );
	if ( retval < 0 && errno != ESRCH )
		perror ( "kill()" );
	else
	{
		waitpid ( PID, &status, 0 );
		printf ( "%s", buffer );
		if ( WIFEXITED ( status ) )
			return WEXITSTATUS ( status );
		else
			return EXIT_FAILURE;
	}

	return EXIT_FAILURE;
}
