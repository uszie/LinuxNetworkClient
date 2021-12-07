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
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <wait.h>

#include "config.h"
#include "plugin.h"
#include "list.h"
#include "nconfig.h"

struct networkPluginList __networkPluginList;
struct guiPluginList __guiPluginList;
struct globalPluginList __globalPluginList;
struct syscallHookList __syscallHookList;

struct networkPluginList *networkPluginList = &__networkPluginList;
struct guiPluginList *guiPluginList = &__guiPluginList;
struct globalPluginList *globalPluginList = &__globalPluginList;
struct syscallHookList *syscallHookList = &__syscallHookList;

static void createGUIPluginList ( void );
static int guiPluginCompare ( GUIPlugin *new, GUIPlugin *entry );
static int guiPluginFind ( const char *search, GUIPlugin *entry );
static void guiPluginFree ( GUIPlugin *entry );
static void guiPluginonInsert ( GUIPlugin *entry );
static void guiPluginPrint ( GUIPlugin *entry );

static void createNetworkPluginList ( void );
static int networkPluginCompare ( NetworkPlugin *new, NetworkPlugin *entry );
static int networkPluginFind ( const char *search, NetworkPlugin *entry );
static void networkPluginFree ( NetworkPlugin *entry );
static void networkPluginonInsert ( NetworkPlugin *entry );
static void networkPluginPrint ( NetworkPlugin *entry );

static void createGlobalPluginList ( void );
static int globalPluginCompare ( GlobalPlugin *new, GlobalPlugin *entry );
static int globalPluginFind ( const char *search, GlobalPlugin *entry );
static void globalPluginonInsert ( GlobalPlugin *entry );
static void globalPluginFree ( GlobalPlugin *entry );
static void globalPluginPrint ( GlobalPlugin *entry );

static int loadPlugin ( const char *filename );

static void guiPluginonInsert ( GUIPlugin *entry )
{
	if ( entry->init )
	{
		LNCdebug ( LNC_INFO, 0, "Initializing %s dialog plugin", entry->type );
		entry->init();
	}

	LNCdebug ( LNC_INFO, 0, "Loaded \"%s\" plugin", entry->type );
}

static void guiPluginFree ( GUIPlugin *entry )
{
	int i;

	if ( entry->cleanUp )
	{
		LNCdebug ( LNC_INFO, 0, "Cleaning up %s dialog plugin", entry->type );
		entry->cleanUp();
	}

	if ( entry->libraries )
	{
		for ( i = 0; entry->libraries[ i ]; ++i )
			free ( entry->libraries[ i ] );
		free ( entry->libraries );
	}

	free( entry->filename );

	LNCdebug ( LNC_INFO, 0, "Unloaded %s plugin", entry->type );

	if ( dlclose( entry->handle ) < 0 )
		LNCdebug( LNC_ERROR, 0, "guiPlugFree() -> dlclose(%s): %s", entry->type, dlerror() );
}

static int guiPluginCompare ( GUIPlugin *new, GUIPlugin *entry )
{
	return strcmp ( new->type, entry->type );
}

static int guiPluginFind ( const char *search, GUIPlugin *entry )
{
	int cmp;

	cmp = strcmp ( search, entry->type );
	if ( cmp == 0 )
		return 0;

	return strcmp( search, entry->filename );
}

static void guiPluginPrint ( GUIPlugin *entry )
{
	LNCdebug ( LNC_INFO, 0, "%s: %s", entry->type, entry->description );
}


static void networkPluginonInsert ( NetworkPlugin *entry )
{
	if ( entry->init )
	{
		LNCdebug ( LNC_INFO, 0, "Initializing %s network plugin", entry->networkName );
		entry->init();
	}

	LNCdebug ( LNC_INFO, 0, "Loaded \"%s\" plugin", entry->networkName );
}

static void networkPluginFree ( NetworkPlugin *entry )
{
	if ( entry->cleanUp )
	{
		LNCdebug ( LNC_INFO, 0, "Cleaning up %s network plugin", entry->networkName );
		entry->cleanUp();
	}

	free( entry->filename );

	LNCdebug ( LNC_INFO, 0, "Unloaded %s plugin", entry->networkName );

	if ( dlclose ( entry->handle ) < 0 )
		LNCdebug ( LNC_ERROR, 0, "netPlugFree() -> dlclose(%s): %s", entry->networkName, dlerror() );
}

static int networkPluginCompare ( NetworkPlugin *new, NetworkPlugin *entry )
{
	return strcmp ( new->networkName, entry->networkName );
}

static int networkPluginFind ( const char *search, NetworkPlugin *entry )
{
	int cmp;

	cmp = strncmp ( search, entry->protocol, entry->protocolLength );
	if ( cmp == 0 && ( search[ entry->protocolLength ] == '\0' || search[ entry->protocolLength ] == '/' ) )
		return 0;

	cmp = strncmp ( search, entry->networkName, entry->networkNameLength );
	if ( cmp == 0 && ( search[ entry->networkNameLength ] == '\0' || search[ entry->networkNameLength ] == '/' ) )
		return 0;

	return strcmp( search, entry->filename );
}

static void networkPluginPrint ( NetworkPlugin *entry )
{
	LNCdebug ( LNC_INFO, 0, "%s: %s", entry->networkName, entry->description );
}


