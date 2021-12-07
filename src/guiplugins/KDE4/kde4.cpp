#include "config.h"
#include "plugin.h"

#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kpassworddialog.h>

#include <X11/Xlib.h>
#include <dlfcn.h>

extern "C"
{
	__attribute__ ( ( visibility ( "default" ) ) ) void *giveGUIPluginInfo ( void );
	extern int getActiveWindowID ( void );
}

static const char KDE4type[] = "KDE4";
static const char KDE4description[] = "This plugin allows you to have KDE4 styled username/password dialogs";
static const char *KDE4dependencies[] = { "lib_global_plugin_Xext.so", NULL };

struct pthread_key_struct
{
	uintptr_t seq;
	void ( *destr ) ( void * );
};

struct pthread_key_struct *pthread_keys;
//struct pthread_key_struct *QtCoreKey;
pthread_key_t QtCoreKey;

void cleanerFunc ( void *arg )
{
	( void ) arg;
}

int getPthreadUninitializedDataSection ( unsigned long *startAddress, unsigned long *endAddress )
{
	char *output;
	char *ptr;
	char *end;
	char exec[ BUFSIZ ];
	int exitStatus;

	if ( !startAddress || !endAddress )
	{
		errno = EINVAL;
		LNCdebug ( LNC_ERROR, 1, "getPthreadUninitializedDataSection()" );
		return -EXIT_FAILURE;
	}

	snprintf ( exec, BUFSIZ, "bash -c \"cat /proc/%d/maps | grep libpthread | grep rw-p | awk '{print $1}'\"", getpid() );
	output = execute ( exec, STDOUT_FILENO | STDERR_FILENO, environ, &exitStatus );
	if ( !output )
	{
		LNCdebug ( LNC_ERROR, 1, "getPthreadUninitializedDataSection() -> execute()" );
		return -EXIT_FAILURE;
	}

	if ( exitStatus != 0 )
	{
		LNCdebug ( LNC_ERROR, 0, "getPthreadUninitializedDataSection() -> execute(): %s", output );
		free ( output );
		return -EXIT_FAILURE;
	}

	ptr = strchr ( output, '\n' );
	if ( ptr )
		*ptr = '\0';

	ptr = strchr ( output, '-' );
	if ( !ptr )
	{
		LNCdebug ( LNC_ERROR, 1, "getPthreadUninitializedDataSection(): Invalid format %s )", output );
		free ( output );
		return -EXIT_FAILURE;
	}

	*ptr = '\0';

	*startAddress = strtoul ( output, &end, 16 );
	if ( *startAddress == ULONG_MAX )
	{
		LNCdebug ( LNC_ERROR, 1, "getPthreadUninitializedDataSection() -> strtoul( %s, %s )", output, end );
		free ( output );
		return -EXIT_FAILURE;
	}

	if ( end && *end != '\0' )
	{
		LNCdebug ( LNC_ERROR, 0, "getPthreadUninitializedDataSection() -> strtoul( %s, %s ): %s is not a valid number", output, end, output );
		free ( output );
		return -EXIT_FAILURE;
	}

	*endAddress = strtoul ( ++ptr, &end, 16 );
	if ( *startAddress == ULONG_MAX )
	{
		LNCdebug ( LNC_ERROR, 1, "getPthreadUninitializedDataSection() -> strtoul( %s, %s )", ptr, end );
		free ( output );
		return -EXIT_FAILURE;
	}

	if ( end && *end != '\0' )
	{
		LNCdebug ( LNC_ERROR, 0, "getPthreadUninitializedDataSection() -> strtoul( %s, %s ): %s is not a valid number", ptr, end, ptr );
		free ( output );
		return -EXIT_FAILURE;
	}

	free ( output );

	return EXIT_SUCCESS;
}

