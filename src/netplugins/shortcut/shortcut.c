/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#include "networkclient.h"
#include "list.h"
#include "nconfig.h"
#include "plugin.h"
#include "pluginhelper.h"
#include "user.h"
#include "nconfig.h"

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
#include <kdb.h>
#include <kdbprivate.h>

typedef struct shortcut {
	struct list list;
	char URL[ PATH_MAX ];
	int URLLength;
	char *name;
	int nameLength;
	char shortName[ PATH_MAX ];
	int shortNameLength;
	char longName[ PATH_MAX ];
	int longNameLength;
	int64_t usageCount;
}
Shortcut;

defineList( shortcutList, struct shortcut );
static struct shortcutList __tmpList;
static struct shortcutList *tmpList = &__tmpList;
static struct shortcutList __shortcutList;
static struct shortcutList *shortcutList = &__shortcutList;

static int64_t visibleUsageCount = 0;
static int64_t biggestUsageCount = 0;
static volatile int needsResorting = 0;

static const char Shortcutdescription[] = "This plugin keeps track of most used shares";
static const char ShortcutnetworkName[] = "Shortcut Entries";
static const char Shortcutprotocol[] = "Shortcut";

static void shortcutOpendirHook( const char *URL, int *replace, int runMode, NETWORK_DIR *result );
static void shortcutStatHook( const char *URL, struct stat *buf, int *replace, int runMode, int result );

static int Shortcutinit( void );
static void ShortcutcleanUp( void );

static void initShortcutList( void );
static int shortcutFind( char *search, struct shortcut *entry );
static int shortcutCompare( struct shortcut *search, struct shortcut *entry );
static void shortcutPrint( struct shortcut *entry );

//static char *createNewURL( char *buffer, const char *oldURL, struct shortcut *shortcut );
static void rebuildShortcut( char *buffer, char *shortcut );

static int monitorShortcuts( const char *path, const char *filename, int event );
static int monitorConfiguration( const char *path, const char *filename, int event );
static void *readShortCutEntries( void * );
static void resortShortcutList( void );
static int shortcutExists( const char *URL );
static int64_t readShortcutUsageCount( const char *URL );
static void writeShortcutUsageCount( const char *URL, int64_t usageCount );

static int addShortcutEntry( const char *URL );
static int deleteShortcutEntry( const char *URL );
static int updateShortcutEntry( const char *URL );

static volatile int maxVisibleShortcuts = 5;
static volatile int useLongShortcutNames = 0;


#define forEachShortcut( entry )															\
	struct list *__tmp__;																\
	int __i__;						\
	for ( __i__ = 0, ( entry ) = container_of( shortcutList->list.next, typeof( *( entry ) )), __tmp__ = shortcutList->list.next->next; __i__ < maxVisibleShortcuts && ( entry ) != container_of( &shortcutList->list, typeof( *( entry ) )) && entry->usageCount > 0; ++__i__, entry = container_of(__tmp__, typeof( *(entry))), __tmp__ = __tmp__->next )

#define findShortcutEntry( name )				\
({								\
	lockShortcutList();					\
	resortShortcutList();					\
	unlockShortcutList();					\
	findListEntry( shortcutList, name );			\
})

#define lockShortcutList() lockList( shortcutList, WRITE_LOCK )
#define unlockShortcutList() unlockList( shortcutList )