static void globalPluginonInsert ( GlobalPlugin *entry )
{
	if ( entry->init )
	{
		LNCdebug ( LNC_INFO, 0, "Initializing %s global plugin", entry->type );
		entry->init();
	}

	LNCdebug ( LNC_INFO, 0, "Loaded \"%s\" plugin", entry->type );
}

static void globalPluginFree ( GlobalPlugin *entry )
{
	if ( entry->cleanUp )
	{
		LNCdebug ( LNC_INFO, 0, "Cleaning up %s global plugin", entry->type );
		entry->cleanUp();
	}

	free( entry->filename );

	LNCdebug ( LNC_INFO, 0, "Unloaded %s plugin", entry->type );

	if ( dlclose ( entry->handle ) < 0 )
		LNCdebug ( LNC_ERROR, 0, "globalPluginFree() -> dlclose(%s): %s", entry->type, dlerror() );
}

static int globalPluginCompare ( GlobalPlugin *new, GlobalPlugin *entry )
{
	return strcmp ( new->type, entry->type );
}

static int globalPluginFind ( const char *search, GlobalPlugin *entry )
{
	int cmp;

	cmp = strcmp ( search, entry->type );
	if ( cmp == 0 )
		return 0;

	return strcmp( search, entry->filename );
}

static void globalPluginPrint ( GlobalPlugin *entry )
{
	LNCdebug ( LNC_INFO, 0, "%s: %s", entry->type, entry->description );
}

char **findLibraryDependencies ( const char *path, int providePath )
{
	int status;
	int i;
	char *output;
	char *cpy;
	char *ptr1;
	char *begin;
	char *end;
	char *token;
	char exec[ ARG_MAX ];
	char **libraries;

	snprintf ( exec, ARG_MAX, "bash -c \"ldd %s | awk '{print $3}' | grep /\"", path );
	output = execute ( exec, STDIN_FILENO | STDOUT_FILENO, NULL, &status );
	if ( status != 0 || !output )
	{
		LNCdebug ( LNC_ERROR, 0, "findLibraryDependencies( %s ) -> execute( %s ): exit status = %d, output = \"%s\"", path, exec, status, output );
		if ( output )
			free( output );
		return NULL;
	}

	libraries = malloc ( LIB_MAX * sizeof ( char * ) );
	if ( !libraries )
	{
		LNCdebug ( LNC_ERROR, 1, "findLibraryDependencies( %s ) -> malloc()", path );
		free( output );
		return NULL;
	}

	cpy = output;
	for ( i = 0; ( token = strtok_r ( cpy, "\n", &ptr1 ) ) && ( i < LIB_MAX - 1 ); ++i )
	{
 		cpy = NULL;
		if ( !providePath ) {
			begin = strrchr ( token, '/' );
			if ( !begin )
				begin = token;
			else
				begin += 1;

			end = strstr ( begin, ".so" );
			if ( end )
			* end = '\0';

			libraries[ i ] = strdup ( begin );
		} else
			libraries[ i ] = strdup( token );
	}
	libraries[ i ] = NULL;

	LNCdebug ( LNC_DEBUG, 0, "findLibraryDependencies( %s ): this library links to ->", path );
	for ( i = 0; libraries[ i ]; ++i )
		LNCdebug ( LNC_DEBUG, 0, "%s", libraries[ i ] );

	free( output );

	return libraries;
}

int browselistInitialized = 0;

int dlcloseByPath( const char *path ) {
	void *handle;
	int retval;

	printf( "dlcloseByPath(): closing %s\n", path );
	handle = dlopen( path, RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND );
	if ( !handle ) {
		LNCdebug( LNC_ERROR, 0, "dlcloseByPath( %s ) -> dlopen( %s, RTLD_NOLOAD ): %s", path, path, dlerror() );
		return -EXIT_FAILURE;
	}

	retval = dlclose( handle );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 0, "dlcloseByPath( %s ) -> dlclose(): %s", path, dlerror() );
		return retval;
	}

	retval = dlclose( handle );
	if ( retval < 0 ) {
		LNCdebug( LNC_ERROR, 0, "dlcloseByPath( %s ) -> dlclose(): %s", path, dlerror() );
		return retval;
	}

	return EXIT_SUCCESS;
}

void unloadLibraryDependencies( unsigned long soHandles[][ MAX_SO_HANDLES ] ) {
	int i;

	for ( i = 0; ( i < MAX_SO_HANDLES ) && ( soHandles[ 0 ][ i ] != 0 ); ++i ) {
		if ( dlclose( ( void * ) soHandles[ 0 ][ i ] ) < 0 )
			LNCdebug( LNC_ERROR, 1, "unloadLibraryDependencies() ->dlclose()" );
	}
}

int loadLibraryDependencies( const char *filename, unsigned long soHandles[][ MAX_SO_HANDLES ] ) {
	char path[ PATH_MAX ];
	char **libraries;
	int retval;
	int i;
	int j;
	int cnt;
	void *handle;

	snprintf( path, PATH_MAX, "%s/%s", LNC_PLUGIN_DIR, filename );
	libraries = findLibraryDependencies( path, 1 );
	if ( !libraries ) {
		LNCdebug( LNC_ERROR, 1, "loadLibraryDependencies( %s ) -> findLibraryDependencies( %s, 1)", filename, path );
		return -EXIT_FAILURE;
	}

	retval = EXIT_SUCCESS;
	cnt = 0;
	for ( i = 0; libraries[ i ]; ++i ) {
		handle = dlopen( libraries[ i ], RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND );
		if ( !handle ) {
			LNCdebug( LNC_ERROR, 0, "loadLibraryDependencies( %s ) -> dlopen( %s, RTLD_GLOBAL | RTLD_NOW ): %s", path, libraries[ i ], dlerror() );
			for ( j = 0; j < i; ++j ) {
				dlcloseByPath( libraries[ j ] );
			}

			retval = -EXIT_FAILURE;
			goto out;
		}

		soHandles[ 0 ][ cnt ] = ( unsigned long ) handle;
		++cnt;
	}

out:
	for ( i = 0; libraries[ i ]; ++i )
		free( libraries[ i ] );
	free( libraries );

	return retval;
}

