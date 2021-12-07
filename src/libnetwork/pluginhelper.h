/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_PLUGINHELPER_H__
#define __LNC_PLUGINHELPER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"
#include "list.h"
#include "user.h"

#include <limits.h>
#include <sys/types.h>
#include <string.h>

#define SHARES_MAX 					25
#define SERVERS_MAX 					100
#define DOMAINS_MAX 					25

#define NEEDS_ACTION 					125
#define SCANNED 						126
#define SERVICE_MOUNTED 				127

#define POINTER_COPY					0
#define FULL_COPY						1

#define PARTIAL_RESOLVE					1
#define FULL_RESOLVE					2
#define FORCE_RESCAN					4
#define FORCE_FULL_RESCAN				8
#define LAZY_UMOUNT					16
#define REMOVE_PDIR					32
#define REMOVE_PPDIR					64
#define FORCE_AUTHENTICATION 				128
#define DISABLE_AUTHENTICATION				256

#define AUTO_HOST					1
#define MANUAL_HOST					2

#define SHORT_HOSTNAME					1
#define FULL_HOSTNAME					2

#define LNC_ROOT_DIR					"/dev/shm/.LNC"
#define MAXALIASES					39

	extern char realRoot[ PATH_MAX ];
	extern int realRootLength;

	struct service {
		struct list list;
		char *network;
		char *domain;
		char *host;
		char *share;
		char *comment;
		char *aliases[ MAXALIASES ];
		volatile u_int32_t IP;
		volatile u_int32_t expireTime;
		volatile u_int32_t lastTimeUsed;
		volatile int flags;
		volatile int hostType;
		volatile int badConnection;
		volatile int status;
		volatile int error;
		pthread_rwlock_t busyLock;
	};
	typedef struct service Service;

	typedef int ( *SNPRoutine ) ( Service * service );
	typedef int ( *SNPAuthRoutine ) ( Service * service, struct auth * auth );

	defineList( browselist, struct service );
	extern struct browselist *browselist;
	extern pthread_once_t browselistInit;

	void startServiceTimer( void );
	void restartServiceTimer( void );
	void stopServiceTimer( void );

	void markAsBadURL( const char * URL );
	void initBrowselist( void );
	void cleanupBrowselist( void );
	int scanURL( const char * URL, unsigned int flags, int *usePluginSyscall );

	char *IPtoString( u_int32_t ip, char *buffer, size_t size );
	u_int32_t IPtoNumber( const char * ip );
	u_int32_t getIPAddress( const char *hostname );
	int isValidIP ( const char *IP );
	char *getHostName( u_int32_t IP, char *buffer, size_t bufferSize, int mode );
	char *getDomainName( u_int32_t IP, char *buffer, size_t bufferSize );

	char *createServicePath ( char *buffer, size_t size, Service *service );
	char *createServiceKey( char *buffer, size_t size, Service *service, int mode );
	void createRealPath( char *path, const char *URL );

	Service *newServiceEntry( u_int32_t IP, char * domain, char * host, char * share, char * comment, char * network );
	int isMountInUse( const char * path );
	int isMounted( const char * path );

	void removeExpiredBrowselistEntries( u_int32_t timeStamp, Service * service, int deepRemove );

	void updateServiceTimeStamp( Service * service );
	void updateServiceIPAddress( Service *service, u_int32_t IP );
	int updateManualHosts( const char * path, const char * filename, int event );

//#define setBusy( service ) pthread_rwlock_wrlock( &( service )->busyLock )
//#define unsetBusy( service ) service->flags = 0; pthread_rwlock_unlock( &( service )->busyLock )

#define needsAction( service ) ( ( service )->status == NEEDS_ACTION )

#define markSyscallStartTime()													\
	u_int64_t __syscall_time__;												\
																\
	__syscall_time__ = getCurrentTime();

#define handleSyscallError( __SYSCALL_URL__ )											\
	int savedErrno = errno;													\
																\
	if ( errno == ENODEV || errno == EIO || errno == EHOSTDOWN || errno == ETIMEDOUT ) {					\
		if ( errno == EIO && ( getCurrentTime() - __syscall_time__) < 2000000ULL )					\
			savedErrno = ENOMEDIUM;											\
		else														\
			markAsBadURL( __SYSCALL_URL__ );									\
	}															\
	errno = savedErrno;

#ifdef __cplusplus
}
#endif
#endif
