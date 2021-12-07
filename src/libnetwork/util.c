/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "nconfig.h"
#include "util.h"
#include "dir.h"
#include "user.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>
#include <signal.h>
#include <pwd.h>

static void *threadWrapper( void *arg );

u_int64_t getCurrentTime( void ) {
	struct timeval __t__;

	gettimeofday( &__t__, NULL );
	return ( ( u_int64_t ) __t__.tv_sec * 1000000ULL ) + ( u_int64_t ) __t__.tv_usec;
}

pthread_once_t timeInit = PTHREAD_ONCE_INIT;
u_int64_t startTime;

void initTime( void ) {
	startTime = getCurrentTime();
}

u_int32_t getProgramTime( void ) {
	u_int64_t currentTime;

	pthread_once( &timeInit, initTime );
	currentTime = getCurrentTime();

	return ( u_int32_t ) ( ( currentTime - startTime ) / 10000 );
}

char *strdup_len( const char *src, int len ) {
	size_t size = ( len + 1 ) * sizeof( char );
	char *new = malloc( size );

	memmove( new, src, size - 1 );
	new[ size - 1 ] = '\0';

	return new;
}

char *strcpy_len( char *dest, const char *src, int len ) {
	memmove( dest, src, len * sizeof( char ) );
	dest[ len ] = '\0';

	return dest;
}

size_t strlcpy( char *dst, const char *src, size_t size )
{
	char *ptr;
	size_t srcSize;

	ptr = memccpy( dst, src, '\0', size );
	if ( ptr ) {
		srcSize = ( ptr - dst ) - 1;
		dst[ srcSize ] = '\0';
	} else {
		dst[ size - 1 ] = '\0';
		srcSize = size + strlen( src+ size );
	}

	return srcSize;
}

char *replaceCharacter( char *string, char which, char with ) {
	char * ptr = string;

	while ( *ptr ) {
		if ( *ptr == which )
			* ptr = with;
		ptr++;
	}

	return string;
}

char *replaceString( char *string, char *which, char *with ) {
	char *ptr;
	char *oldptr;
	char *buffer;
	int whichLen = strlen( which );
	int withLen = strlen( with );
	unsigned int len;

	if ( withLen == whichLen ) {
		ptr = string;
		while ( ( ptr = strstr( ptr, which ) ) ) {
			memmove( ptr, with, withLen * sizeof( char ) );
			ptr += withLen;
		}
	} else if ( withLen < whichLen ) {
		ptr = string;
		while ( ( ptr = strstr( ptr, which ) ) ) {
			strlcpy( ptr, with, withLen );
			memmove( ptr+withLen, ptr+whichLen, ( strlen( ptr+whichLen ) + 1 ) * sizeof( char ) );
			ptr += withLen;
		}
	} else {

		len = ( strlen( string ) + withLen ) * 2;
		buffer = malloc( len * sizeof( char ) );
		if ( !buffer ) {
			LNCdebug( LNC_ERROR, 1, "replaceString( %s, %s, %s ) -> malloc()", string, which, with );
			return NULL;
		}


		buffer[ 0 ] = '\0';
		ptr = string;
		oldptr = string;

		while ( ( ptr = strstr( ptr, which ) ) ) {
			if ( ( strlen( buffer ) + strlen( ptr ) + withLen + 1 ) > len ) {
				buffer = realloc( buffer, ( ( len * 2 ) + 1 ) * sizeof( char ) );
				len = len * 2;
			}

			strncat( buffer, oldptr, ptr -oldptr );
			strcat( buffer, with );
			oldptr = ptr = ptr + whichLen;

		}
		if ( *oldptr != '\0' )
			strcat( buffer, oldptr );
		strcpy( string, buffer );
		free( buffer );
	}


	return string;
}

int64_t charToLong( char *string ) {
	int64_t isLongLong;
	char *remain;

	if ( ( isLongLong = strtoll( string, &remain, 10 ) ) && ( *remain == '\0' ) )
		return isLongLong;
	return ( int64_t ) 0;
}

unsigned int unpackToInt( unsigned char *src ) {
	unsigned int bitCounter = 1;
	unsigned int tmp = 0;
	int i;

	for ( i = 3; i >= 0; i-- ) {
		for ( bitCounter = 01; !( bitCounter & 0100000000 ); bitCounter <<= 1 ) {
			if ( src[ i ] & bitCounter )
				tmp |= bitCounter;
		}

		if ( i > 0 )
			tmp <<= 8;
	}

	return tmp;
}