static void shortcutOpendirHook( const char *URL, int *replace, int runMode, NETWORK_DIR *result ) {
	int cmp;
	int hasShares;
	static u_int64_t previousTime = 0;
	u_int64_t currentTime;
	u_int64_t delay = 333333ULL;
	int64_t usageCount;
	int64_t factor;
	NetworkPlugin *plugin;
	Service *service;

	( void ) replace;
	( void ) runMode;

	if ( result ) {
		cmp = strncmp( URL + 10, Shortcutprotocol, 8 );
		if ( cmp == 0 )
			return ;

		plugin = findNetworkPlugin( ( char * ) URL + 10 );
		if ( !plugin )
			return ;

		if ( plugin->getShares )
			hasShares = 1;
		else
			hasShares = 0;
		putNetworkPlugin( plugin );

		service = findListEntry( browselist, URL );
		if ( !service )
			return;

		if ( ( service->hostType == MANUAL_HOST ) || !service->host || ( hasShares && !service->share ) ) {
			putListEntry( browselist, service );
			return;
		}

		putListEntry( browselist, service );

		currentTime = getCurrentTime();
		if ( currentTime < ( previousTime + delay ) )
			return ;

		usageCount = readShortcutUsageCount( URL );
		if ( usageCount < 0LL )
			usageCount *= -1LL;

		if ( usageCount < visibleUsageCount ) {
			factor = ( visibleUsageCount - usageCount ) / 20LL;
			if ( factor < 0 || factor == 0 )
				factor = 1;
			usageCount += factor;
		} else
			usageCount += 1;

		writeShortcutUsageCount( URL, usageCount );
		previousTime = currentTime;

	} else if ( errno == ENODEV || errno == EIO || errno == EHOSTDOWN || errno == ETIMEDOUT || errno == ENOMSG ) {
		if ( !shortcutExists( URL ) )
			return;

		usageCount = readShortcutUsageCount( URL );
		if ( usageCount > 0 ) {
			usageCount = usageCount * -1;
			writeShortcutUsageCount( URL, usageCount );
		}
	}
}

static void shortcutStatHook( const char *URL, struct stat *buf, int *replace, int runMode, int result ) {
	int cmp;
	int hasShares;
	static u_int64_t previousTime = 0;
	u_int64_t currentTime;
	u_int64_t delay = 333333ULL;
	int64_t usageCount;
	int64_t factor;
	NetworkPlugin *plugin;
	Service *service;

	( void ) replace;
	( void ) runMode;
	( void ) buf;


	if ( result == 0 ) {
		cmp = strncmp( URL + 10, Shortcutprotocol, 8 );
		if ( cmp == 0 )
			return ;

		plugin = findNetworkPlugin( ( char * ) URL + 10 );
		if ( !plugin )
			return ;

		if ( plugin->getShares )
			hasShares = 1;
		else
			hasShares = 0;

		putNetworkPlugin( plugin );

		service = findListEntry( browselist, URL );
		if ( !service )
			return;

		if ( ( service->flags & MANUAL_HOST ) || !service->host || ( hasShares && !service->share ) ) {
			putListEntry( browselist, service );
			return;
		}

		putListEntry( browselist, service );

		currentTime = getCurrentTime();
		if ( currentTime < ( previousTime + delay ) )
			return ;

// 		usageCount = readShortcutUsageCount( URL );
// 		if ( usageCount < 0LL )
// 			usageCount *= -1LL;
//
// 		if ( usageCount < visibleUsageCount ) {
// 			factor = ( visibleUsageCount - usageCount ) / 20LL;
// 			if ( factor < 0 || factor == 0 )
// 				factor = 1;
// 			usageCount += factor;
// 		} else
// 			usageCount += 1;

//		writeShortcutUsageCount( URL, usageCount );
		previousTime = currentTime;

	} else if ( errno == ENODEV || errno == EIO || errno == EHOSTDOWN || errno == ETIMEDOUT || errno == ENOMSG  ) {
		if ( !shortcutExists( URL ) )
			return;

		//usageCount = readShortcutUsageCount( URL );
		//if ( usageCount > 0 ) {
		//	usageCount = usageCount * -1;
		//	writeShortcutUsageCount( URL, usageCount );
		//}
	}
}

