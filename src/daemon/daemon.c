/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include"config.h"
#include "daemon.h"
#include "networkclient.h"
#include "session.h"
#include "nconfig.h"
#include "display.h"
#include "util.h"
#include "fusefs.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <wait.h>
#include <getopt.h>
#include <libintl.h>
#include <locale.h>
#include <ctype.h>

static struct applicationList __applicationList;
struct applicationList *applicationList = &__applicationList;

static struct libraryList __libraryList;
struct libraryList *libraryList = &__libraryList;

volatile sig_atomic_t exiting = 0;
volatile sig_atomic_t childExited = 0;
sigset_t blockMask;
int useFullNetworkName = 1;
int doFork = 1;
char userName[ NAME_MAX + 1 ];

void printApplications( void *data ) {
	char * ptr = data;
	int i;

	for ( i = 0; i < 6; ++ i ) {
		switch ( i ) {
			case 0:
				printf( "%s", ptr );
				break;
			case 1:
				printf( " with arguments \"%s\"", ptr );
				break;
			case 2:
				printf( ": Disabled \"%s\"", ptr );
				break;
			case 3:
				printf( ", Use fake syscall \"%s\"", ptr );
				break;
			case 4:
				printf( ", Use opendir syscall \"%s\"", ptr );
				break;
			case 5:
				printf( ", Use stat syscall \"%s\"\n", ptr );
				return ;
		}

		ptr = strchr( ptr, '\0' );
		ptr += 1;
	}
}

void applicationPrint( struct application *application ) {
	LNCdebug( LNC_INFO, 0, "application = %s, cmdline = %s, disabled= %d, use fake syscall = %d, use opendir = %d, use stat = %d", application->name, application->cmdLine ? application->cmdLine : "", application->disabled, application->fakeSyscall, ( application->syscall & NETWORKCLIENT_OPENDIR ) ? 1 : 0, ( application->syscall & NETWORKCLIENT_STAT ) ? 1 : 0 );
}

void applicationFree( struct application *application ) {
	free( application->name );
	if ( application->cmdLine )
		free( application->cmdLine );
	free( application );
}

int applicationCompare( struct application *new, struct application *entry ) {
	return strcmp( new->name, entry->name );
}

int applicationFind( const char *search, struct application *entry ) {
	return strcmp( search, entry->name );
}

int isLibrary( const char *filename ) {
	char * ptr;

	ptr = strstr( filename, ".so" );
	if ( !ptr )
		return 0;
	else if ( *( ptr + 3 ) == '\0' )
		return 1;
	else {
		for ( ptr += 3;*ptr != '\0'; ++ptr ) {
			if ( isdigit( *ptr ) || *ptr == '.' )
				continue;
			return 0;
		}
	}

	return 1;
}

int addApplicationEntry( const char *applicationName, const char *string ) {
	char * token;
	char *cpy;
	char *ptr;
	int tokenCount = 1;
	struct application *application;

	application = malloc( sizeof( struct application ) );
	if ( !application ) {
		LNCdebug( LNC_ERROR, 1, "addApplicationEntry( %s, %s ) -> malloc()", applicationName, string );
		return -EXIT_FAILURE;
	}

	application->name = strdup( ( char * ) applicationName );
	application->cmdLine = NULL;
	application->syscall = NETWORKCLIENT_OPENDIR | NETWORKCLIENT_STAT;
	application->isLibrary = 0;
	application->disabled = 0;
	application->fakeSyscall = 0;

	if ( string ) {
		cpy = strdup( string );
		ptr = cpy;
		while ( ( token = strsep( &ptr, "," ) ) ) {
			switch ( tokenCount ) {
				case 1:
					if ( strcasecmp( token, "Null" ) == 0 )
						application->cmdLine = NULL;
					else
						application->cmdLine = strdup( token );
					break;
				case 2:
					if ( strcasecmp( token, "Yes" ) == 0 )
						application->disabled = 1;
					break;
				case 3:
					if ( strcasecmp( token, "Yes" ) == 0 )
						application->fakeSyscall = 1;
					break;
				case 4:
					if ( strcasecmp( token, "No" ) == 0 )
						application->syscall &= ~NETWORKCLIENT_OPENDIR;
					break;
				case 5:
					if ( strcasecmp( token, "No" ) == 0 )
						application->syscall &= ~NETWORKCLIENT_STAT;
					break;
			}
			tokenCount += 1;
		}
		free( cpy );
	}

	application->previousSyscallStartTime = 0;
	application->previousSyscallEndTime = application->previousSyscallStartTime;
	application->syscallStartTime = getCurrentTime();

	if ( isLibrary( applicationName ) ) {
		application->isLibrary = 1;
		addListEntry( libraryList, application );
	} else
		addListEntry( applicationList, application );

	return EXIT_SUCCESS;
}

