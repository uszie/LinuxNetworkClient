//
// C++ Implementation: %{MODULE}
//
// Description:
//
//
// Author: %{AUTHOR} <%{EMAIL}>, (C) %{YEAR}
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include <kconfig.h>
#include "konq_dcop.h"
#include "util.h"
#include <dcopclient.h>
#include <X11/Xlib.h>
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <wait.h>
#include <stdlib.h>
#include <kglobal.h>
#include <kdebug.h>
#include <kio/global.h>

int getActiveKonquerorWindow( void ) {
	DCOPClient client;
	QCString addr( 0 );

	client.setServerAddress( addr );
	bool success = client.attach();
	if ( !success ) {
		LNCdebug( LNC_ERROR, 0, "getActiveKonquerorWindow() -> client.attach(): Couldn't attach to DCOP server" );
		return -EXIT_FAILURE;
	}

	QCStringList apps = client.registeredApplications();
	for ( QCStringList::Iterator it = apps.begin(); it != apps.end(); ++it ) {
		QCString &clientId = *it;
		if ( strncmp( clientId.data(), "konqueror", 9 ) != 0 )
			continue;

		QCStringList objs = client.remoteObjects( clientId, 0 );
		for ( QCStringList::Iterator jt = objs.begin(); jt != objs.end(); ++jt ) {
			QCString &objId = *jt;
			if ( strncmp( objId.data(), "konqueror-mainwindow", 20 ) != 0 )
				continue;

			QByteArray data, replyData;
			QCString replyType;
			QDataStream arg( data, IO_WriteOnly );

			arg << 5;

			if ( !client.call( clientId.data(), objId.data(), "isActiveWindow()", data, replyType, replyData ) ) {
				LNCdebug( LNC_ERROR, 0, "getActiveKonquerorWindow() -> client.call(): Invalid call" );
				return 0;
			}

			bool result;
			QDataStream reply( replyData, IO_ReadOnly );

			reply >> result;
			if ( result == 0 )
				continue;

			if ( !client.call( clientId.data(), objId.data(), "getWinID()", data, replyType, replyData ) ) {
				LNCdebug( LNC_ERROR, 0, "getActiveKonquerorWindow() -> client.call(): Invalid call" );
				return 0;
			}

			int winID;
			QDataStream reply2( replyData, IO_ReadOnly );

			reply2 >> winID;
			return winID;
		}
	}

	return 0;
}

int showErrorMessage( const char *caption, const char *applicationName, const char *errorMessage, int errorCode, int windowID ) {
	int pid;
	int status;
//	int retval;
//	static int pipefd[ 2 ];
//	static int init = 1;

/*	if ( !init ) {
		printf( "Writing to pipe\n" );
		write( pipefd[ 1 ], "new", 3 );
		return EXIT_SUCCESS;
	}

	retval = pipe( pipefd );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "showErrorMessage( %s, %s, %d, %d ) -> pipe()", caption, applicationName, errorMessage, errorCode, windowID );
		return -EXIT_FAILURE;
	}*/

//	close( pipefd[ 0 ] );
	pid = fork();
	if ( pid < 0 ) {
		LNCdebug( LNC_ERROR, 1, "showErrorMessage( %s, %s, %s, %d, %d ) -> fork()", caption, applicationName, errorMessage, errorCode, windowID );
		return -EXIT_FAILURE;
	} else if ( pid == 0 ) {
//		char buffer[ BUFSIZ ];

//		close( pipefd[ 1 ] );
		int argc = 2;
		const char *argv[] = { "Networkclient error plugin", "--nocrashhandler", NULL };
		KAboutData about("Networkclient error plugin", I18N_NOOP( applicationName ), "0.1"/*VERSION*/ );
		KCmdLineArgs::init( argc, ( char ** ) argv, &about );
		KApplication app;
		KConfig config;
		QString result, cap;

		config.setGroup( "Locale" );
		KGlobal::locale()->setLanguage( config.readEntry( "Language", "en_US" ) );

		if ( errorCode == 0 )
			result = i18n( errorMessage );
		else
			result = KIO::buildErrorString( errorCode, errorMessage );
		cap = i18n( caption );

//		while ( 1 ) {
//			read( pipefd[ 0 ], buffer, BUFSIZ );
			KMessageBox::errorWId( windowID, result, cap );
//		}
		_exit( 0 );
	}  else
		waitpid( pid, &status, 0 );

//	init = 0;
	return EXIT_SUCCESS;
}
