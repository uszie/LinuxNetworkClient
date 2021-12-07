/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_NCONFIG_H__
#define __LNC_NCONFIG_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

#include <fam.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

#include <kdb.h>
#ifndef __cplusplus
//#include <kdbbackend.h>
//#include <kdbprivate.h>
#endif

#define FILESIZE 10*BUFSIZ
#define LNC_CONFIG_DIR 1
#define LNC_CONFIG_KEY 2

#define USER_ROOT 1
#define SYSTEM_ROOT 2

extern pthread_rwlock_t LNCConfigLock;
extern pthread_once_t LNCConfigLockInit;
# if __WORDSIZE == 64
	#define int_to_void_cast ( void * ) ( int64_t )
#else
	#define int_to_void_cast ( void * )
#endif

# if __WORDSIZE == 64
	#define void_to_int_cast ( int64_t )
#else
	#define void_to_int_cast ( int )
#endif

#define lockLNCConfig( flags )									\
 ({												\
 	int retval;										\
												\
	/*void initLNCConfigLock( void ) {							\
		void resetLNCConfigLock( void ) {						\
			pthread_rwlock_init( &LNCConfigLock , NULL );				\
		}										\
		pthread_atfork( NULL, NULL, resetLNCConfigLock);				\
	}											\
												\
	pthread_once( &LNCConfigLockInit, initLNCConfigLock );*/				\
												\
	if ( flags == READ_LOCK )								\
        	retval = pthread_rwlock_rdlock( &LNCConfigLock );				\
    	else											\
        	retval = pthread_rwlock_wrlock( &LNCConfigLock );				\
												\
	if ( retval != 0 )									\
		LNCdebug(LNC_ERROR, 0, "lockLNCConfig() -> pthread_rwlock_lock()");		\
	retval;											\
})

#define unlockLNCConfig()									\
({												\
	int retval;										\
												\
	retval = pthread_rwlock_unlock( &LNCConfigLock );					\
	if ( retval != 0 )									\
		LNCdebug(LNC_ERROR, 0, "unlockLNCConfig() -> pthread_rwlock_unlock()" );	\
	retval;											\
})

	struct networkConfig {
		int LogLevel;
		char *DisabledPlugins;
		char *DefaultUsername;
		char *DefaultPassword;
		int DisablePasswordWriting;
		int BrowselistUpdateInterval;
		int UmountInterval;
		int UseBroadcast;
		char *PingNetworks;
		char *IgnoreAddresses;
		int UseFullNetworkName;
		int EnableDebugMode;
	};

	struct key {
		char name[ NAME_MAX+ 1 ];
		char parentName[ NAME_MAX + 1 ];
		char value[ BUFSIZ ];
		int type;
	};

	typedef int ( monitorFunction ) ( const char *, const char *, int );
	struct monitor {
		struct list list;
		char *filename;
		FAMRequest request;
		monitorFunction *callback;
		u_int32_t timeStamp;
	};

	extern struct networkConfig networkConfig;
	extern void *keysDBHandle;

	void freeNetworkConfiguration( void );
	int readNetworkConfiguration( void );
	int updateNetworkConfiguration( const char * path, const char * filename, int event );
	void printNetworkConfiguration( void );

	void initKeys( const char * applicationName );
	void cleanupKeys( const char * applicationName );
	void clearKeys( const char * applicationName, int mode );
	void delKey( const char * applicationName, const char * key, const int flags );
	struct key *forEveryKey2( const char *applicationName, int mode );
	void rewindKeys( const char * applicationName, int mode );
	size_t getKeyCount( const char * applicationName, int mode );
	char *getCharKey( const char * applicationName, const char * key, const char * defaultValue );
	int setCharKey( const char * applicationName, const char * key, const char * value, const int flags );
	int64_t getNumKey( const char * applicationName, const char * key, const int64_t defaultValue );
	int setNumKey( const char * applicationName, const char * key, const int64_t value, const int flags );
	/*	int write_entry(const char *filename, const char *key, const char *value, const char *group);
		void *read_entry(const char *filename, const char *key, const char *group, const char *def);
		void *read_config_entry(const char *key, const char *group, const char *def);
		int write_config_entry(const char *key, const char *value, const char *group);*/

	char *readCharEntry( const char * filename, const char * key, const char * group, const char * def );
	int readNumEntry( const char * filename, const char * key, const char * group, const int def );
	int writeCharEntry( const char * filename, const char * key, const char * value, const char * group );
	int writeNumEntry( const char * filename, const char * key, const int value, const char * group );

	ssize_t copyFileToBuffer( char **string, const char * filename );
	int copyBufferToFile( const char * string, const char * filename );

	char *removeTrailingWhites( char * string );
	char *removePreceedingWhites( char * string );
	char *removeWhites( char * string );
	char *removeTrailingSlashes( char * string );

	const char *printFileMonitorEvent( int event, char *buffer, size_t size );
	int addFileToMonitorList( const char * filename, monitorFunction * callback );
	void delFileFromMonitorList( const char *filename, monitorFunction *callback );
	void stopFileMonitor( void ) ;

#ifdef __cplusplus
}
#endif
#endif