void freeNetworkClientConfiguration( void ) {
	LNCdebug( LNC_INFO, 0, "freeNetworkClientConfiguration()" );

	clearList( applicationList );
	clearList( libraryList );
}

pthread_once_t objectListInit = PTHREAD_ONCE_INIT;

void initObjectLists( void ) {
	applicationList = initList( applicationList, applicationCompare, applicationFind, applicationFree, applicationPrint, NULL, NULL, NULL );
	libraryList = initList( libraryList, applicationCompare, applicationFind, applicationFree, applicationPrint, NULL, NULL, NULL );
}

void readNetworkClientConfiguration( void ) {
	struct key *key;

	pthread_once( &objectListInit, initObjectLists );
	freeNetworkClientConfiguration();

	LNCdebug( LNC_INFO, 0, "readNetworkClientConfiguration()" );

	initKeys( "networkclient/Applications" );
	rewindKeys( "networkclient/Applications", USER_ROOT );
	while ( ( key = forEveryKey2( "networkclient/Applications", USER_ROOT ) ) ) {
		if ( key->type == LNC_CONFIG_KEY )
			addApplicationEntry( key->name, key->value );
		free( key );
	}
	cleanupKeys( "networkclient/Applications" );

	LNCdebug( LNC_INFO, 0, "Enabled applications:" );
	printList( applicationList );
	LNCdebug( LNC_INFO, 0, "Enabled libraries:" );
	printList( libraryList );
}

int updateNetworkClientConfiguration( const char *path, const char *filename, int event ) {
	static u_int64_t lastTime = 0ULL;
	u_int64_t currentTime;

	( void ) path;
	( void ) filename;

	LNCdebug( LNC_DEBUG, 0, "updateNetworkClientConfiguration(): path = %s, filename = %s, event = %d", path, filename, event );

	if ( event == FAMChanged ||
	        event == FAMCreated ||
	        event == FAMDeleted ) {
		currentTime = getCurrentTime();
		if ( ( lastTime + 500000ULL ) > currentTime )
			return EXIT_SUCCESS;

		lastTime = currentTime;

		readNetworkClientConfiguration();
	}
	return EXIT_SUCCESS;
}

int hasProperArguments( pid_t PID, struct application *application ) {
	char *cmdLine;
	char *args;
	char *ptr;
	char *token;
	int retval = 1;

	if ( !application->cmdLine || application->cmdLine[ 0 ] == '\0' )
		return 1;

	cmdLine = getCommandLineArguments( PID );
	if ( !cmdLine )
		return 0;

	args = strdup( application->cmdLine );

	ptr = args;
	while ( ( token = strsep( &ptr, " " ) ) ) {
		if ( strstr( cmdLine, token ) == NULL ) {
			retval = 0;
			break;
		}
	}

	free( args );
	free( cmdLine );
	return retval;
}