static int Shortcutinit( void ) {
	struct passwd * user;
	char buffer[ BUFSIZ ];
	char path[ PATH_MAX ];
	char *configEntry;

	initKeys( "LNC/plugins/Shortcut" );
	maxVisibleShortcuts = getNumKey( "LNC/plugins/Shortcut", "MaximumVisibleShortcuts", 5 );
	configEntry = getCharKey( "LNC/plugins/Shortcut", "useLongShortcutNames", "False" );
	if ( strcasecmp( configEntry, "True" ) == 0 )
		useLongShortcutNames = 1;
	else
		useLongShortcutNames = 0;

	free( configEntry );

	setNumKey( "LNC/plugins/Shortcut", "MaximumVisibleShortcuts", maxVisibleShortcuts, USER_ROOT );
	setCharKey( "LNC/plugins/Shortcut", "useLongShortcutNames", useLongShortcutNames ? "True" : "False", USER_ROOT );
	cleanupKeys( "LNC/plugins/Shortcut" );

	initShortcutList();

	createThread( ( void * ( * ) ( void * ) ) readShortCutEntries, NULL, DETACHED );

	registerSyscallHook( shortcutOpendirHook, LNC_OPENDIR, AFTER_SYSCALL );
	registerSyscallHook( shortcutStatHook, LNC_STAT, AFTER_SYSCALL );

	user = getSessionInfoByUID( geteuid(), buffer );
	if ( !user ) {
		LNCdebug( LNC_ERROR, 1, "Shortcutinit() -> getSessionInfoByUID( %d )", geteuid() );
		return -EXIT_FAILURE;
	}

	snprintf( path, PATH_MAX, "%s/%s/user/sw/LNC/Shortcuts", user->pw_dir, KDB_DB_USER );
	addFileToMonitorList( path, monitorShortcuts );

	snprintf( path, PATH_MAX, "%s/%s/user/sw/LNC/plugins/Shortcut", user->pw_dir, KDB_DB_USER );
	addFileToMonitorList( path, monitorConfiguration );

	return EXIT_SUCCESS;
}

static void ShortcutcleanUp( void ) {
	struct passwd * user;
	char path[ PATH_MAX ];
	char buffer[ BUFSIZ ];

	unregisterSyscallHook( shortcutOpendirHook );
	unregisterSyscallHook( shortcutStatHook );
	clearList( shortcutList );

	user = getSessionInfoByUID( geteuid(), buffer );
	if ( !user ) {
		LNCdebug( LNC_ERROR, 1, "Shortcutinit() -> getSessionInfoByUID( %d )", geteuid() );
		return;
	}

	snprintf( path, PATH_MAX, "%s/%s/user/sw/LNC/Shortcuts", user->pw_dir, KDB_DB_USER );
	delFileFromMonitorList( path, monitorShortcuts );

	snprintf( path, PATH_MAX, "%s/%s/user/sw/LNC/plugins/Shortcut", user->pw_dir, KDB_DB_USER );
	delFileFromMonitorList( path, monitorConfiguration );
}

// static char *createNewURL( char *buffer, const char *oldURL, struct shortcut *shortcut ) {
// 	char *ptr;
//
// 	ptr = strchr( oldURL + 19, '/' );
// 	if ( ptr ) {
// 		strcpy( buffer, shortcut->URL );
// 		strcpy( buffer + shortcut->URLLength, ptr );
// 		return buffer;
// 	}
//
// 	return ( char * ) shortcut->URL;
// }

static int shortcutFind( char *search, struct shortcut *entry ) {
	int cmp;

	cmp = strncmp( search, entry->URL, entry->URLLength );
	if ( cmp == 0 ) //&& ( search[ entry->URLLength ] == '\0' || search[ entry->URLLength ] == '/' ) )
		return 0;

	cmp = strncmp( search, entry->longName, entry->longNameLength );
	if ( cmp == 0 ) //&& ( search[ entry->longNameLength ] == '\0' || search[ entry->longNameLength ] == '/' ) )
		return 0;

	cmp = strncmp( search, entry->shortName, entry->shortNameLength );
	if ( cmp == 0 ) //&& ( search[ entry->shortNameLength ] == '\0' || search[ entry->shortNameLength ] == '/' ) )
		return 0;

	return -1;
}

static int shortcutCompare( struct shortcut *search, struct shortcut *entry ) {
	int64_t cmp = entry->usageCount - search->usageCount;

	if ( cmp == 0LL || cmp < 0LL )
		return -1;

	return 1;
}

static void shortcutPrint( struct shortcut *entry ) {
	LNCdebug( LNC_INFO, 0, "Shortcut name = %s, usage count = %lld", entry->name, entry->usageCount );
}

static void rebuildShortcut( char *buffer, char *shortcut ) {
	while ( ( *buffer++ = *shortcut++ ) ) {
		if ( *( buffer - 1 ) == '\\' )
			* ( buffer - 1 ) = '/';
	}
}