static int loadPlugin ( const char *filename )
{
	void * handle;
	void * ( *initFunc ) ( void );
	char *ptr;
	char URL[ PATH_MAX ];
	char path[ PATH_MAX ];
	char tmp[ NAME_MAX + 1 ];
	int i;
	int retval;
	int firstLoad = 1;
	int loadSymbolsGlobally = 0;
	unsigned long soHandles[ MAX_SO_HANDLES ];
	Service *found;
	NetworkPlugin *networkPlugin;
	GUIPlugin *guiPlugin;
	GlobalPlugin *globalPlugin;

	lockLNCConfig( READ_LOCK );
	if ( strstr( networkConfig.DisabledPlugins, filename ) ) {
		LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): Plugin disabled by config, skipping", filename );
		errno = EPERM;
		unlockLNCConfig();
		return -EPERM;
	}
	unlockLNCConfig();

	if ( networkPluginsOnly && strncmp( filename, "lib_network_plugin_", 19 ) != 0 ) {
		LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): networkPluginsOnly is set, skipping", filename );
		errno = EPERM;
		return -EPERM;
	}

	if ( enabledPlugins ) {
		snprintf( tmp, NAME_MAX + 1, "%s", filename );
		ptr = strchr( tmp, '.' );
		if ( ptr )
			*ptr = '\0';

		if ( !strstr( enabledPlugins, tmp ) ) {
			LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): only plugins %s should be loaded, skipping", filename, enabledPlugins );
			errno = EPERM;
			return -EPERM;
		}
	}

	if ( strncmp( filename, "lib_gui_plugin_", 15 ) == 0 ) {
		guiPlugin = findGuiPluginUnlocked( filename );
		if ( guiPlugin ) {
			putGuiPlugin( guiPlugin );
			LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): Plugin already loaded, skipping", filename );
			return EXIT_SUCCESS;
		}
	} else if ( strncmp( filename, "lib_global_plugin_", 18 ) == 0 ) {
		globalPlugin = findGlobalPluginUnlocked( filename );
		if ( globalPlugin ) {
			putGlobalPlugin( globalPlugin );
			LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): Plugin already loaded, skipping", filename );
			return EXIT_SUCCESS;
		}
	} else if ( strncmp( filename, "lib_network_plugin_", 19 ) == 0 ) {
		networkPlugin = findNetworkPluginUnlocked( filename );
		if ( networkPlugin ) {
			putNetworkPlugin( networkPlugin );
			LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): Plugin already loaded, skipping", filename );
			return EXIT_SUCCESS;
		}
	} else {
		LNCdebug( LNC_ERROR, 0, "loadPlugin( %s ): %s is not a valid plugin", filename, filename );
		return -EXIT_FAILURE;
	}

	sprintf ( path, "%s/%s", LNC_PLUGIN_DIR, filename );

