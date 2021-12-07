/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/

#ifndef __NETWORKCLIENT_DISPLAY_H__
#define __NETWORKCLIENT_DISPLAY_H__

#include "user.h"

#include <sys/types.h>
#include <pwd.h>

dev_t getActiveConsole( void );
int lookupProcessLibrary( pid_t pid, const char **libs );
char *getCommandLineArguments( pid_t pid );
char *getKdeinitProcessName( pid_t pid );
char *getActiveDisplayDevice( void  );
char *getDisplayDevice( pid_t PID, struct passwd *user );
char *getXauthority( pid_t PID, char *display );
int Dialog( const char *URL, struct auth *auth, pid_t PID );
#endif