static void initShortcutList( void ) {
	initList( shortcutList, shortcutCompare, shortcutFind, NULL, shortcutPrint, NULL, NULL, NULL );
	initList( tmpList, shortcutCompare, shortcutFind, NULL, shortcutPrint, NULL, NULL, NULL );
}

static void *readShortCutEntries( void *ignore ) {
	struct key *key;
	char URL[ PATH_MAX ];
	int depth;
	NetworkPlugin *plugin;

	( void ) ignore;

	sleep( 2 );
	initKeys( "LNC/Shortcuts" );
	rewindKeys( "LNC/Shortcuts", USER_ROOT );
	while ( ( key = forEveryKey2( "LNC/Shortcuts", USER_ROOT ) ) ) {
		if ( key->type != LNC_CONFIG_KEY ) {
			free( key );
			continue;
		}

		if ( key->value[ 0 ] == '\0' ) {
			LNCdebug( LNC_ERROR, 0, "readShortCutEntries(): invalid key entry ( %s )", key->name );
			free( key );
			continue;
		}

		rebuildShortcut( URL, key->name );
		depth = getDepth( URL );
		if ( depth < 3 ) {
			LNCdebug( LNC_ERROR, 0, "readShortCutEntries(): invalid shortcut format %s, skipping", URL );
			free( key );
			continue;
		}

		plugin = findListEntry( networkPluginList, URL + 10 );
		if ( plugin ) {
			if ( strncmp( URL + 10, plugin->networkName, plugin->networkNameLength ) == 0 )
				LNCdebug( LNC_ERROR, 0, "readShortCutEntries(): invalid shortcut format %s, shortcut is using network name instead of protocol name, skipping", URL );
			else
				addShortcutEntry( URL );

			putListEntry( networkPluginList, plugin );

		}

		free( key );
	}
	cleanupKeys( "LNC/Shortcuts" );

	needsResorting = 1;
	LNCdebug( LNC_INFO, 0, "shortcutList" );
	printList( shortcutList );
	LNCdebug( LNC_INFO, 0, "shortcutList" );

	return NULL;
}

static void resortShortcutList( void ) {
	struct shortcut * entry;
	struct shortcut *found;
	struct shortcut *tmp;
	struct shortcut *new;
	int i, j, cmp;

	if ( !needsResorting )
		return ;

	LNCdebug( LNC_DEBUG, 0, "resortShortcuts(): resorting shortcut list" );

	tmpList->list.next = &tmpList->list;
	tmpList->list.prev = &tmpList->list;
	forEachListEntry( shortcutList, entry ) {
		forEachListEntry( tmpList, new ) {
			cmp = tmpList->Compare( entry, new );
			if ( cmp < 0 || cmp == 0 )
				break;
		}

		entry->list.next = &new->list;
		entry->list.prev = new->list.prev;
		new->list.prev->next = &entry->list;
		new->list.prev = &entry->list;
	}

	if ( isListEmpty( tmpList ) ) {
		shortcutList->list.next = &shortcutList->list;
		shortcutList->list.prev = &shortcutList->list;
	} else {
		shortcutList->list = tmpList->list;
		tmpList->list.next->prev = &shortcutList->list;
		tmpList->list.prev->next = &shortcutList->list;
	}

	i = 0;
	forEachListEntry( shortcutList, tmp ) {
		j = 0;
		tmp->name = tmp->shortName;
		tmp->nameLength = tmp->shortNameLength;
		forEachListEntry( shortcutList, found ) {
			if ( useLongShortcutNames || ( found != tmp && strcmp( found->shortName, tmp->shortName ) == 0 ) ) {
				found->name = found->longName;
				found->nameLength = found->longNameLength;
				tmp->name = tmp->longName;
				tmp->nameLength = tmp->longNameLength;
			}
			if ( ++j == maxVisibleShortcuts )
				break;
		}

		if ( i == 0 ) {
			biggestUsageCount = tmp->usageCount;
			if ( biggestUsageCount < 0 )
				biggestUsageCount = 0;
		}

		if ( ++i == maxVisibleShortcuts ) {
			visibleUsageCount = tmp->usageCount;
			if ( visibleUsageCount < 0 )
				visibleUsageCount = 0;
			break;
		}
	}

	needsResorting = 0;
}

