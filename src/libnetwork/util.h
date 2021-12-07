/***************************************************************************
 *   Copyright (C) 2003 by Usarin Heininga                                 *
 *   usarin@heininga.nl                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef __LNC_UTIL_H__
#define __LNC_UTIL_H__
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#define ATACHED 1
#define DETACHED 2
#define LOOP_MODE 3

#define LNC_ERROR 1
#define LNC_INFO 2
#define LNC_DEBUG 3

#define LOCK "lock ; "
	typedef struct
	{
		volatile int counter;
	}
	atomic_t;
#define ATOMIC_INIT(i)  { (i) }
#define atomic_read(v)          ((v)->counter)
#define atomic_set(v,i)         (((v)->counter) = (i))

#define functionDebug( function )	\
({ \
	char __buf[ BUFSIZ ];	\
	typeof( function ) retval;	\
	retval = function;	\
	LNCdebug( LNC_INFO, 0, ""#function" = %s", strerror_r( errno, __buf, BUFSIZ) );	\
	retval;	\
})

	static __inline__ void atomic_inc( atomic_t * v )
	{
		__asm__ __volatile__(
		    LOCK "incl %0"
	    : "=m" ( v->counter )
				    : "m" ( v->counter ) );
	}

	static __inline__ void atomic_dec( atomic_t * v )
	{
		__asm__ __volatile__(
		    LOCK "decl %0"
	    : "=m" ( v->counter )
				    : "m" ( v->counter ) );
	}

	static __inline__ int atomic_dec_and_test( atomic_t * v )
	{
		unsigned char c;

		__asm__ __volatile__(
		    LOCK "decl %0; sete %1"
	    : "=m" ( v->counter ), "=qm" ( c )
				    : "m" ( v->counter ) : "memory" );
		return c != 0;
	}

	static __inline__ int atomic_inc_and_test( atomic_t * v )
	{
		unsigned char c;

		__asm__ __volatile__(
		    LOCK "incl %0; sete %1"
	    : "=m" ( v->counter ), "=qm" ( c )
				    : "m" ( v->counter ) : "memory" );
		return c != 0;
	}

	extern FILE *networkclient_stdout;

	u_int64_t getCurrentTime( void );
	u_int32_t getProgramTime( void );
	char *strdup_len( const char *src, int len );
	char *strcpy_len( char *dest, const char *src, int len );
	size_t strlcpy( char *dst, const char *src, size_t size );
	char *replaceCharacter( char *string, char which, char with );
	char *replaceString( char *string, char *which, char *with );
	int64_t charToLong( char *string );
	int argumentParser( char * arg_string, char * args[] );
	char *execute( char * prog, unsigned int mode, char **environment, int * exitStatus );
	void *createThread( void * ( *start_routine ) ( void * ), void * arg, int mode );
	void killThread( pthread_t * tid );
	void signalThread( pthread_t * tid, int signal );

	void *createTSDBuffer( size_t size, pthread_key_t *key, int forceAllocation );
	void freeTSDBuffers( void *ignore );
	void freeAllTSDBuffers( void );
	void __LNCdebug( int debugLevel, int useErrno, const char * format, ... );

	char *getProcessName( pid_t pid );

#ifdef _DISABLE_DEBUG
#define LNCdebug( level, errno, format... ) do {} while ( 0 )
#else
#define LNCdebug( level, errno, format... ) __LNCdebug( level, errno, format )
#endif

	unsigned int unpackToInt( unsigned char * src );
	void packToChar( unsigned char * dest, unsigned int src );

	char *strcpy_len( char * dest, const char * src, int len );
	char *strdup_len( const char * src, int len );

#ifdef __cplusplus
}
#endif
#endif