int isDaemonProcess( struct syscallContext *context ) {
	char path[ PATH_MAX ];
	char buffer[ PATH_MAX ];
	int points = 0;
	int size;
	int reference;
	const char *lib[] = { "libX11", NULL
	                    };

	reference = lookupProcessLibrary( context->PID, lib );
	if ( reference == ( ( INT_MAX / 2 ) + 1 ) ) {
		LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ) -> lookupProcessLibrary( %d, libX11 ): this program links to X", context->PID, context->PID );
		return 0;
	} else {
		LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ) -> lookupProcessLibrary( %d, libX11 ): this program doesn't link to X", context->PID, context->PID );
		points += 1;
	}

	snprintf( path, PATH_MAX, "/proc/%d/fd/0", context->PID );
	size = readlink( path, buffer, PATH_MAX );
	if ( size == 0 || size >= ( PATH_MAX - 1 ) ) {
		LNCdebug( LNC_ERROR, 0, "isDaemonProcess( %d ) -> readlink( %s ): symbolic link is invalid", context->PID, path );
		return 0;
	} else if ( size < 0 ) {
		LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ): input doesn't exist", context->PID );
		points += 1;
	} else {
		buffer[ size ] = '\0';
		if ( strcmp( buffer, "/dev/null" ) == 0 ) {
			LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ): input links to /dev/null", context->PID );
			points += 1;
		}
	}

	snprintf( path, PATH_MAX, "/proc/%d/fd/1", context->PID );
	size = readlink( path, buffer, PATH_MAX );
	if ( size == 0 || size >= ( PATH_MAX - 1 ) ) {
		LNCdebug( LNC_ERROR, 0, "isDaemonProcess( %d ) -> readlink( %s ): symbolic link is invalid", context->PID, path );
		return 0;
	} else if ( size < 0 ) {
		LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ): output doesn't exist", context->PID );
		points += 1;
	} else {
		buffer[ size ] = '\0';
		if ( !( strstr( buffer, "/dev/tty" ) || strstr( buffer, "/dev/pts" ) ) ) {
			LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ) -> openConsole( %s ): filename is not a console", context->PID, buffer );
			points += 1;
		}
	}

	snprintf( path, PATH_MAX, "/proc/%d/cwd", context->PID );
	size = readlink( path, buffer, PATH_MAX );
	if ( size == 0 || size >= ( PATH_MAX - 1 ) ) {
		LNCdebug( LNC_ERROR, 0, "isDaemonProcess( %d ) -> readlink( %s ): symbolic link is invalid", context->PID, path );
		return 0;
	} else if ( size < 0 ) {
		LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ): cwd doesn't exist", context->PID );
		points += 1;
	} else {
		buffer[ size ] = '\0';
		if ( strcmp( buffer, "/" ) == 0 ) {
			LNCdebug( LNC_DEBUG, 0, "isDaemonProcess( %d ): cwd links to /", context->PID );
			points += 1;
		}
	}

	return ( points >= 2 ) ? 1 : 0;
}

int networkclientCallback( const char *URL, struct auth *auth ) {
	struct syscallContext*context;
	u_int64_t delay = 500000ULL; /*333333ULL;*/
	u_int64_t currentTime;
	u_int64_t spend;
	int retval = -EXIT_FAILURE;
	unsigned int flags;

	context = getSyscallContext();
	if ( !context->application ) {
		if ( !isDaemonProcess( context ) ) {
			LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ) -> isDaemonProcess( %s ): This application is not a daemon process, enabling", URL, context->applicationName );
			addApplicationEntry( context->applicationName, NULL );
			context->application = findListEntry( applicationList, context->applicationName );
		} else {
			LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ) -> isDaemonProcess( %s ): This application is a daemon process, disabling", URL, context->applicationName );
			addApplicationEntry( context->applicationName, "Null,Yes,Yes,Yes,Yes" );
			context->application = findListEntry( applicationList, context->applicationName );
		}

		if ( !context->application ) {
			LNCdebug( LNC_DEBUG, 1, "networkclientCallback( %s ) -> findListEntry( applicationList, %s )", URL, context->applicationName );
			return -EXIT_FAILURE;
		}

		if ( context->application->disabled ) {
			LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ): This application is not enabled in this configuration ( %s )", URL, context->applicationName );