static void writeShortcutUsageCount( const char *URL, int64_t usageCount ) {
	char key[ PATH_MAX ] = "user/sw/LNC/Shortcuts/";
	char value[ BUFSIZ ];
	int retval;

	strlcpy( key + 22, URL, PATH_MAX - 22 );
	replaceCharacter( key + 22, '/', '\\' );

	snprintf( value, BUFSIZ, "%lld", ( long long int ) usageCount );
	retval= kdbSetString( ( KDB *) keysDBHandle, key, value);
	if ( retval != 0 )
		LNCdebug( LNC_ERROR, 1, "writeShortcutUsageCount( %s ) -> kdbSetValue( %s, %lld )", URL, key, usageCount );
}

static int64_t readShortcutUsageCount( const char *URL ) {
	char key[ PATH_MAX ] = "user/sw/LNC/Shortcuts/";
	char value[ BUFSIZ ] = "";
	int retval;

	strlcpy( key + 22, URL, PATH_MAX - 22 );
	replaceCharacter( key + 22, '/', '\\' );

	retval = kdbGetString( ( KDB *) keysDBHandle, key, value, BUFSIZ );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 1, "readShortcutUsageCount( %s ) -> kdbGetString( %s, value, %d )", URL, key, BUFSIZ );
		return ( int64_t ) 0;
	}

	return charToLong( value );
}

static int shortcutExists( const char *URL ) {
	char key[ PATH_MAX ] = "user/sw/LNC/Shortcuts/";
	char value[ BUFSIZ ];
	int retval;

	strlcpy( key + 22, URL, PATH_MAX );
	replaceCharacter( key + 22, '/', '\\' );

	retval = kdbGetString( ( KDB *) keysDBHandle, key, value, BUFSIZ );
	if ( retval == 0 )
		return 1;

	return 0;

	if ( retval < 0 && errno == ENOENT )
		return 0;

	return 1;
}

static int addShortcutEntry( const char *URL ) {
	NetworkPlugin * plugin;
	struct shortcut *new;
	char cpy[ PATH_MAX ];
	char *token;
	char *ptr;
	char *tokens[ 6 ];
	int i;
	int hasShares;

	plugin = findNetworkPlugin( ( char * ) URL + 10 );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "addShortcutEntry( %s) -> findNetworkPlugin()", URL );
		return -EXIT_FAILURE;
	}

	if ( plugin->getShares )
		hasShares = 1;
	else
		hasShares = 0;

	putNetworkPlugin( plugin );

	new = malloc( sizeof( struct shortcut ) );
	if ( !new ) {
		LNCdebug( LNC_ERROR, 1, "addShortcutEntry( %s ) -> malloc()", URL );
		return -EXIT_FAILURE;
	}

	snprintf( cpy, PATH_MAX, "%s", URL );
	for ( i = 1; ( token = strtok_r( ( i == 1 ) ? cpy : NULL, "/", &ptr ) ); ++i ) {
		if ( i > 5 || ( !hasShares && i > 4 ) ) {
			LNCdebug( LNC_ERROR, 0, "addShortcutEntry( %s ): Invalid shortcut format, to long", URL );
			return -EXIT_FAILURE;
		}

		tokens[ i ] = token;
	}

	i -= 1;

	if ( ( hasShares && i < 4 ) || i < 3 ) {
		LNCdebug( LNC_ERROR, 0, "addShortcutEntry( %s ): Invalid shortcut format, to short", URL );
		return -EXIT_FAILURE;
	}

	if ( hasShares ) {
		sprintf( new->shortName, "%s on %s (%s)", tokens[ i ], tokens[ i - 1 ], tokens[ 2 ] );
		if ( i == 5 )
			sprintf( new->longName, "%s on %s in %s (%s)", tokens[ 5 ], tokens[ 4 ], tokens[ 3 ], tokens[ 2 ] );
		else
			strlcpy( new->longName, new->shortName, PATH_MAX );
	} else {
		sprintf( new->shortName, "%s (%s)", tokens[ i ], tokens[ 2 ] );
		if ( i == 4 )
			sprintf( new->longName, "%s in %s (%s)", tokens[ 4 ], tokens[ 3 ], tokens[ 2 ] );
		else
			strlcpy( new->longName, new->shortName, PATH_MAX );
	}

	new->name = new->shortName;
	strlcpy( new->URL, URL, PATH_MAX );
	new->URLLength = strlen( new->URL );
	new->shortNameLength = strlen( new->shortName );
	new->longNameLength = strlen( new->longName );
	new->nameLength = new->shortNameLength;

	new->usageCount = readShortcutUsageCount( new->URL );
	if ( new->usageCount > visibleUsageCount )
		needsResorting = 1;

	addListEntry( shortcutList, new );

	return EXIT_SUCCESS;
}

