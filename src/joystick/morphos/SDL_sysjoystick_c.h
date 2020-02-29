/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SDL_SYSJOYSTICK_C_H
#define SDL_SYSJOYSTICK_C_H

#ifndef EXEC_LISTS_H
#include <exec/lists.h>
#endif

typedef unsigned int uint32;
typedef signed int int32;

#define MAX_JOYSTICKS       32

#define MAX_AXES            8
#define MAX_BUTTONS         16
#define MAX_HATS            8

struct sensordata
{
	struct MsgPort *sensorport;
	struct MinList  sensorlist;
	int             joystick_count;

	struct Task    *notifytask;
};

/*struct joystick_hwdata
{
	struct MinNode node;
	struct MinList notifylist;

	ULONG attached;
	SDL_JoystickGUID guid;
	TEXT name[0];
};*/

struct joystick_hwdata
{
	struct MinNode node;
	struct MinList notifylist;

	ULONG attached;
	SDL_JoystickGUID guid;
	TEXT name[0];
	
    //AIN_DeviceHandle   *handle;
    //APTR                context;

    uint32              axisBufferOffset[MAX_AXES];
    int32               axisData[MAX_AXES];
    TEXT                axisName[MAX_AXES][32];

    uint32              buttonBufferOffset[MAX_BUTTONS];
    int32               buttonData[MAX_BUTTONS];

    uint32              hatBufferOffset[MAX_HATS];
    int32               hatData[MAX_HATS];
};


struct sensornotify
{
	struct MinNode node;
	APTR   notifyptr;
	ULONG  type;
	DOUBLE values[0];
};

#endif