//			putListEntry( applicationList, context->application );
			return -EXIT_FAILURE;
		}
	}

	if ( context->application->disabled ) {
		LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ): This application is not enabled in this configuration ( %s )", URL, context->applicationName );
//		putListEntry( applicationList, context->application );
		return -EXIT_FAILURE;
	}

	LNCdebug( LNC_INFO, 0, "found entry for %s => %s, %d %d", context->applicationName, context->application->name, context->PID, getpgid( context->PID ) );
	currentTime = getCurrentTime();
	spend = currentTime - context->application->syscallStartTime;
	if ( ( context->application->syscall & context->syscall ) && hasProperArguments( context->PID, context->application ) ) {
		flags = 0;
//		flags = getConnectionParameters();
		if ( ( flags & FORCE_FULL_RESCAN ) ||
		        ( flags & FORCE_RESCAN ) ||
		        ( flags & FORCE_AUTHENTICATION ) ||
		        ( ( context->application->previousSyscallEndTime + delay ) < context->application->syscallStartTime ) ) {
			retval = Dialog( URL, auth, context->PID );
			LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ) -> Dialog( URL, %d ) = %d", URL, context->PID, retval );
			if ( context->syscall == NETWORKCLIENT_STAT )
				context->markForUpdate = 1;
		} else {
			LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ): This syscall is to fast", URL );
			LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ): %llu %llu", URL, context->application->previousSyscallEndTime, context->application->syscallStartTime );
		}
	} else
		LNCdebug( LNC_DEBUG, 0, "networkclientCallback( %s ): This application doesn't have the proper syscall's enabled", URL );

//		putListEntry( applicationList, context->application );

	return retval;
}

void exitHandler( int sig ) {
	LNCdebug( LNC_INFO, 0, "exitHandler(): recieved signal %d ( %s )", sig, strsignal( sig ) );
	exiting = 1;
	if ( fuse )
		fuse_exit( fuse );
}

void childHandler( int sig ) {
	LNCdebug( LNC_INFO, 0, "childHandler(): recieved signal %d ( %s )", sig, strsignal( sig ) );
	childExited = 1;
}

void cleanUp( void ) {
	char path[ PATH_MAX ];

	LNCdebug( LNC_INFO, 0, "cleanup(): cleaning up" );
	snprintf( path, PATH_MAX, "%s/.xauth", realRoot );
	unlink( path );
	freeNetworkClientConfiguration();
	LNCcleanUp();
}

void cleanUpChildren( void ) {
	int status;
	pid_t resultPID;
	Session *session;
	int flags = WNOHANG;

	if ( exiting )
		flags = 0;

	lockList( sessionList, WRITE_LOCK );
	forEachListEntry( sessionList, session ) {
		resultPID = waitpid( session->sessionPID, &status, flags );
		if ( resultPID < 0 )
			LNCdebug( LNC_ERROR, 1, "cleanUpChildren() -> waitpid( \"%d\" )", session->sessionPID );
		else if ( resultPID == 0 )
			continue;
		else {
			if ( WIFSIGNALED( status ) )
				LNCdebug( LNC_INFO, 0, "cleanUpChildren(): session %s received signal %d (%s)", session->userName, WTERMSIG( status ), strsignal( WTERMSIG( status ) ) );
			delListEntryUnlocked( sessionList, session );
		}
	}
	unlockList( sessionList );
}

void mainCleanUp( void ) {
	int retval;

	LNCdebug( LNC_INFO, 0, "mainCleanUp(): cleaning up" );

	clearList( sessionList );
	stopFileMonitor();
	freeAllTSDBuffers();
	retval = unlink( pidPath );
	if ( retval < 0 )
		LNCdebug( LNC_ERROR, 1, "mainCleanUp() -> unlink( %s )", pidPath );

	LNCdebug( LNC_INFO, 0, "Bye" );
	if ( networkclient_stdout )
		fclose( networkclient_stdout );
}