static int deleteShortcutEntry( const char *URL ) {
	NetworkPlugin * plugin;
	struct shortcut *found;

	plugin = findNetworkPlugin( ( char * ) URL + 10 );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "deleteShortcutEntry( %s) -> findNetworkPlugin()", URL );
		return -EXIT_FAILURE;
	}
	putNetworkPlugin( plugin );

	found = findListEntry( shortcutList, ( char * ) URL );
	if ( !found ) {
		LNCdebug( LNC_ERROR, 1, "deleteShortcutEntry( %s ) -> findListEntry()", URL );
		return -EXIT_FAILURE;
	}

	delListEntry( shortcutList, found );
	putListEntry( shortcutList, found );

	return EXIT_SUCCESS;
}

static int updateShortcutEntry( const char *URL ) {
	NetworkPlugin * plugin;
	int64_t previousUsageCount;
	struct shortcut *found;

	plugin = findNetworkPlugin( ( char * ) URL + 10 );
	if ( !plugin ) {
		LNCdebug( LNC_ERROR, 1, "updateShortcutEntry( %s) -> findNetworkPlugin()", URL );
		return -EXIT_FAILURE;
	}
	putNetworkPlugin( plugin );

	found = findListEntry( shortcutList, ( char * ) URL );
	if ( !found ) {
		LNCdebug( LNC_ERROR, 1, "updateShortcutEntry( %s ) -> findListEntry()", URL );
		return -EXIT_FAILURE;
	}

	previousUsageCount = found->usageCount;
	found->usageCount = readShortcutUsageCount( found->URL );
	if ( previousUsageCount <= visibleUsageCount && found->usageCount > visibleUsageCount )
		needsResorting = 1;

	if ( previousUsageCount > 0 && found->usageCount < 0 )
		needsResorting = 1;

	putListEntry( shortcutList, found );

	return EXIT_SUCCESS;
}

static int monitorShortcuts( const char *path, const char *filename, int event ) {
	char shortcut[ PATH_MAX + 1 ];

	snprintf( shortcut, PATH_MAX, "%s", filename );
	replaceString( shortcut, "%5C", "/");

	LNCdebug( LNC_DEBUG, 0, "monitorShortcuts(): path = %s, filename = %s, event = %d", path, filename, event );
	if ( event == FAMCreated ) {
		addShortcutEntry( shortcut );
		printList( shortcutList );
	} else if ( event == FAMDeleted ) {
		deleteShortcutEntry( shortcut );
		printList( shortcutList );
	} else if ( event == FAMChanged )
		updateShortcutEntry( shortcut );

	return EXIT_SUCCESS;
}

static int monitorConfiguration( const char *path, const char *filename, int event ) {
	char *configEntry;

	LNCdebug( LNC_DEBUG, 0, "monitorConfiguration(): path = %s, filename = %s, event = %d", path, filename, event );
	if ( strcmp( filename, "MaximumVisibleShortcuts" ) != 0 )
		return EXIT_SUCCESS;

	initKeys( "LNC/plugins/Shortcut");
	if ( event == FAMCreated || event == FAMChanged || event == FAMDeleted ) {
		maxVisibleShortcuts = getNumKey( "LNC/plugins/Shortcut", "MaximumVisibleShortcuts", 5 );
		configEntry = getCharKey( "LNC/plugins/Shortcut", "useLongShortcutNames", "False" );
		if ( strcasecmp( configEntry, "True" ) == 0 )
			useLongShortcutNames = 1;
		else
			useLongShortcutNames = 0;

		free( configEntry );

		needsResorting = 1;

		LNCdebug( LNC_INFO, 0, "monitorConfiguration(): Maximum visible shortcuts is %d", maxVisibleShortcuts );
		LNCdebug( LNC_INFO, 0, "monitorConfiguration(): Using long shortcut names is %s", useLongShortcutNames ? "True" : "False" );
	}

	cleanupKeys( "LNC/plugins/Shortcut");

	return EXIT_SUCCESS;
}