loadAgain:
	if ( firstLoad ) {
		memset( soHandles, 0, sizeof( soHandles ) );
		retval = loadLibraryDependencies( filename, &soHandles );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 0, "loadPlugin( %s ) -> loadLibraryDependencies( %s ): Could not load library dependencies", filename, filename );
			return -EXIT_FAILURE;
		}

		handle = dlopen ( path, RTLD_LOCAL | RTLD_LAZY/* | RTLD_DEEPBIND*/ );
	} else {
		if ( loadSymbolsGlobally )
			handle = dlopen ( path, RTLD_GLOBAL | RTLD_NOW/* | RTLD_DEEPBIND*/ );
		else
			handle = dlopen ( path, RTLD_LOCAL | RTLD_NOW/* | RTLD_DEEPBIND*/ );
	}

	if ( !handle )
	{
		LNCdebug ( LNC_ERROR, 0, "loadPlugin() -> dlopen(%s, %s ): %s", path, loadSymbolsGlobally ? "RTLD_GLOBAL | RTLD_NOW" : "RTLD_LOCAL | RTLD_NOW", dlerror() );
		unloadLibraryDependencies( &soHandles );
		return -EXIT_FAILURE;
	}

	if ( ( initFunc = ( void * ( * ) ( void ) ) dlsym ( handle, "giveNetworkPluginInfo" ) ) != NULL )
	{
		NetworkPlugin * plugin;

		plugin = initFunc();
		if ( !plugin )
		{
			LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ) -> initFunc( giveNetworkPluginInfo ): Plugin didn't return any information about itself", path );
			dlclose ( handle );
			unloadLibraryDependencies( &soHandles );
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}


		if ( firstLoad )
		{
			if ( plugin->dependencies )
			{
				for ( i = 0; plugin->dependencies[ i ]; ++i )
				{
					LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): forfilling dependency %s", path, plugin->dependencies[ i ] );
					retval = loadPlugin ( plugin->dependencies[ i ] );
					if ( retval < 0 )
					{
						LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ) -> loadPlugin( %s ): Failed to load dependency, skipping", filename, plugin->dependencies[ i ] );
						dlclose ( handle );
						unloadLibraryDependencies( &soHandles );
						return -EXIT_FAILURE;
					}
				}
			}

			if ( plugin->global )
				loadSymbolsGlobally = 1;

			firstLoad = 0;
			dlclose ( handle );
			goto loadAgain;
		}

		plugin->handle = handle;
		plugin->filename = strdup( filename );
		plugin->networkNameLength = strlen ( plugin->networkName );
		plugin->protocolLength = strlen ( plugin->protocol );
		memcpy( plugin->soHandles, soHandles, sizeof( plugin->soHandles ) );

		if ( plugin->mount || plugin->type == USE_BROWSELIST )
		{
			Service * service;

			pthread_once ( &browselistInit, initBrowselist );
			snprintf( URL, PATH_MAX, "network://%s", plugin->protocol );
			lockList( browselist, WRITE_LOCK );
			found = findListEntryUnlocked( browselist, URL );
			if ( !found ) {
				service = newServiceEntry ( 0, NULL, NULL, NULL, strdup ( plugin->networkName ), strdup ( plugin->protocol ) );
				addListEntryUnlocked( browselist, service );
			} else
				putListEntry( browselist, found );
			unlockList( browselist );
			browselistInitialized = 1;
		}

		addListEntryUnlocked ( networkPluginList, plugin );
	}
	else if ( ( initFunc = ( void * ( * ) ( void ) ) dlsym ( handle, "giveGUIPluginInfo" ) ) != NULL )
	{
		GUIPlugin * plugin;

		plugin = initFunc();
		if ( !plugin )
		{
			LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ) -> initFunc( giveGUIPluginInfo ): Plugin didn't return any information about itself", path );
			dlclose ( handle );
			unloadLibraryDependencies( &soHandles );
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}

		if ( firstLoad )
		{
			if ( plugin->dependencies )
			{
				for ( i = 0; plugin->dependencies[ i ]; ++i )
				{
					LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): forfilling dependency %s", path, plugin->dependencies[ i ] );
					retval = loadPlugin ( plugin->dependencies[ i ] );
					if ( retval < 0 )
					{
						LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ) -> loadPlugin( %s ): Failed to load dependency, skipping", filename, plugin->dependencies[ i ] );
						firstLoad = 0;
						dlclose ( handle );
						unloadLibraryDependencies( &soHandles );
						return -EXIT_FAILURE;
					}
				}
			}

			if ( plugin->global )
				loadSymbolsGlobally = 1;

			firstLoad = 0;
			dlclose ( handle );
			goto loadAgain;
		}

		memcpy( plugin->soHandles, soHandles, sizeof( plugin->soHandles ) );
		plugin->libraries = findLibraryDependencies ( path, 0 );
		plugin->handle = handle;
		plugin->filename = strdup( filename );
		addListEntryUnlocked ( guiPluginList, plugin );
	}
	else if ( ( initFunc = ( void * ( * ) ( void ) ) dlsym ( handle, "giveGlobalPluginInfo" ) ) != NULL )
	{
		GlobalPlugin * plugin;

		plugin = initFunc();
		if ( !plugin )
		{
			LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ) -> initFunc( giveGlobalPluginInfo ): Plugin didn't return any information about itself", path );
			dlclose ( handle );
			unloadLibraryDependencies( &soHandles );
			errno = ENOSYS;
			return -EXIT_FAILURE;
		}

		if ( firstLoad )
		{
			if ( plugin->dependencies )
			{
				for ( i = 0; plugin->dependencies[ i ]; ++i )
				{
					LNCdebug ( LNC_INFO, 0, "loadPlugin( %s ): forfilling dependency %s", path, plugin->dependencies[ i ] );
					retval = loadPlugin ( plugin->dependencies[ i ] );
					if ( retval < 0 )
					{
						LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ) -> loadPlugin( %s ): Failed to load dependency, skipping", filename, plugin->dependencies[ i ] );
						dlclose ( handle );
						unloadLibraryDependencies( &soHandles );
						return -EXIT_FAILURE;
					}
				}
			}

			if ( plugin->global )
				loadSymbolsGlobally = 1;

			firstLoad = 0;
			dlclose ( handle );
			goto loadAgain;
		}

		memcpy( plugin->soHandles, soHandles, sizeof( plugin->soHandles ) );
		plugin->handle = handle;
		plugin->filename = strdup( filename );
		addListEntryUnlocked ( globalPluginList, plugin );
	}
	else
	{
		LNCdebug ( LNC_ERROR, 0, "loadPlugin( %s ): This type of plugin is not supported", path );
		dlclose ( handle );
		unloadLibraryDependencies( &soHandles );
	}

	return EXIT_SUCCESS;
}

static void createGUIPluginList ( void )
{
	guiPluginList = initList ( guiPluginList, guiPluginCompare, guiPluginFind, guiPluginFree, guiPluginPrint, guiPluginonInsert, NULL, NULL );
}

static void createNetworkPluginList ( void )
{
	networkPluginList = initList ( networkPluginList, networkPluginCompare, networkPluginFind, networkPluginFree, networkPluginPrint, networkPluginonInsert, NULL, NULL );
}