void showUsage( void ) {
	printf( "\nNetworkclient Daemon v" VERSION " -- Linux support for browsing public networks\n" );
	printf( "\nUsage: networkclient [options...]\nValid options are:\n" );
	printf( "    -u,        --user                  Start session for user 'user' \n" );
	printf( "    -f,        --nofork                Don't fork into the background\n" );
	printf( "    -h,        --help                  Print this message\n" );
	printf( "\n" );
}

void parseCommandLineArguments( int argc, char *argv[] ) {
	char opt;
	int index;
	static struct option options[] = {
		                                 { "help", 0, 0, 'h' },
		                                 { "user", 1, 0, 'u' },
		                                 { "nofork", 2, 0, 'f' },
		                                 { 0, 0, 0, 0 }
	                                 };

	while ( ( opt = getopt_long( argc, argv, "hu:f", options, &index ) ) != -1 ) {
		switch ( opt ) {
			case '?':
			case 'h':
				showUsage();
				exit( EXIT_SUCCESS );

			case 'u':
				snprintf( userName, NAME_MAX + 1, "%s", optarg );
				break;
			case 'f':
				doFork = 0;
				break;
		}
	}
}

int modprobeFuseFS( void ) {
	char * output;
	int retval;

	output = execute( "modprobe fuse", STDOUT_FILENO | STDERR_FILENO, environ, &retval );
	if ( !output ) {
		LNCdebug( LNC_ERROR, 1, "modprobeFuseFS() -> execute( modprobe fuse )" );
		return -EXIT_FAILURE;
	}

	if ( strlen( output ) > 1 ) {
		LNCdebug( LNC_ERROR, 0, "modprobeFuseFS() -> execute( modprobe fuse ): %s", output );
		free( output );
		return -EXIT_FAILURE;
	}

	free( output );

	return EXIT_SUCCESS;
}

