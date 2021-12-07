/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include <linux/limits.h>
#include <stdlib.h>
#include <signal.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <errno.h>
#include <display.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

#include "networkclient.h"
#include "plugin.h"

static void crashHandler ( int sig );

static struct sigaction oldSIGSEGVAction;
static struct sigaction oldSIGILLAction;
static struct sigaction oldSIGABRTAction;
static struct sigaction oldSIGFPEAction;

static void setupHandlers ( void )
{
	struct sigaction crashAction;

	crashAction.sa_handler = crashHandler;
	sigemptyset ( &crashAction.sa_mask );
	crashAction.sa_flags = 0;
	if ( sigaction ( SIGSEGV, &crashAction, &oldSIGSEGVAction ) < 0 ||
	        sigaction ( SIGILL, &crashAction, &oldSIGILLAction ) < 0 ||
	        sigaction ( SIGABRT, &crashAction, &oldSIGABRTAction ) < 0 ||
	        sigaction ( SIGFPE, &crashAction, &oldSIGFPEAction ) < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "setupHandlers() -> sigaction()" );
	}
}

static void resetHandlers ( void )
{
	if ( sigaction ( SIGSEGV, &oldSIGSEGVAction, NULL ) < 0 ||
	        sigaction ( SIGILL, &oldSIGILLAction, NULL ) < 0 ||
	        sigaction ( SIGABRT, &oldSIGABRTAction, NULL ) < 0 ||
	        sigaction ( SIGFPE, &oldSIGFPEAction, NULL ) < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "resetHandlers() -> sigaction()" );
	}

}

static int isKDE4Session ( const char *display )
{
	DIR *procDIR;
	struct dirent *entry;
	char path[ PATH_MAX ];
	char *buffer, *ptr;
	int len, i, retval, kde4 = 0;
	struct stat statBuf;

	procDIR = opendir ( "/proc" );
	if ( !procDIR )
	{
		return -EXIT_FAILURE;
	}

	while ( ( entry = readdir ( procDIR ) ) )
	{
		snprintf ( path, PATH_MAX, "/proc/%s/cmdline", entry->d_name );
		retval = stat ( path, &statBuf );
		if ( retval != 0 )
			continue;

		len = copyFileToBuffer ( &buffer, path );
		if ( len < 8 )
			continue;

		if ( strncmp ( buffer, "kdeinit4", 8 ) != 0 )
			continue;

		snprintf ( path, PATH_MAX, "/proc/%s/environ", entry->d_name );
		len = copyFileToBuffer ( &buffer, path );
		if ( len < 0 )
			continue;

		for ( i = 0; i < len; i++ )
		{
			if ( buffer[ i ] == '\0' )
				buffer[ i ] = '\n';
		}

		ptr = strstr ( buffer, "DISPLAY=" );
		if ( !ptr || strncmp ( ptr+8, display, 2 ) != 0 )
			continue;

		ptr = strstr ( buffer, "KDE_FULL_SESSION=true" );
		if ( !ptr )
			continue;

		ptr = strstr ( buffer, "KDE_SESSION_VERSION=4" );
		if ( !ptr )
			continue;

		kde4 = 1;
		break;
	}

	closedir ( procDIR );

	return kde4;
}

static void crashHandler ( int sig )
{
	char crashExec[ 100 ];
	char *output;
	int status;
	int activeConsole;
	char *display;
	char *xauthority;

	LNCdebug ( LNC_INFO, 0, "crashHandler(): received signal %s", strsignal ( sig ) );

	display = getActiveDisplayDevice ();
	if ( !display )
	{
		LNCdebug ( LNC_ERROR, 1, "crashHandler( %d ) -> getDisplayDevice( %d )", sig, activeConsole );
		goto out;
	}

	setenv ( "DISPLAY", display, 1 );
	free ( display );

	xauthority = getXauthority ( getpid(), getenv( "DISPLAY" ) );
	if ( xauthority ) {
		setenv ( "XAUTHORITY", xauthority, 1 );
		free ( xauthority );
	}

	if ( isKDE4Session ( display ) )
		sprintf ( crashExec, "kwrapper4 drkonqi --signal %d --appname networkclient --bugaddress usarin@heininga.nl --programname networkclient --pid %d --appversion 0.1", sig, getpid() );
	else
		sprintf ( crashExec, "kwrapper drkonqi --signal %d --appname networkclient --bugaddress usarin@heininga.nl --programname networkclient --pid %d --appversion 0.1", sig, getpid() );

	LNCdebug( LNC_INFO, 0, "crashHandler( %d ): executing \"%s\"", sig, crashExec );
	output = execute ( crashExec, STDOUT_FILENO | STDERR_FILENO, environ, &status );
	if ( status != 0 || !output )
		LNCdebug ( LNC_ERROR, 0, "crashHandler() -> execute( %s ): exit status = %d, output = \"%s\"", crashExec, status, output );

	if ( output )
		free ( output );

out:
	if ( sig != 0 )
	{
		resetHandlers();
		raise ( sig );
	}
}

static int crashInit ( void )
{
	setupHandlers();
//	pthread_atfork ( NULL, NULL, setupHandlers );

	return EXIT_SUCCESS;
}

static void crashCleanUp ( void )
{
	resetHandlers();
}

static const char crashType[] = "Crash Manager";
static const char crashDescription[] = "This plugin intercepts program crashes and shows some information about it";
static const char *crashDependencies[] = { NULL };

static GlobalPlugin crash =
{
	.init = crashInit,
	.cleanUp = crashCleanUp,
	.type = crashType,
	.description = crashDescription,
	.dependencies = crashDependencies,
	.pluginRoutine = NULL,
};

void *giveGlobalPluginInfo ( void )
{
	return & crash;
}