void packToChar( unsigned char *dest, unsigned int src ) {
	unsigned int bitCounter = 1;
	unsigned int tmp = src;
	int i;

	memset( dest, 0, 4 );
	for ( i = 0; i < 4; i++ ) {
		for ( bitCounter = 01; !( bitCounter & 0100000000 ); bitCounter <<= 1 ) {
			if ( tmp & bitCounter )
				dest[ i ] |= bitCounter;
		}

		tmp >>= 8;
		if ( tmp <= 0 )
			break;
	}
}

FILE *networkclient_stdout = NULL;

#define INIT_VAL 9999999
pthread_once_t debugInit = PTHREAD_ONCE_INIT;

pid_t PID = -1;

void initDebug( void ) {
	struct passwd * pw;
	char *buffer;
	int retval;
	int size;
	static char logFile[ PATH_MAX ];

	size = sysconf( _SC_GETPW_R_SIZE_MAX );
	if ( size < 0 )
		size = BUFSIZ;

	buffer = malloc( sizeof( struct passwd ) + size );
	if ( !buffer ) {
		fprintf( stderr, "LNCdebug() -> malloc()\n" );
		return ;
	}

	getpwuid_r( geteuid(), ( struct passwd * ) buffer, buffer + sizeof( struct passwd ), size, &pw );
	if ( !pw ) {
		free( buffer );
		fprintf( stderr, "LNCdebug() -> getpwuid_r( %d ): %s\n", geteuid(), strerror( errno ) );
		return ;
	}

	sprintf( logFile, "%s/.LNC", pw->pw_dir );
	free( buffer );
	retval = mkdir( logFile, 0755 );
	if ( retval < 0 && errno != EEXIST ) {
		fprintf( stderr, "LNCdebug() -> mkdir( %s, 0755 ): %s\n", logFile, strerror( errno ) );
		return ;
	}

	strcat( logFile, "/networkclient.log" );
	networkclient_stdout = fopen( logFile, "a+" );
	if ( !networkclient_stdout ) {
		fprintf( stderr, "LNCdebug() -> fopen( %s,  a+ ): %s", logFile, strerror( errno ) );
		return ;
	}

	PID = getpid();
}

void __LNCdebug( int debugLevel, int useErrno, const char *format, ... ) {
	int savedErrno = errno;
	char buf[ BUFSIZ ];
	va_list arg;

	if ( debugLevel > networkConfig.LogLevel )
		return ;

	pthread_once( &debugInit, initDebug );
	if ( PID == 0 || PID == -1 ) {
		va_start( arg, format );
		printf( "%d: ", getpid() );
		vprintf( format, arg );
		printf( "\n" );
		if ( PID == -1 ) {
			printf( "%d: __LNCdebug() -> initDebug(): failed to init debug routine\n", getpid() );
			PID = 0;
		}
		va_end( arg );
		return ;
	}

	va_start( arg, format );
	if ( useErrno == 0 ) {
		fprintf( networkclient_stdout, "%d: ", PID );
		vfprintf( networkclient_stdout, format, arg );
		fprintf( networkclient_stdout, "\n" );
	} else {
		fprintf( networkclient_stdout, "%d: ", PID );
		vfprintf( networkclient_stdout, format, arg );
		fprintf( networkclient_stdout, ": " );
		fprintf( networkclient_stdout, "%s", strerror_r( savedErrno, buf, BUFSIZ ) );
		fprintf( networkclient_stdout, "\n" );
	}
	va_end( arg );
	fflush( networkclient_stdout );
	errno = savedErrno;
}