int main( int argc, char *argv[] ) {
	char buffer[ BUFSIZ ];
	struct sigaction childAction;
	struct sigaction exitAction;
	struct sigaction ignoreAction;
	struct passwd *user;
	sigset_t oldMask;
	Session *session;
	Service tmp;
	char key[ PATH_MAX ];
	char IPBuffer[ 16 ];
	int i;

	tmp.share = strdup( "C on USARIN" );
	tmp.host = strdup( "USARIN" );
	tmp.domain = strdup( "HEININGA" );
	tmp.network = strdup( "SMB" );
	tmp.IP = IPtoNumber ( "192.168.1.3" );
//	printf( "%s\n", IPtoString( tmp.IP, IPBuffer, 16 ) );
//	return 0;
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 1 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 2 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 3 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n\n", createServiceKey( key, PATH_MAX, &tmp, 4 ) );
// 	memset( key, '\0', PATH_MAX );
//
// 	tmp.share = NULL;
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 1 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 2 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 3 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n\n", createServiceKey( key, PATH_MAX, &tmp, 4 ) );
// 	memset( key, '\0', PATH_MAX );
//
// 	tmp.IP = 0;
// 	tmp.host = NULL;
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 1 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 2 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 3 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n\n", createServiceKey( key, PATH_MAX, &tmp, 4 ) );
// 	memset( key, '\0', PATH_MAX );
//
// 	tmp.domain = NULL;
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 1 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 2 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 3 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n\n", createServiceKey( key, PATH_MAX, &tmp, 4 ) );
// 	memset( key, '\0', PATH_MAX );
//
// 	tmp.network = NULL;
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 1 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 2 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n", createServiceKey( key, PATH_MAX, &tmp, 3 ) );
// 	memset( key, '\0', PATH_MAX );
// 	printf( "\"%s\"\n\n", createServiceKey( key, PATH_MAX, &tmp, 4 ) );
// 	memset( key, '\0', PATH_MAX );
//
// 	return 0;

// 	for ( i = 0; i < 50000000; ++i ) {
// 		createServiceKey( key, PATH_MAX, &tmp, 2 );
// 	}
// 	return 0;
	setlocale( LC_ALL, "" );
	textdomain( PACKAGE );
	bindtextdomain( PACKAGE, LOCALE_DIR );

	parseCommandLineArguments( argc, argv );

	sigemptyset( &blockMask );
	sigaddset( &blockMask, SIGINT );
	sigaddset( &blockMask, SIGHUP );
	sigaddset( &blockMask, SIGTERM );
	sigaddset( &blockMask, SIGQUIT );
	sigaddset( &blockMask, SIGABRT );

	exitAction.sa_handler = exitHandler;
	sigemptyset( &( exitAction.sa_mask ) );
	sigaddset( &( exitAction.sa_mask ), SIGINT );
	sigaddset( &( exitAction.sa_mask ), SIGHUP );
	sigaddset( &( exitAction.sa_mask ), SIGTERM );
	sigaddset( &( exitAction.sa_mask ), SIGQUIT );
	sigaddset( &( exitAction.sa_mask ), SIGABRT );
	exitAction.sa_flags = 0;

	childAction.sa_handler = childHandler;
	sigemptyset( &childAction.sa_mask );
	childAction.sa_flags = 0;

	ignoreAction.sa_handler = SIG_IGN;
	sigemptyset( &ignoreAction.sa_mask );
	ignoreAction.sa_flags = 0;

	if ( sigaction( SIGHUP, &exitAction, NULL ) < 0 ||
	        sigaction( SIGINT, &exitAction, NULL ) < 0 ||
	        sigaction( SIGQUIT, &exitAction, NULL ) < 0 ||
	        sigaction( SIGTERM, &exitAction, NULL ) < 0 ||
	        sigaction( SIGPIPE, &ignoreAction, NULL ) < 0 ) {
		LNCdebug( LNC_ERROR, 1, "main() -> sigaction()" );
		return -EXIT_FAILURE;
	}

	if ( userName [ 0 ] != '\0' ) {
		sigprocmask( SIG_SETMASK, &blockMask, &oldMask );

		user = getSessionInfoByName( userName, buffer );
		if ( !user ) {
			LNCdebug( LNC_ERROR, 0, "main() -> getSessionInfoByName( %s ): User doesn't exist, exiting", userName );
			sigprocmask( SIG_UNBLOCK, &blockMask, NULL );
			exit( -EXIT_FAILURE );
		}
		session = sessionNew( user->pw_uid, user->pw_gid, user->pw_name, user->pw_dir );
		spawnSession( session );
	} else {
		if ( doFork ) {
			if ( daemon( 0, 1 ) < 0 ) {
				LNCdebug( LNC_ERROR, 1, "main() -> daemon()" );
				return -EXIT_FAILURE;
			}
		}

		if ( modprobeFuseFS() < 0 )
			return -EXIT_FAILURE;

		sigaddset( &blockMask, SIGCHLD );
		sigprocmask( SIG_SETMASK, &blockMask, &oldMask );
		if ( sigaction( SIGCHLD, &childAction, NULL ) < 0 ) {
			LNCdebug( LNC_ERROR, 1, "main() -> sigaction()" );
			return -EXIT_FAILURE;
		}

		sprintf( pidPath, "%snetworkclient.pid", _PATH_VARRUN );
		pidFile = fopen( pidPath, "w+" );
		if ( !pidFile ) {
			LNCdebug( LNC_ERROR, 1, "main() -> fopen( %s, \"w+\" )", pidPath );
			return -EXIT_FAILURE;
		}
		fprintf( pidFile, "%d", getpid() );
		fclose( pidFile );

		createSessionList();
		updateSessions( NULL, NULL, 1 );
		addFileToMonitorList( "/var/run/utmp", updateSessions );

		while ( 1 ) {
			sigsuspend( &oldMask );
			if ( childExited ) {
				cleanUpChildren();
				childExited = 0;
			} else if ( exiting )
				break;
		}
		sigdelset( &blockMask, SIGCHLD );
		sigprocmask( SIG_SETMASK, &blockMask, &oldMask );
		mainCleanUp();
	}

	sigprocmask( SIG_UNBLOCK, &blockMask, NULL );
	exit( EXIT_SUCCESS );
}