// static NETWORK_FILE *Shortcutopen( const char *URL, int flags, mode_t mode, NETWORK_FILE *file ) {
// 	struct shortcut * shortcut;
// 	NETWORK_FILE *tmp;
// 	char newURL[ PATH_MAX ];
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutopen( %s, %d ) -> findShortcutEntry()", URL, flags );
// 		return NULL;
// 	}
//
// 	tmp = LNCopen( createNewURL( newURL, URL, shortcut ), flags, mode );
// 	putListEntry( shortcutList, shortcut );
// 	if ( tmp )
// 		LNCclose( file );
//
// 	return tmp;
// }

static NETWORK_DIR *Shortcutopendir( const char *URL, unsigned int flags, NETWORK_DIR *dir ) {
	( void ) flags;

	if ( strcmp( URL, "network://Shortcut" ) == 0 ) {
		dir->privateData = ( void * ) 1;
		errno = 0;
		return dir;
	}

	errno = ENOENT;
	return NULL;
}

int Shortcutclosedir( NETWORK_DIR *dir ) {
	( void ) dir;
	return EXIT_SUCCESS;
}

int Shortcutreaddir( NETWORK_DIR *dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result ) {
	int i = 1;
	Shortcut *shortcut;

	lockShortcutList();
	forEachShortcut( shortcut ) {
		if ( i == void_to_int_cast dir->privateData ) {
			dir->privateData = ( void * ) ( ( int64_t ) dir->privateData + 1 );
			entry->type = DT_LNK;
			strlcpy( entry->name, shortcut->name, NAME_MAX + 1 );
			strlcpy( entry->comment, shortcut->URL, NAME_MAX + 1 );
			entry->IP[ 0 ] = '\0';
			*result = entry;
			unlockShortcutList();

			return EXIT_SUCCESS;
		}
		++i;
	}
	unlockShortcutList();

	errno = 0;
	*result = NULL;

	return EXIT_SUCCESS;
}

static int Shortcutstat( const char *URL, struct stat *buf ) {
	struct shortcut *shortcut;

	shortcut = findShortcutEntry( ( char * ) URL + 19 );
	if ( !shortcut ) {
		LNCdebug( LNC_ERROR, 1, "Shortcutstat( %s ) -> findShortcutEntry()", URL );
		return -EXIT_FAILURE;
	}

	buf->st_mode |= S_IFLNK | 0644;
	buf->st_nlink = 1;
	buf->st_size = shortcut->URLLength;
	putListEntry( shortcutList, shortcut );

	return EXIT_SUCCESS;
}

static ssize_t Shortcutreadlink( const char *URL, char *buf, size_t bufsize ) {
	struct shortcut * shortcut;
	ssize_t retval = -EXIT_FAILURE;

	shortcut = findShortcutEntry( ( char * ) URL + 19 );
	if ( !shortcut ) {
		LNCdebug( LNC_ERROR, 1, "Shortcutreadlink %s ) -> findShortcutEntry()", URL );
		return retval;
	}

	snprintf( buf, bufsize, "%s", shortcut->URL );
	retval = shortcut->URLLength;
	putListEntry( shortcutList, shortcut );

	return retval;
}