char *execute( char *prog, unsigned int mode, char **environment, int *exitStatus ) {
	int pipefd_out[ 2 ], pipefd_err[ 2 ];
	int retval, status;
	int flags;
	size_t size_stdout = 0, size_stderr = 0;
	char stdoutbuf[ BUFSIZ ] = "", stderrbuf[ BUFSIZ ] = "";
	char *output;
	char *args[ 100 ];
	char string[ BUFSIZ ] = "";
//	static pthread_key_t executeKey = 0;
	pid_t pid;
	posix_spawnattr_t attrp;
	sigset_t signalMask, emptyMask;
	posix_spawn_file_actions_t actions;

	*exitStatus = -EXIT_FAILURE;

	posix_spawn_file_actions_init ( &actions );
	if ( mode & STDOUT_FILENO ) {
		retval = pipe( pipefd_out );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "exec_prog( %s, %d ) -> pipe()", prog, mode );
			return NULL;
		}
		posix_spawn_file_actions_adddup2( &actions, pipefd_out[ 1 ], STDOUT_FILENO );
	}

	if ( mode & STDERR_FILENO ) {
		retval = pipe( pipefd_err );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "exec_prog( %s, %d ) -> pipe()", prog, mode );
			return NULL;
		}
		posix_spawn_file_actions_adddup2( &actions, pipefd_err[ 1 ], STDERR_FILENO );
	}

	sigfillset( &signalMask );
	sigdelset( &signalMask, SIGKILL );
	sigdelset( &signalMask, SIGSTOP );
	__sigdelset( &signalMask, 65 );
	sigemptyset( &emptyMask );

	posix_spawnattr_init( &attrp );
	posix_spawnattr_setsigdefault( &attrp, &signalMask );
	posix_spawnattr_setsigmask( &attrp, &emptyMask );
	posix_spawnattr_setflags( &attrp, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK );
	argumentParser( strcpy( string, prog ), args );

	retval = posix_spawnp( &pid, args[ 0 ], &actions, &attrp, args, environment );
	posix_spawnattr_destroy( &attrp );
	posix_spawn_file_actions_destroy( &actions );
	waitpid( pid, &status, 0 );

	if ( mode & STDOUT_FILENO ) {
		close( pipefd_out[ 1 ] );

		flags = fcntl( pipefd_out[ 0 ], F_GETFL );
		if ( flags < 0 ) {
			LNCdebug( LNC_ERROR, 1, "execute( %s, %d ) -> fcntl( pipefd_out[ 0 ], F_GETFL )", prog, mode );
			return NULL;
		}

		retval = fcntl( pipefd_out[ 0 ], F_SETFL, flags | O_NONBLOCK  );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "execute( %s, %d ) -> fcntl( pipefd_out[ 0 ], F_SETFL )", prog, mode );
			return NULL;
		}

		size_stdout = read( pipefd_out[ 0 ], stdoutbuf, BUFSIZ );
		close( pipefd_out[ 0 ] );
		if ( size_stdout == BUFSIZ ) {
			LNCdebug( LNC_ERROR, 0, "execute( %s, %d ) -> read( pipefd_out ): Output too big\n", prog, mode );
			return NULL;
		}
	}

	if ( mode & STDERR_FILENO ) {
		close( pipefd_err[ 1 ] );

		flags = fcntl( pipefd_err[ 0 ], F_GETFL );
		if ( flags < 0 ) {
			LNCdebug( LNC_ERROR, 1, "execute( %s, %d ) -> fcntl( pipefd_err[ 0 ], F_GETFL )", prog, mode );
			return NULL;
		}

		retval = fcntl( pipefd_err[ 0 ], F_SETFL, flags | O_NONBLOCK  );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "execute( %s, %d ) -> fcntl( pipefd_err[ 0 ], F_SETFL )", prog, mode );
			return NULL;
		}

		size_stderr = read( pipefd_err[ 0 ], stderrbuf, BUFSIZ );
		close( pipefd_err[ 0 ] );
		if ( size_stderr == BUFSIZ ) {
			LNCdebug( LNC_ERROR, 0, "execute( %s, %d ) -> read( pipefd_err ): Output too big\n", prog, mode );
			return NULL;
		}
	}

//	output = ( char * ) createTSDBuffer( 2 * BUFSIZ * sizeof( char ), &executeKey, 1 );
	output = malloc( 2 * BUFSIZ * sizeof( char ) );
	if ( !output ) {
//		LNCdebug( LNC_ERROR, 1, "execute( %s, %d ) -> createTSDBuffer( %d )", prog, mode, 2 * BUFSIZ );
		LNCdebug( LNC_ERROR, 1, "execute( %s, %d ) -> malloc( %d )", prog, mode, 2 * BUFSIZ * sizeof( char ) );
		return NULL;
	}
	sprintf( output, "%s%s", stdoutbuf, stderrbuf );

	if ( WIFEXITED( status ) ) {
		*exitStatus = WEXITSTATUS( status );
		if ( *exitStatus == 127 ) {
			LNCdebug( LNC_ERROR, 0, "execute( %s, %d ) -> posix_spawnp( ): Failed to execute \"%s\"", prog, mode, prog );
			free( output );
			return NULL;
		}
	}
	return output;
}

