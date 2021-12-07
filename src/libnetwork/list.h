/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __LNC_LIST_H__
#define __LNC_LIST_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "util.h"

#define READ_LOCK 0
#define WRITE_LOCK 1

	struct list {
		void *data;
		struct list *prev, *next;
		atomic_t count;
	};
	typedef int ( *listFunc1 ) ( const void *, const void * );
	typedef void ( *listFunc2 ) ( void * );
	typedef void ( *listFunc3 ) ( const void *, const void * );

#define defineList( name, dataType )													\
struct name {																\
	struct list list;														\
	void ( *Print ) ( void *data );													\
	void ( *Free ) ( void *data );													\
	int ( *Find ) ( const void *data1, const void *data2 );										\
	int ( *Compare ) ( const void *data1, const void *data2 );									\
	void ( *onInsert ) ( void *data );												\
	void ( *onUpdate ) ( const void *data1, const void *data2 );									\
	void ( *onRemove ) ( void *data );												\
	pthread_rwlock_t lock ;														\
	dataType *type;															\
};

#define initList( name, listCompare, listFind, listFree, listPrint, listOnInsert, listOnUpdate ,listOnRemove )				\
({																	\
	int __retval__;															\
																	\
	( name )->list.next = &( name )->list;												\
	( name )->list.prev = &( name )->list;												\
	( name )->Compare = ( listFunc1 ) listCompare;											\
	( name )->Find = ( listFunc1 ) listFind;											\
	( name )->Free = ( listFunc2 ) listFree;											\
	( name )->Print = ( listFunc2 ) listPrint;											\
	( name )->onInsert = ( listFunc2 ) listOnInsert;										\
	( name )->onUpdate = ( listFunc3 ) listOnUpdate;										\
	( name )->onRemove = ( listFunc2 ) listOnRemove;										\
																	\
	__retval__ = pthread_rwlock_init( &( name )->lock , NULL );									\
	if ( __retval__ != 0 ) {													\
		errno = __retval__;													\
		LNCdebug( LNC_ERROR, 1, "initList() -> pthread_rwlock_init()" );							\
	}																\
																	\
	name;																\
})

#define offsetOfStructure(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define member_type(type, member) __typeof__( ((type *)0)->member )

#define test_container_of(ptr, type, member) ({                      									\
        const member_type(type, member) *__mptr = (ptr);        									\
        (type *)( (char *)__mptr - offsetOfStructure(type,member) );									\
})

#define container_of( ptr , type) test_container_of( ptr, type, list )
#define listHolder(data) ((struct list *)((char *)(data)-(unsigned long)(&((struct list *)0)->data)))

