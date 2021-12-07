/***************************************************************************
 *   Copyright (C) 2003 by Usarin Heininga                                 *
 *   usarin@heininga.nl                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef __LNC_KONQ_DCOP_H__
#define __LNC_KONQ_DCOP_H__
#ifdef __cplusplus
extern "C"
{
#endif
	int getActiveKonquerorWindow( void );
	int showErrorMessage( const char *caption, const char *applicationName, const char *errorMessage, int errorCode, int windowID );
#ifdef __cplusplus
}
#endif
#endif