struct thread {
	void *( *start_routine ) ( void * );
	void *arg;
	int mode;
};

static void *threadWrapper( void *arg ) {
	struct thread * ptr = ( struct thread * ) arg;
	void *retval;

	retval = ptr->start_routine( ptr->arg );
	free( arg );

	return retval;
}

void *createThread( void *( *start_routine ) ( void * ), void *arg, int mode ) {
	pthread_attr_t tattr;
	pthread_t tid;
	int retval;
	void *result;
	struct thread *data;

	retval = pthread_attr_init( &tattr );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "create_thread() -> pthread_attr_init()" );
		return NULL;
	}

	//	retval = pthread_attr_setstacksize( &tattr, PTHREAD_STACK_MIN * 16/*128*/ );
	//	if ( retval < 0 )
	//		LNCdebug( LNC_ERROR, 1, "createThread() -> pthread_attr_setstacksize( %d )", PTHREAD_STACK_MIN * 16/*128*/ );

	if ( mode == DETACHED ) {
		retval = pthread_attr_setdetachstate( &tattr, PTHREAD_CREATE_DETACHED );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "create_thread() -> pthread_attr_setdetachstate()" );
			return NULL;
		}
	}

	data = malloc( sizeof( struct thread ) );
	data->start_routine = start_routine;
	data->arg = arg;
	data->mode = mode;

	retval = pthread_create( &tid, &tattr, threadWrapper, ( void * ) data );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "create_thread() -> pthread_create()" );
		pthread_attr_destroy( &tattr );
		free( data );
		return NULL;
	}

	pthread_attr_destroy( &tattr );

	if ( mode == ATACHED ) {
		retval = pthread_join( tid, &result );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "create_thread() -> pthread_join()" );
			return NULL;
		}
	}

	return ( mode == ATACHED ) ? result : ( void * ) tid;
}

void signalThread( pthread_t *tid, int signal ) {
	pthread_kill( *tid, signal );
}

void killThread( pthread_t *tid ) {
	pthread_kill( *tid, SIGTERM );
}

int argumentParser( char *string, char *args[] ) {
	int i, j, cnt;
	int state = 0;

	for ( i = j = cnt = 0; string[ i ]; i++ ) {
		if ( string[ i ] == '\"' ) {
			if ( state == 0 )
				state = 1;
			else
				state = 0;
		}

		if ( string[ i ] == ' ' && state == 0 ) {
			if ( string[ i - 1 ] == '\"' ) {
				string[ i - 1 ] = '\0';
				args[ cnt++ ] = string + j + 1;
			} else {
				string[ i ] = '\0';
				args[ cnt++ ] = string + j;
			}
			j = i + 1;
		}
	}

	if ( string[ i - 1 ] == '\"' ) {
		string[ i - 1 ] = '\0';
		args[ cnt++ ] = string + j + 1;
	} else {
		string[ i ] = '\0';
		args[ cnt++ ] = string + j;
	}
	args[ cnt ] = NULL;

	return EXIT_SUCCESS;
}

pthread_once_t TSDListInit = PTHREAD_ONCE_INIT;
pthread_mutex_t TSDMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t allocatedTSDList[ PTHREAD_KEYS_MAX ];;

void initTSDList( void ) {
	int i;

	for ( i = 0; i < PTHREAD_KEYS_MAX; ++i )
		allocatedTSDList[ i ] = 0;
}

void *createTSDBuffer( size_t size, pthread_key_t *key, int forceAllocation ) {
	void * buffer;
	int retval;
	int i;

	pthread_once( &TSDListInit, initTSDList );

	retval = pthread_mutex_lock( &TSDMutex );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "createTSDBuffer() -> pthread_mutex_lock()" );
		return NULL;
	}

	if ( *key == 0 ) {
		retval = pthread_key_create( key, free );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "createTSDBuffer() -> pthread_key_create()" );
			pthread_mutex_unlock( &TSDMutex );
			return NULL;
		}

		for ( i = 0; allocatedTSDList[ i ] != 0; ++i )
			;
		allocatedTSDList[ i ] = *key;
	}
	pthread_mutex_unlock( &TSDMutex );

	buffer = pthread_getspecific( *key );
	if ( !buffer && forceAllocation ) {
		buffer = malloc( size );
		if ( !buffer ) {
			LNCdebug( LNC_ERROR, 1, "createTSDBuffer() -> malloc()" );
			return NULL;
		}

		retval = pthread_setspecific( *key, buffer );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "createTSDBuffer() -> pthread_setspecific()" );
			free( buffer );
			return NULL;
		}
	}

	return buffer;
}