static void createGlobalPluginList ( void )
{
	globalPluginList = initList ( globalPluginList, globalPluginCompare, globalPluginFind, globalPluginFree, globalPluginPrint, globalPluginonInsert, NULL, NULL );
}

int networkOnly = 0;
pthread_once_t pluginListsInit = PTHREAD_ONCE_INIT;

void initPluginLists( void ) {
	createNetworkPluginList();
	createGUIPluginList();
	createGlobalPluginList();
}

int loadPlugins ( void )
{
	char file[ PATH_MAX ], *ext;
	struct dirent *buf, *entry;
	struct stat statbuf;
	DIR *dir;

	LNCdebug( LNC_INFO, 0, "loadPlugins()" );

	pthread_once( &pluginListsInit, initPluginLists );

	lockList( globalPluginList, WRITE_LOCK );
	lockList( guiPluginList, WRITE_LOCK );
	lockList( networkPluginList, WRITE_LOCK );

	buf = malloc ( sizeof ( struct dirent ) + ( sizeof ( char ) * NAME_MAX ) );
	if ( !buf )
	{
		LNCdebug ( LNC_ERROR, 1, "loadPlugins() -> malloc( %d )", sizeof ( struct dirent ) + ( sizeof ( char ) * NAME_MAX ) );
		unlockList( globalPluginList );
		unlockList( guiPluginList );
		unlockList( networkPluginList );
		return -EXIT_FAILURE;
	}

	dir = opendir ( LNC_PLUGIN_DIR );
	if ( !dir )
	{
		LNCdebug ( LNC_ERROR, 1, "loadPlugins() -> opendir(%s)", LNC_PLUGIN_DIR );
		unlockList( globalPluginList );
		unlockList( guiPluginList );
		unlockList( networkPluginList );
		return -ENOENT;
	}

	while ( ( readdir_r ( dir, buf, &entry ) == 0 ) )
	{
		if ( !entry )
			break;

		sprintf ( file, "%s/%s", LNC_PLUGIN_DIR, entry->d_name );
		if ( ( stat ( file, &statbuf ) == 0 ) && S_ISREG ( statbuf.st_mode ) &&
		        ( ext = strrchr ( entry->d_name, '.' ) ) &&
		        ( strcmp ( ext, ".so" ) == 0 ) )
		{
				loadPlugin ( entry->d_name );
		}
	}
	closedir ( dir );
	free( buf );

	unlockList( globalPluginList );
	unlockList( guiPluginList );
	unlockList( networkPluginList );

	return EXIT_SUCCESS;
}

int unloadPlugin( const char *filename ) {
	char *ptr;
	char *save;
	char *token;
	char pluginsToUnload[ PATH_MAX ];
	unsigned long soHandles[ MAX_SO_HANDLES ];
	int unloaded;
	int i;
	int len;
	int retval;
	NetworkPlugin *networkPlugin, *networkEntry;
	GUIPlugin *guiPlugin, *guiEntry;
	GlobalPlugin *globalPlugin, *globalEntry;

	pluginsToUnload[ 0 ] = '\0';
	len = 0;
	forEachListEntry( guiPluginList, guiEntry ) {
		if ( !guiEntry->dependencies )
			continue;

		for ( i = 0; guiEntry->dependencies[ i ]; ++i ) {
			if ( strcmp( filename, guiEntry->dependencies[ i ] ) != 0 )
				continue;

			if ( !strstr( pluginsToUnload, guiEntry->filename ) ) {
				if ( len >= ( PATH_MAX - NAME_MAX ) ) {
					LNCdebug( LNC_ERROR, 0, "unloadPlugin( %s ): To many plugins to unload %s", filename, pluginsToUnload );
					return -EXIT_FAILURE;
				}

				strlcpy( pluginsToUnload+len, guiEntry->filename, PATH_MAX - len  );
				len += strlen( guiEntry->filename );
				strcpy( pluginsToUnload+len, "," );
				len += 1;
			}
		}
	}

	forEachListEntry( globalPluginList, globalEntry ) {
		if ( !globalEntry->dependencies )
			continue;

		for ( i = 0; globalEntry->dependencies[ i ]; ++i ) {
			if ( strcmp( filename, globalEntry->dependencies[ i ]) != 0 )
				continue;

			if ( !strstr( pluginsToUnload, globalEntry->filename ) ) {
				if ( len >= ( PATH_MAX - NAME_MAX ) ) {
					LNCdebug( LNC_ERROR, 0, "unloadPlugin( %s ): To many plugins to unload %s", filename, pluginsToUnload );
					return -EXIT_FAILURE;
				}

				strlcpy( pluginsToUnload+len, globalEntry->filename, PATH_MAX - len );
				len += strlen( globalEntry->filename );
				strcpy( pluginsToUnload+len, "," );
				len += 1;
			}
		}
	}

	forEachListEntry( networkPluginList, networkEntry ) {
		if ( !networkEntry->dependencies )
			continue;

		for ( i = 0; networkEntry->dependencies[ i ]; ++i ) {
			if ( strcmp( filename, networkEntry->dependencies[ i ] ) != 0 )
				continue;

			if ( !strstr( pluginsToUnload, networkEntry->filename ) ) {
				if ( len >= ( PATH_MAX - NAME_MAX ) ) {
					LNCdebug( LNC_ERROR, 0, "unloadPlugin( %s ): To many plugins to unload %s", filename, pluginsToUnload );
					return -EXIT_FAILURE;
				}

				strlcpy( pluginsToUnload+len, networkEntry->filename, PATH_MAX - len );
				len += strlen( networkEntry->filename );
				strcpy( pluginsToUnload+len, "," );
				len += 1;
			}
		}
	}

	ptr = pluginsToUnload;
	while ( ( token = strtok_r( ptr, ",", &save ) ) ) {
		ptr = NULL;
		LNCdebug( LNC_INFO, 0, "unloadPlugin( %s ): unloading dependency %s ", filename, token );
		retval = unloadPlugin( token );
		if ( retval < 0 ) {
			LNCdebug( LNC_ERROR, 1, "unloadPlugin( %s ) -> unloadPlugin( %s ): Failed to unload dependency", filename, token );
			return -EXIT_FAILURE;
		}
	}

	unloaded = 0;
	if ( strncmp( filename, "lib_gui_plugin_", 15 ) == 0 ) {
		guiPlugin = findGuiPluginUnlocked( filename );
		if ( guiPlugin ) {
			memcpy( soHandles, guiPlugin->soHandles, sizeof( soHandles ) );
			delListEntryUnlocked( guiPluginList, guiPlugin );
			putGuiPlugin( guiPlugin );
			unloaded = 1;
		}
	} else if ( strncmp( filename, "lib_global_plugin_", 18 ) == 0 ) {
		globalPlugin = findGlobalPluginUnlocked( filename );
		if ( globalPlugin ) {
			memcpy( soHandles, globalPlugin->soHandles, sizeof( soHandles ) );
			delListEntryUnlocked( globalPluginList, globalPlugin );
			putGlobalPlugin( globalPlugin );
			unloaded = 1;
		}
	} else if ( strncmp( filename, "lib_network_plugin_", 19 ) == 0 ){
		networkPlugin = findNetworkPluginUnlocked( filename );
		if ( networkPlugin ) {
			memcpy( soHandles, networkPlugin->soHandles, sizeof( soHandles ) );
			delListEntryUnlocked( networkPluginList, networkPlugin );
			putNetworkPlugin( networkPlugin );
			unloaded = 1;
		}
	} else {
		LNCdebug( LNC_ERROR, 0, "unloadPlugin( %s ): %s is not a valid plugin", filename, filename );
		return -EXIT_FAILURE;
	}

	if ( unloaded ) {
		unloadLibraryDependencies( &soHandles );
		LNCdebug ( LNC_INFO, 0, "unloadPlugin( %s ): unloaded plugin %s", filename, filename );
	} else
		LNCdebug ( LNC_INFO, 0, "unloadPlugin( %s ): plugin %s not loaded", filename, filename );

	return EXIT_SUCCESS;
}

