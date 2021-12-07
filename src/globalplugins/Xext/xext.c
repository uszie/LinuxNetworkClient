/***************************************************************************
 *   Copyright (C) 2003 by Usarin Heininga                                 *
 *   usarin@heininga.nl                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "util.h"
#include "plugin.h"
#include <X11/Xlib.h>

static Window getRealWindowID( Window w, int depth, Display *dpy ) {
	static Atom wm_state;
	Atom result;
	Atom type;
	int format;
	unsigned long nitems, after;
	unsigned char* prop;
	Window root, parent;
	Window* children;
	unsigned int nchildren;
	unsigned int i;
	Window ret = None;

	if ( depth > 5 ) {
		printf("no active window found\n" );
		abort();
		return None;
	}

	wm_state = XInternAtom( dpy, "WM_STATE", False );

	if ( ( result = XGetWindowProperty( dpy, w, wm_state, 0, 0, False, AnyPropertyType, &type, &format, &nitems, &after, &prop ) ) == Success ) {
		if ( prop != NULL )
			XFree( prop );

		if ( type != None ) {
printf("1 returning %d\n", ( int ) w );
			return w;
		}
	}

	if ( XQueryTree( dpy, w, &root, &parent, &children, &nchildren ) != 0 ) {
		printf( "nchildren = %d\n", nchildren );
		for ( i = 0; i < nchildren && ret == None; ++i ) {
			ret = getRealWindowID( children[ i ], depth + 1, dpy );
			printf("ret =  %d\n", ( int ) ret );
		}

		if ( children != NULL )
			XFree( children );
	}
printf("2 returning %d\n", ( int ) ret );
	return ret;
}

int getActiveWindowID( void ) {
	Window root;
	Window child;
	Display *dpy;
	int screen;
	uint mask;
	int rootX, rootY, winX, winY;
	int windowID;

	dpy = XOpenDisplay( NULL );
	if ( !dpy ) {
		LNCdebug( LNC_ERROR, 0, "getActiveWindowID() -> XOpenDisplay(): Failed to open display %s", getenv( "DISPLAY" ) );
		return 0;
	}

	screen = XDefaultScreen( dpy );

	XQueryPointer( dpy, XRootWindow( dpy, screen ), &root, &child, &rootX, &rootY, &winX, &winY, &mask );
printf( "child = %d\n root = %d\n", ( int ) child, ( int ) root );
	windowID = ( int ) getRealWindowID( child, 0, dpy );
printf("3 returning %d\n", windowID );
	XCloseDisplay( dpy );
	if ( windowID == 0 )
		windowID = child;
	LNCdebug( LNC_DEBUG, 0, "getActiveWindowID(): windowID = %d\n", windowID );
printf("4 returning %d\n", windowID );
	return windowID;
}

static const char XextType[] = "X Extensions";
static const char XextDescription[] = "This plugin provides X functions used by other plugins";
static const char *XextDependencies[] = {
	NULL
};

static GlobalPlugin Xext =
{
	.type = XextType,
 	.description = XextDescription,
 	.dependencies = XextDependencies,
	.global = 1,
};

void *giveGlobalPluginInfo( void ) {
	return & Xext;
}