void freeTSDBuffers( void *ignore ) {
	int i;
	char *buffer;

	( void ) ignore;

	pthread_once( &TSDListInit, initTSDList );

	for ( i = 0; ( i < PTHREAD_KEYS_MAX ) && ( allocatedTSDList[ i ] != 0 ); ++i ) {
		buffer = pthread_getspecific( allocatedTSDList[ i ] );
		if ( !buffer )
			continue;

		free( buffer );
	}
}

void freeAllTSDBuffers( void ) {
	int i;
	char *buffer;

	pthread_once( &TSDListInit, initTSDList );

	for ( i = 0; ( i < PTHREAD_KEYS_MAX ) && ( allocatedTSDList[ i ] != 0 ); ++i ) {
		buffer = pthread_getspecific( allocatedTSDList[ i ] );
		if ( !buffer ) {
			pthread_key_delete( allocatedTSDList[ i ] );
			continue;
		}

		free( buffer );
		pthread_setspecific( allocatedTSDList[ i ], NULL );
		pthread_key_delete( allocatedTSDList[ i ] );
	}
}

char *getProcessName( pid_t PID ) {
	char path[ PATH_MAX ];
	char *ptr;
	char *buffer;
	char *name;
	char *result;
	int len;
	static pthread_key_t processNameKey = 0;

	sprintf( path, "/proc/%d/cmdline", PID );
	len = copyFileToBuffer( &buffer, path );
	if ( len < 0 ) {
		LNCdebug( LNC_ERROR, 1, "getProcessName( %d ) -> copyFileToBuffer( %s )", PID, path );
		return NULL;
	}

	name = NULL;
	ptr = strstr( buffer, "[kdeinit]" );
	if ( ptr && ( ( ptr - 1 ) > buffer ) ) {
		* ( ptr - 1 ) = '\0';
		ptr = strstr( buffer, ": " );
		if ( ptr )
			name = ptr + 2;
	}

	if ( !name ) {
		ptr = strrchr( buffer, '/' );
		if ( !ptr )
			name = buffer;
		else
			name = ptr + 1;
	}

	result = createTSDBuffer( ( NAME_MAX + 1 ) * sizeof( char ), &processNameKey, 1 );
	if ( !result ) {
		LNCdebug( LNC_ERROR, 1, "getProcessName( %d ) -> createTSDBuffer()", PID );
		free( buffer );
		return NULL;
	}

	snprintf( result, NAME_MAX + 1, "%s", name );
	free( buffer );

	return result;
}

/*void *TSDmalloc( size_t size ) {
	pthread_key_t key = 0;
	size_t sizeExtra;
	void *buffer;

	sizeExtra = sizeof( pthread_key_t ) + sizeof( char );
	buffer = createTSDBuffer( size + sizeExtra, &key );
	if ( !buffer ) {
		LNCdebug( LNC_ERROR, 0, "TSDmalloc() -> createTSDBuffer(): failed to create buffer" );
		return 0;
	}
	sprintf( buffer, "%u", key );

	return buffer + sizeExtra;
}

char *TSDstrdup( const char *string ) {
	size_t size;
	char *cpy;

	size = ( strlen( string ) + 1 ) * sizeof( char );
	cpy = TSDmalloc( size );
	if ( !cpy )
		return NULL;

	return strcpy( cpy, string );
}

void TSDfree( void *buffer ) {
	pthread_key_t key = 10;
	size_t size;
	void *realBuffer;
	int retval;

	pthread_once( &mutexCreated, createMutex );

	retval = pthread_mutex_lock( &TSDLock );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "TSDfree() -> pthread_mutex_lock()" );
		return ;
	}

	size = sizeof( pthread_key_t ) + sizeof( char );
	realBuffer = buffer - size;
	key = strtol( realBuffer, NULL, 10 );
	free( realBuffer );
	retval = pthread_key_delete( key );
	if ( retval < 0 )
		LNCdebug( LNC_ERROR, 1, "TSDfree() -> pthread_key_create()" );

	pthread_mutex_unlock( &TSDLock );
}*/