void unloadPlugins()
{
//	NetworkPlugin * plugin;

	if ( browselistInitialized )
		cleanupBrowselist();


//	lockList ( networkPluginList, WRITE_LOCK );
//	forEachListEntry ( networkPluginList, plugin )
//		atomic_set ( &plugin->list.count, 1 );
//	unlockList ( networkPluginList );

	clearList ( networkPluginList );
	clearList ( guiPluginList );
	clearList ( globalPluginList );
}

void reloadPlugins( void ) {
	char *ptr;
	char *cpy;
	char *token;
	char buffer[ PATH_MAX ];

	loadPlugins();

	lockLNCConfig ( READ_LOCK );
	snprintf( buffer, PATH_MAX, "%s", networkConfig.DisabledPlugins );
	unlockLNCConfig();

	lockList( globalPluginList, WRITE_LOCK );
	lockList( guiPluginList, WRITE_LOCK );
	lockList( networkPluginList, WRITE_LOCK );

	cpy = buffer;
	while ( ( token = strtok_r( cpy, ",", &ptr ) ) ) {
		cpy = NULL;
		unloadPlugin( token );
	}

	unlockList( globalPluginList );
	unlockList( guiPluginList );
	unlockList( networkPluginList );

	printLoadedPlugins();
}

void printLoadedPlugins( void ) {
	printList ( networkPluginList );
	printList ( guiPluginList );
	printList ( globalPluginList );
}

int updatePluginConfiguration ( const char *path, const char *filename, int event )
{
	char *ptr;
	char buffer[ BUFSIZ ];

	LNCdebug ( LNC_DEBUG, 0, "updatePluginConfiguration(): path = %s, filename = %s, event = %s", path, filename, printFileMonitorEvent( event, buffer, BUFSIZ ) );

	ptr = strrchr( filename, '.' );
	if ( !ptr || strcmp( ptr, ".so" ) != 0 )
		return EXIT_SUCCESS;

	if ( strncmp( filename, "lib_network_plugin_", 19 ) != 0 && strncmp( filename, "lib_gui_plugin_", 15 ) != 0 && strncmp( filename, "lib_global_plugin_", 18 ) != 0 )
		return EXIT_SUCCESS;

	lockList( globalPluginList, WRITE_LOCK );
	lockList( guiPluginList, WRITE_LOCK );
	lockList( networkPluginList, WRITE_LOCK );
	LNCdebug ( LNC_INFO, 0, "updatePluginConfiguration(): path = %s, filename = %s, event = %s", path, filename, printFileMonitorEvent( event, buffer, BUFSIZ ) );
	if ( event == FAMChanged ) {
		unloadPlugin( filename );
		//loadPlugin( filename );
		loadPlugins();
	} else if ( event == FAMCreated ) {
		//loadPlugin( filename );
		loadPlugins();
	} else if ( event == FAMDeleted ) {
		unloadPlugin( filename );
	}

	unlockList( globalPluginList );
	unlockList( guiPluginList );
	unlockList( networkPluginList );

	return EXIT_SUCCESS;
}

