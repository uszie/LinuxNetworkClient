/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_PLUGIN_H__
#define __LNC_PLUGIN_H__
#ifdef __cplusplus
extern "C" {
#endif

	typedef struct netplugin NetworkPlugin;
#include "networkclient.h"
#include "user.h"
#include "list.h"
#include "pluginhelper.h"

#define LIB_MAX					512

#define LNC_OPEN 				1
#define LNC_CLOSE				2
#define LNC_OPENDIR 				4
#define LNC_CLOSEDIR				8
#define LNC_READDIR				16
#define LNC_READ				32
#define LNC_WRITE				64
#define LNC_LSEEK				128
#define LNC_TRUNCATE				256
#define LNC_STAT				512
#define LNC_MKNOD				1024
#define LNC_UNLINK				2048
#define LNC_MKDIR				4096
#define LNC_RMDIR				8192
#define LNC_RENAME				16384
#define LNC_SYMLINK				32768
#define LNC_READLINK				65536
#define LNC_CHOWN				131072
#define LNC_CHMOD				262144
#define LNC_UTIME				524288


#define PRE_SYSCALL 				1
#define AFTER_SYSCALL 				2

#define FULL					1
#define USE_BROWSELIST				2
#define SIMPLE					3

#define MAX_SO_HANDLES				100


	typedef int ( *ScanFunction ) ( Service * service );
	typedef int ( *ScanAuthFunction ) ( Service * service, struct auth * auth );

	struct netplugin {
		int ( *init ) ( void );
		void ( *cleanUp ) ( void );
		char *filename;
		const char *networkName;
		int networkNameLength;
		const char *protocol;
		int protocolLength;
		int portNumber;
		const char *description;
		int supportsAuthentication;
		int minimumTouchDepth;
		const char **dependencies;
		unsigned long soHandles[ MAX_SO_HANDLES ];
		int sortbyDomain;
		int global;
		int type;
		void *handle;

		ScanAuthFunction mount;
		ScanFunction umount;
		ScanFunction getDomains;
		ScanFunction getHosts;
		ScanAuthFunction getShares;

		NETWORK_FILE *( *open ) ( const char *URL, int flags, mode_t mode, NETWORK_FILE *file );
		ssize_t ( *read ) ( NETWORK_FILE *file, void *buf, size_t count );
		ssize_t ( *write ) ( NETWORK_FILE *file, void *buf, size_t count );
		off_t ( *lseek ) ( NETWORK_FILE *file, off_t offset, int whence );
		int ( *close ) ( NETWORK_FILE *file );
		NETWORK_DIR *( *opendir ) ( const char *URL, unsigned int flags, NETWORK_DIR *dir );
		int ( *readdir ) ( NETWORK_DIR *dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result );
		int ( *closedir ) ( NETWORK_DIR *dir );
		int ( *stat ) ( const char *URL, struct stat *buf );
		int ( *mknod ) ( const char *URL, mode_t mode );
		int ( *unlink ) ( const char *URL );
		int ( *truncate ) ( const char *URL, off_t size );
		int ( *mkdir ) ( const char *URL, mode_t mode );
		int ( *rmdir ) ( const char *URL );
		ssize_t ( *readlink ) ( const char *URL, char *buf, size_t bufsize );
		int ( *rename ) ( const char *oldURL, const char *newURL );
		int ( *symlink ) ( const char *fromURL, const char *toURL );
		/*int ( *link ) ( const char *fromURL, const char *toURL );*/
		int ( *chown ) ( const char *URL, uid_t owner, gid_t group );
		int ( *chmod ) ( const char *URL, mode_t mode );
		int ( *utime ) ( const char *URL, struct utimbuf *buf );

		struct list list;
	};

	struct guiplugin {
		int ( *init ) ( void );
		void ( *cleanUp ) ( void );
		char *filename;
		const char *type;
		int ( *dialog ) ( const char *URL, struct auth *auth, const char *processName );
		void *handle;
		const char *description;
		char **libraries;
		const char **dependencies;
		unsigned long soHandles[ MAX_SO_HANDLES ];
		int global;
		int needsX;
		int singleShot;
		int refreshTerminalScreen;
		int libraryReference;
		struct list list;
	};
	typedef struct guiplugin GUIPlugin;

	struct globalplugin {
		int ( *init ) ( void );
		void ( *cleanUp ) ( void );
		char *filename;
		void *handle;
		const char *type;
		const char *description;
		const char **dependencies;
		unsigned long soHandles[ MAX_SO_HANDLES ];
		int global;
		void * ( *pluginRoutine ) ( void * );
		struct list list;
	};
	typedef struct globalplugin GlobalPlugin;

	struct syscallHook {
		void *hook;
		int syscall;
		int flags;
		struct list list;
	};

	defineList( networkPluginList, struct netplugin );
	defineList( guiPluginList, struct guiplugin );
	defineList( globalPluginList, struct globalplugin );
	defineList( syscallHookList, struct syscallHook );

	extern struct networkPluginList *networkPluginList;
	extern struct guiPluginList *guiPluginList;
	extern struct globalPluginList *globalPluginList;
	extern int browselistInitialized;

	int loadPlugins( void );
	int unloadPlugin( const char *filename );
	void unloadPlugins( void );
	void reloadPlugins( void );
	void printLoadedPlugins( void );
	int updatePluginConfiguration ( const char *path, const char *filename, int event );

	int registerSyscallHook( void * hook, int syscall, int flags );
	void unregisterSyscallHook( void * hook );
	void clearSyscallHooks( void );

	NETWORK_FILE * runLNCopenHooks( const char * URL, int flags, mode_t mode, int * replaceResult, int runMode, NETWORK_FILE * retval );
	ssize_t runLNCreadHooks( NETWORK_FILE * file, void * buf, size_t count, int * replaceResult, int runMode, ssize_t retval );
	ssize_t runLNCwriteHooks( NETWORK_FILE * file, void * buf, size_t count, int * replaceResult, int runMode, ssize_t retval );
	off_t runLNClseekHooks( NETWORK_FILE * file, off_t offset, int whence, int * replaceResult, int runMode, off_t retval );
	int runLNCcloseHooks( NETWORK_FILE * file, int * replaceResult, int runMode, int retval );
	NETWORK_DIR * runLNCopendirHooks( const char * URL, unsigned int flags, int * replaceResult, int runMode, NETWORK_DIR * retval );
	int runLNCreaddirHooks( NETWORK_DIR * dir, NETWORK_DIRENT *entry, NETWORK_DIRENT **result, int * replaceResult, int runMode, int retval );
	int runLNCclosedirHooks( NETWORK_DIR * dir, int * replaceResult, int runMode, int retval );
	int runLNCstatHooks( const char * URL, struct stat * buf, int * replaceResult, int runMode, int retval );
	int runLNCmknodHooks( const char * URL, mode_t mode, int * replaceResult, int runMode, int retval );
	int runLNCunlinkHooks( const char * URL, int * replaceResult, int runMode, int retval );
	int runLNCtruncateHooks( const char * URL, off_t offset, int * replaceResult, int runMode, int retval );
	int runLNCmkdirHooks( const char * URL, mode_t mode, int * replaceResult, int runMode, int retval );
	int runLNCrmdirHooks( const char * URL, int * replaceResult, int runMode, int retval );
	ssize_t runLNCreadlinkHooks( const char * URL, char * buf, size_t bufsize, int * replaceResult, int runMode, ssize_t retval );
	int runLNCrenameHooks( const char * fromURL, const char * toURL, int * replaceResult, int runMode, int retval );
	int runLNCsymlinkHooks ( const char *fromURL, const char *toURL, int *replaceResult, int runMode, int retval );
	int runLNCchownHooks( const char * URL, uid_t owner, gid_t group, int * replaceResult, int runMode, int retval );
	int runLNCchmodHooks( const char * URL, mode_t mode, int * replaceResult, int runMode, int retval );
	int runLNCutimeHooks( const char * URL, struct utimbuf * buf, int * replaceResult, int runMode, int retval );

#define findNetworkPlugin( search ) findListEntry( networkPluginList, search )
#define findNetworkPluginUnlocked( search ) findListEntryUnlocked( networkPluginList, search )
#define putNetworkPlugin( plugin ) putListEntry( networkPluginList, plugin )

#define findGlobalPlugin( search ) findListEntry( globalPluginList, search )
#define findGlobalPluginUnlocked( search ) findListEntryUnlocked( globalPluginList, search )
#define putGlobalPlugin( plugin ) putListEntry( globalPluginList, plugin )

#define findGuiPlugin( search ) findListEntry( guiPluginList, search )
#define findGuiPluginUnlocked( search ) findListEntryUnlocked( guiPluginList, search )
#define putGuiPlugin( plugin ) putListEntry( guiPluginList, plugin )

#ifdef __cplusplus
}
#endif
#endif