#define forEachListEntry( head, entry )													\
	struct list *__tmp__ ## entry;													\
																	\
	for ( ( entry ) = container_of(( head )->list.next, __typeof__( *( entry ) )), __tmp__  ## entry = ( head )->list.next->next;	\
	( entry ) != container_of( &( head )->list, __typeof__( *( entry ) ));								\
	entry = container_of(__tmp__ ## entry, __typeof__( *(entry))), __tmp__  ## entry = __tmp__ ## entry->next )

#define forEachListEntryReverse( head, entry )												\
	struct list *__tmp_r__ ## entry;												\
																	\
	for ( ( entry ) = container_of(( head )->list.prev, __typeof__( *( entry ) )), __tmp_r__ ## entry = ( head )->list.prev->prev;	\
	( entry ) != container_of( &( head )->list, __typeof__( *( entry ) ));								\
	entry = container_of(__tmp_r__ ## entry, __typeof__( *(entry))), __tmp_r__ ## entry = __tmp_r__ ## entry->prev )


#define lockList( head, flags )											\
 ({														\
	int retval;												\
														\
	if ( flags == READ_LOCK )										\
        	retval = pthread_rwlock_rdlock( &( head )->lock );						\
    	else													\
        	retval = pthread_rwlock_wrlock( &( head )->lock );						\
														\
	if ( retval != 0 ) {											\
		errno = retval;											\
		LNCdebug(LNC_ERROR, 1, "lockList( %s ) -> pthread_rwlock_lock()", #head );			\
		printf( "lockList( %s ) -> pthread_rwlock_lock()", #head );					\
		abort();											\
	}													\
														\
	retval;													\
})

#define unlockList( head )											\
({														\
    	int retval;												\
														\
	retval = pthread_rwlock_unlock( &( head )->lock );							\
	if ( retval != 0 ) {											\
		errno = retval;											\
		LNCdebug(LNC_ERROR, 1, "unlockList( %s ) -> pthread_rwlock_unlock()", #head );			\
		printf( "unlockList( %s ) -> pthread_rwlock_unlock()", #head );					\
		abort();											\
	}													\
	retval;													\
})

#define freeListEntry( head, entry )										\
({														\
	if ( ( head )->Free )											\
		( head )->Free( entry );									\
	else													\
		free( entry );											\
})

#define addListEntryUnlocked( head, data )									\
({														\
	__typeof__( *(data) ) *__entry1__ = container_of(( head )->list.next, __typeof__( *( __entry1__ ) ) );	\
	int cmp = -1;												\
														\
	if ( ( head )->Compare ) {										\
		forEachListEntry( head, __entry1__ ) {								\
			cmp = ( head )->Compare( data, __entry1__ );						\
        		if ( cmp < 0 || cmp == 0 )								\
            			break;										\
    		}												\
	} else													\
		__entry1__ = container_of(( head )->list.prev, __typeof__( *( __entry1__ ) ) );			\
														\
	atomic_set( &( data )->list.count, 1 );									\
	if ( cmp != 0 && ( head )->onInsert )									\
		( head )->onInsert( data );									\
														\
	if ( cmp == 0 /*&& ( head )->onUpdate*/ ) {								\
		printf( "\nDOUBLE ENTRY\n\n");									\
		abort();											\
		( head )->onUpdate( data, __entry1__ );								\
		( data )->list.next = __entry1__->list.next;							\
		( data )->list.prev = __entry1__->list.prev;							\
		__entry1__->list.prev->next = &( data )->list;							\
		__entry1__->list.next->prev = &( data )->list;							\
		__entry1__->list.next = __entry1__->list.prev = NULL;						\
		putListEntry( head, __entry1__ );								\
	} else {												\
    		( data )->list.next = &__entry1__->list;							\
    		( data )->list.prev = __entry1__->list.prev;							\
    		__entry1__->list.prev->next = &( data )->list;							\
    		__entry1__->list.prev = &( data )->list;							\
	}													\
/*	if ( cmp == 0 ) {											\
		( data )->list.next = __entry1__->list.next;							\
		__entry1__->list.next->prev = &( data )->list;							\
		putListEntry( head, __entry1__ );								\
	}*/													\
})

#define addListEntry( head, data )										\
({														\
	lockList( head, WRITE_LOCK );										\
	addListEntryUnlocked( head, data );									\
	unlockList( head );											\
})

#define getListEntry( head, entry ) atomic_inc( &( entry )->list.count );

#define putListEntry( head, entry )										\
({														\
	if ( atomic_dec_and_test( &( entry )->list.count ) ) {							\
		freeListEntry( head, entry );									\
	}													\
})

#define delListEntry( head, entry )										\
({														\
	lockList( head, WRITE_LOCK );										\
	( entry )->list.prev->next = ( entry )->list.next;							\
	( entry )->list.next->prev = ( entry )->list.prev;							\
	( entry )->list.next = ( entry )->list.prev = NULL;							\
														\
	if ( ( head )->onRemove )										\
    		( head )->onRemove( entry );									\
	putListEntry( head, entry );										\
	unlockList( head );											\
})

#define delListEntryUnlocked( head, entry )									\
({														\
	( entry )->list.prev->next = ( entry )->list.next;							\
	( entry )->list.next->prev = ( entry )->list.prev;							\
														\
	if ( ( head )->onRemove )										\
		( head )->onRemove( entry );									\
	putListEntry( head, entry );										\
})

#define delExpiredListEntries( head, stamp, func, data ) 							\
({														\
	__typeof__( *( head )->type ) *__entry__;								\
														\
	if ( head ) {												\
		lockList( head, WRITE_LOCK );									\
		forEachListEntry( head, __entry__ )								\
			if ( __entry__->list.timeStamp < stamp )						\
				if ( expiredHelper( func, __entry__, data ) )					\
					delListEntryUnlocked( head, __entry__ );				\
		unlockList( head );										\
	}													\
})

#define findListEntry( head, search )										\
({														\
	__typeof__( *( head )->type ) *__entry__ = NULL;							\
	lockList( head, READ_LOCK );										\
	__entry__ = findListEntryUnlocked( head, search );							\
	unlockList( head );											\
	__entry__;												\
})

#define findListEntryUnlocked( head, search )									\
({														\
	int found = 0;												\
	__typeof__( *( head )->type ) *__entry__ = NULL;							\
														\
	if ( head && ( head )->Find ) {										\
		forEachListEntry( head, __entry__ ) {								\
			if ( ( head )->Find( search, __entry__ ) == 0 ) {					\
				getListEntry( head, __entry__ );						\
				found = 1;									\
				break;										\
			}											\
		}												\
	}													\
														\
	if ( !found ) {												\
		errno = ENODEV;											\
		__entry__ = NULL;										\
	}													\
	__entry__;												\
})

/*#define getPreviousListEntry( head, entry )												\
({																			\
	container_of( ( entry )->list.prev, __typeof__( *( entry ) ) );									\
})

#define getNextListEntry( head, entry )													\
({																			\
	container_of( ( entry )->list.next, __typeof__( *( entry ) ) );									\
})*/

	/*#define clearList( head ) ({ __typeof__( *( head )->type ) *__entry__; if ( head ) { lockList( head, WRITE_LOCK ); forEachListEntry( head, __entry__ ) delListEntryUnlocked( head, __entry__ ); unlockList( head ); } })*/

#define clearList( head )											\
({														\
	__typeof__( *( head )->type ) *__entry__;								\
														\
	if ( ( head ) ) {											\
		lockList( head, WRITE_LOCK );									\
    		forEachListEntry( head, __entry__ )								\
    			delListEntryUnlocked( head, __entry__ );						\
    		unlockList( head );										\
	}													\
})

#define clearListReverse( head )										\
({														\
	__typeof__( *( head )->type ) *__entry_r__;								\
														\
	if ( head ) {												\
		lockList( head, WRITE_LOCK );									\
    		forEachListEntryReverse( head, __entry_r__ )							\
    			delListEntryUnlocked( head, __entry_r__ );						\
    		unlockList( head );										\
	}													\
})

#define printList( head )											\
({														\
	__typeof__( *( head )->type ) *__entry__;								\
														\
	if ( head && ( head )->Print ) {									\
		lockList( head, READ_LOCK );									\
		forEachListEntry( head, __entry__ )								\
    			( head )->Print( __entry__ );								\
    		unlockList( head );										\
	}													\
})

#define isListEmpty( head )											\
({														\
	int __retval__;												\
														\
	do {													\
		if ( & ( head )->list == ( head )->list.next )							\
			__retval__ = 1;										\
		else												\
			__retval__ = 0;										\
	} while ( 0 );												\
														\
	 __retval__;												\
})

#ifdef __cplusplus
}
#endif
#endif