// static int Shortcutmknod( const char *URL, mode_t mode ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutmknod( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCmknod( createNewURL( newURL, URL, shortcut ), mode );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }
//
// static int Shortcutunlink( const char *URL ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutunlink( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCunlink( createNewURL( newURL, URL, shortcut ) );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }
//
// static int Shortcuttruncate( const char *URL, off_t size ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcuttruncate( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCtruncate( createNewURL( newURL, URL, shortcut ), size );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }
//
// static int Shortcutrmdir( const char *URL ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	char key[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutrmdir( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	if ( strcmp( shortcut->name, URL + 19 ) == 0 ) {
// 		snprintf( key, PATH_MAX, "user/sw/LNC/Shortcuts/%s", shortcut->URL );
// 		replaceCharacter( key + 22, '/', '\\' );
// 		retval = kdbRemove( ( KDB *) keysDBHandle, key );
// 		if ( retval > 0 )
// 			retval *= -1;
// 	} else
// 		retval = LNCrmdir( createNewURL( newURL, URL, shortcut ) );
//
// 		putListEntry( shortcutList, shortcut );
// 	return retval;
// }
//
// static int Shortcutmkdir( const char *URL, mode_t mode ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutmkdir( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCmkdir( createNewURL( newURL, URL, shortcut ), mode );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }
//
// static int Shortcutrename( const char *fromURL, const char *toURL ) {
// 	struct shortcut * shortcut;
// 	char newToURL[ PATH_MAX ];
// 	char newFromURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) fromURL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutrenam( %s ) -> findShortcutEntry()", fromURL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCrename( createNewURL( newFromURL, fromURL, shortcut ), createNewURL( newToURL, toURL, shortcut ) );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }

/*static  int Shortcutsymlink( const char *fromURL, const char *toURL ) {
	struct shortcut *shortcut;
	char newURL[ PATH_MAX ];
	int retval;

	shortcut = findShortcutEntry( ( char * ) fromURL + 19 );
	if ( !shortcut ) {
		LNCdebug( LNC_ERROR, 1, "Shortcutsymlink( %s ) -> findShortcutEntry()", fromURL );
		return -EXIT_FAILURE;
	}

	retval = LNCsymlink( createNewURL( newURL, fromURL, shortcut ), toURL );
	putListEntry( shortcutList, shortcut );

	return retval;
}

static  int Shortcutlink( const char *fromURL, const char *toURL ) {
	struct shortcut *shortcut;
	char newURL[ PATH_MAX ];
	int retval;

	shortcut = findShortcutEntry( fromURL + 19 );
	if ( !shortcut ) {
		LNCdebug( LNC_ERROR, 1, "Shortcutlink( %s ) -> findShortcutEntry()", fromURL );
		return -EXIT_FAILURE;
	}

	retval = LNClink( createNewURL( newURL, fromURL, shortcut ), toURL );
	putListEntry( shortcutList, shortcut );

	return retval;
}*/

// static int Shortcutchown( const char *URL, uid_t owner, gid_t group ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutchown( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCchown( createNewURL( newURL, URL, shortcut ), owner, group );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }
//
// static int Shortcutchmod( const char *URL, mode_t mode ) {
// 	struct shortcut * shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcutchmod( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCchmod( createNewURL( newURL, URL, shortcut ), mode );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }
//
// static int Shortcututime( const char *URL, struct utimbuf *buf ) {
// 	struct shortcut *shortcut;
// 	char newURL[ PATH_MAX ];
// 	int retval;
//
// 	shortcut = findShortcutEntry( ( char * ) URL + 19 );
// 	if ( !shortcut ) {
// 		LNCdebug( LNC_ERROR, 1, "Shortcututime( %s ) -> findShortcutEntry()", URL );
// 		return -EXIT_FAILURE;
// 	}
//
// 	retval = LNCutime( createNewURL( newURL, URL, shortcut ), buf );
// 	putListEntry( shortcutList, shortcut );
//
// 	return retval;
// }

static NetworkPlugin shortcut =
    {
        .supportsAuthentication = 1,
        .minimumTouchDepth = 2,
        .init = Shortcutinit,
        .cleanUp = ShortcutcleanUp,
        .networkName = ShortcutnetworkName,
        .description = Shortcutdescription,
        .protocol = Shortcutprotocol,
//        .open = Shortcutopen,
        .opendir = Shortcutopendir,
        .readdir = Shortcutreaddir,
        .closedir = Shortcutclosedir,
        .stat = Shortcutstat,
        .readlink = Shortcutreadlink,
//        .mknod = Shortcutmknod,
//        .unlink = Shortcutunlink,
//        .truncate = Shortcuttruncate,
//        .rmdir = Shortcutrmdir,
//        .mkdir = Shortcutmkdir,
//        .rename = Shortcutrename,
//        .chown = Shortcutchown,
//        .chmod = Shortcutchmod,
//        .utime = Shortcututime,
    };

void *giveNetworkPluginInfo( void ) {
	return & shortcut;
}