pthread_once_t syscallHookListInit = PTHREAD_ONCE_INIT;
int hooksInitialized = 0;

void initSyscallHookList ( void )
{
	syscallHookList = initList ( syscallHookList, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
	hooksInitialized = 1;
}

int registerSyscallHook ( void *hook, int syscall, int flags )
{
	struct syscallHook * new;

	pthread_once ( &syscallHookListInit, initSyscallHookList );

	new = malloc ( sizeof ( struct syscallHook ) );
	if ( !new )
	{
		LNCdebug ( LNC_ERROR, 1, "registerSyscallHook() -> malloc()" );
		return -EXIT_FAILURE;
	}

	new->hook = hook;
	new->syscall = syscall;
	new->flags = flags;
	addListEntry ( syscallHookList, new );

	return EXIT_SUCCESS;
}

void unregisterSyscallHook ( void *hook )
{
	struct syscallHook * entry;

	if ( !hooksInitialized )
		return ;

	lockList ( syscallHookList, WRITE_LOCK );
	forEachListEntry ( syscallHookList, entry ) {
		if ( entry->hook == hook )
		{
			delListEntryUnlocked ( syscallHookList, entry );
			break;
		}
	}
	unlockList ( syscallHookList );
}

void clearSyscallHooks( void ) {
	struct syscallHook * entry;

	LNCdebug( LNC_INFO, 0, "clearSyscallHooks(): clearing registered systemcall hooks" );
	if ( !hooksInitialized )
		return ;

	lockList ( syscallHookList, WRITE_LOCK );
	forEachListEntry ( syscallHookList, entry )
		delListEntryUnlocked ( syscallHookList, entry );
	unlockList ( syscallHookList );
}

NETWORK_FILE * runLNCopenHooks ( const char *URL, int flags, mode_t mode, int *replaceResult, int runMode, NETWORK_FILE *retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	NETWORK_FILE *tmp;
	NETWORK_FILE *newRetval;
	NETWORK_FILE * ( *LNCopenHook ) ( const char *, int, mode_t, int *, int, NETWORK_FILE * );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_OPEN || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCopenHook = ( NETWORK_FILE * ( * ) ( const char *, int, mode_t, int *, int, NETWORK_FILE * ) ) entry->hook;
		tmp = LNCopenHook ( URL, flags, mode, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

ssize_t runLNCreadHooks ( NETWORK_FILE *file, void *buf, size_t count, int *replaceResult, int runMode, ssize_t retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	ssize_t tmp;
	ssize_t newRetval;
	ssize_t ( *LNCreadHook ) ( NETWORK_FILE *, void *, size_t, int *, int, ssize_t );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_READ || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCreadHook = ( ssize_t ( * ) ( NETWORK_FILE *, void *, size_t, int *, int, ssize_t ) ) entry->hook;
		tmp = LNCreadHook ( file, buf, count, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

ssize_t runLNCwriteHooks ( NETWORK_FILE *file, void *buf, size_t count, int *replaceResult, int runMode, ssize_t retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	ssize_t tmp;
	ssize_t newRetval;
	ssize_t ( *LNCwriteHook ) ( NETWORK_FILE *, void *, size_t, int *, int, ssize_t );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_WRITE || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCwriteHook = ( ssize_t ( * ) ( NETWORK_FILE *, void *, size_t, int *, int, ssize_t ) ) entry->hook;
		tmp = LNCwriteHook ( file, buf, count, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

off_t runLNClseekHooks ( NETWORK_FILE *file, off_t offset, int whence, int *replaceResult, int runMode, off_t retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	off_t tmp;
	off_t newRetval;
	off_t ( *LNClseekHook ) ( NETWORK_FILE *, off_t, int, int *, int, off_t );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_LSEEK || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNClseekHook = ( off_t ( * ) ( NETWORK_FILE *, off_t, int, int *, int, off_t ) ) entry->hook;
		tmp = LNClseekHook ( file, offset, whence, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCcloseHooks ( NETWORK_FILE *file, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCcloseHook ) ( NETWORK_FILE *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_CLOSE || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCcloseHook = ( int ( * ) ( NETWORK_FILE *, int *, int, int ) ) entry->hook;
		tmp = LNCcloseHook ( file, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

NETWORK_DIR * runLNCopendirHooks ( const char *URL, unsigned int flags, int *replaceResult, int runMode, NETWORK_DIR *retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	NETWORK_DIR *tmp;
	NETWORK_DIR *newRetval;
	NETWORK_DIR * ( *LNCopendirHook ) ( const char *, unsigned int, int *, int, NETWORK_DIR * );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_OPENDIR || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCopendirHook = ( NETWORK_DIR * ( * ) ( const char *, unsigned int, int *, int, NETWORK_DIR * ) ) entry->hook;
		tmp = LNCopendirHook ( URL, flags, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCreaddirHooks ( NETWORK_DIR *dir, NETWORK_DIRENT *buffer, NETWORK_DIRENT **result, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCreaddirHook ) ( NETWORK_DIR *, NETWORK_DIRENT *, NETWORK_DIRENT **, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_READDIR || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCreaddirHook = ( int ( * ) ( NETWORK_DIR *, NETWORK_DIRENT *, NETWORK_DIRENT **, int *, int, int ) ) entry->hook;
		tmp = LNCreaddirHook ( dir, buffer, result, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCclosedirHooks ( NETWORK_DIR *dir, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCclosedirHook ) ( NETWORK_DIR *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_CLOSEDIR || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCclosedirHook = ( int ( * ) ( NETWORK_DIR *, int *, int, int ) ) entry->hook;
		tmp = LNCclosedirHook ( dir, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCstatHooks ( const char *URL, struct stat *buf, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCstatHook ) ( const char *, struct stat *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_STAT || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCstatHook = ( int ( * ) ( const char *, struct stat *, int *, int, int ) ) entry->hook;
		tmp = LNCstatHook ( URL, buf, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCmknodHooks ( const char *URL, mode_t mode, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCmknodHook ) ( const char *, mode_t, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_MKNOD || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCmknodHook = ( int ( * ) ( const char *, mode_t, int *, int, int ) ) entry->hook;
		tmp = LNCmknodHook ( URL, mode, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCunlinkHooks ( const char *URL, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCunlinkHook ) ( const char *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_UNLINK || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCunlinkHook = ( int ( * ) ( const char *, int *, int, int ) ) entry->hook;
		tmp = LNCunlinkHook ( URL, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCtruncateHooks ( const char *URL, off_t offset, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCtruncateHook ) ( const char *, off_t, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_TRUNCATE || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCtruncateHook = ( int ( * ) ( const char *, off_t, int *, int, int ) ) entry->hook;
		tmp = LNCtruncateHook ( URL, offset, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCmkdirHooks ( const char *URL, mode_t mode, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCmkdirHook ) ( const char *, mode_t, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_MKDIR || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCmkdirHook = ( int ( * ) ( const char *, mode_t, int *, int, int ) ) entry->hook;
		tmp = LNCmkdirHook ( URL, mode, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCrmdirHooks ( const char *URL, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCrmdirHook ) ( const char *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_RMDIR || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCrmdirHook = ( int ( * ) ( const char *, int *, int, int ) ) entry->hook;
		tmp = LNCrmdirHook ( URL, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

ssize_t runLNCreadlinkHooks ( const char *URL, char *buf, size_t bufsize, int *replaceResult, int runMode, ssize_t retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	ssize_t tmp;
	ssize_t newRetval;
	ssize_t ( *LNCreadlinkHook ) ( const char *, char *, size_t, int *, int, ssize_t );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_READLINK || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCreadlinkHook = ( ssize_t ( * ) ( const char *, char *, size_t, int *, int, ssize_t ) ) entry->hook;
		tmp = LNCreadlinkHook ( URL, buf, bufsize, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCrenameHooks ( const char *fromURL, const char *toURL, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCrenameHook ) ( const char *, const char *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_RENAME || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCrenameHook = ( int ( * ) ( const char *, const char *, int *, int, int ) ) entry->hook;
		tmp = LNCrenameHook ( fromURL, toURL, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCsymlinkHooks ( const char *fromURL, const char *toURL, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCsymlinkHook ) ( const char *, const char *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_SYMLINK || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCsymlinkHook = ( int ( * ) ( const char *, const char *, int *, int, int ) ) entry->hook;
		tmp = LNCsymlinkHook ( fromURL, toURL, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCchownHooks ( const char *URL, uid_t owner, gid_t group, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCchownHook ) ( const char *, uid_t, gid_t, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_CHOWN || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCchownHook = ( int ( * ) ( const char *, uid_t, gid_t, int *, int, int ) ) entry->hook;
		tmp = LNCchownHook ( URL, owner, group, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCchmodHooks ( const char *URL, mode_t mode, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCchmodHook ) ( const char *, mode_t, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_CHMOD || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCchmodHook = ( int ( * ) ( const char *, mode_t, int *, int, int ) ) entry->hook;
		tmp = LNCchmodHook ( URL, mode, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}

int runLNCutimeHooks ( const char *URL, struct utimbuf *buf, int *replaceResult, int runMode, int retval )
{
	int savedErrno;
	int replace;
	struct syscallHook * entry;
	int tmp;
	int newRetval;
	int ( *LNCutimeHook ) ( const char *, struct utimbuf *, int *, int, int );

	*replaceResult = 0;
	newRetval = retval;
	savedErrno = errno;

	if ( !hooksInitialized )
		return newRetval;

	lockList ( syscallHookList, READ_LOCK );
	forEachListEntry ( syscallHookList, entry )
	{
		if ( entry->syscall != LNC_UTIME || ! ( entry->flags & runMode ) )
			continue;

		replace = 0;
		errno = savedErrno;
		LNCutimeHook = ( int ( * ) ( const char *, struct utimbuf *, int *, int, int ) ) entry->hook;
		tmp = LNCutimeHook ( URL, buf, &replace, runMode, retval );
		if ( replace )
		{
			newRetval = tmp;
			*replaceResult = 1;
			savedErrno = errno;
		}
	}
	unlockList ( syscallHookList );

	errno = savedErrno;
	return newRetval;
}