int getQTCore4TSDKey ( unsigned long startAddress, unsigned long endAddress )
{
	int retval;
	int i;
	int found;
	unsigned long ptr;
	pthread_key_t key;
	Dl_info info;

	retval = pthread_key_create ( &key, cleanerFunc );
	if ( retval < 0 )
	{
		LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ) -> pthread_key_create()", startAddress, endAddress );
		return -EXIT_FAILURE;

	}

	found = 0;
	for ( ptr = startAddress; ptr < endAddress; ptr += sizeof ( void * ) )
	{
		pthread_keys = ( struct pthread_key_struct * ) ptr;
		if ( pthread_keys[ key ].destr == cleanerFunc )
		{
			found = 1;
			break;
		}
	}

	if ( !found )
	{
		retval = pthread_key_delete ( key );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ) -> pthread_key_delete()", startAddress, endAddress );

		LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ): could not find __pthread_keys symbol", startAddress, endAddress );
		errno = ENOENT;
		return -EXIT_FAILURE;
	}

	found = 0;
	for ( i = 0; i < PTHREAD_KEYS_MAX; ++i )
	{
		if ( pthread_keys[ i ].destr )
		{
			retval = dladdr ( ( void * ) pthread_keys[ i ].destr, &info );
			if ( retval == 0 )
			{
				LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ) -> dladdr( %p )", startAddress, endAddress, pthread_keys[ i ].destr );
				continue;
			}

			if ( strstr ( info.dli_fname, "libQtCore" ) )
			{
				QtCoreKey = ( pthread_key_t ) i;
				found = 1;
				break;
			}

		}
	}

	if ( !found )
	{
		retval = pthread_key_delete ( key );
		if ( retval < 0 )
			LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ) -> pthread_key_delete()", startAddress, endAddress );

		LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ): could not find libQtCore4 TSD destructor in __pthread_keys", startAddress, endAddress );
		errno = ENOENT;
		return -EXIT_FAILURE;
	}

	retval = pthread_key_delete ( key );
	if ( retval < 0 )
		LNCdebug ( LNC_ERROR, 1, "getQTCore4CleanerFunction( %lu, %lu ) -> pthread_key_delete()", startAddress, endAddress );


	return EXIT_SUCCESS;
}

static int KDE4dialog ( const char *URL, struct auth *auth, const char *processName )
{
	int winID = getActiveWindowID();
	int argc = 2;
	const char *argv[] = { "Networkclient KDE4 plugin", "--nocrashhandler", NULL };

	( void ) processName;

	if ( !auth )
		return -EXIT_FAILURE;

	KCmdLineArgs::init ( argc, ( char ** ) argv, "LinuxNetworkClient", NULL, ki18n ( "Networkclient KDE plugin" ), VERSION, ki18n ( KDE4description ) );
	KApplication app;
	app.disableSessionManagement();

	KPasswordDialog dlg ( NULL, KPasswordDialog::ShowUsernameLine );
	QString prompt ( i18n ( "You need to supply a username and a password" ) + "\n" + URL );
	dlg.setWindowModality( Qt::ApplicationModal );
	dlg.setPlainCaption ( i18n ( "Authorization Dialog" ) );
	dlg.setPrompt ( prompt );
	dlg.setUsername ( QString ( auth->username ) );

//	Qt::WindowFlags flags = dlg.windowFlags ();
//	flags = flags & ~Qt::Dialog;
//	flags = flags | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint;
//	dlg.setWindowFlags( flags );

	dlg.show();

	if ( winID > 0 )
		XSetTransientForHint ( dlg.x11Info().display(), dlg.winId(), winID );

	int retval = app.exec();
	if ( retval < 0  || ( dlg.result() != KPasswordDialog::Accepted ) )
		return -EXIT_FAILURE;

	strlcpy ( auth->username, dlg.username().toAscii(), NAME_MAX + 1 );
	strlcpy ( auth->password, dlg.password().toAscii(), NAME_MAX + 1 );

	return EXIT_SUCCESS;
}

int KDE4init( void )
{
	unsigned long startAddress;
	unsigned long endAddress;
	int retval;

	retval = getPthreadUninitializedDataSection( &startAddress, &endAddress );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "KDE4init() -> getPthreadUninitializedDataSection()" );
		return -EXIT_FAILURE;
	}

	retval = getQTCore4TSDKey( startAddress, endAddress );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "KDE4init() -> getQTCore4TSDKey()" );
		return -EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void KDE4cleanup( void )
{
	pthread_key_delete( QtCoreKey );
}

static GUIPlugin kde4 =
{
	KDE4init,
	KDE4cleanup,
	NULL,
	KDE4type,
	KDE4dialog,
	NULL,
	KDE4description,
	NULL,
	KDE4dependencies,
	{0},
	0,
	1,
	0,
	0,
	0,
	{ NULL, NULL, NULL, { 0 } }
};

void *giveGUIPluginInfo ( void )
{
	return &kde4;
}
