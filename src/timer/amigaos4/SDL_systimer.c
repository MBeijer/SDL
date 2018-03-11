/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/*
    SDL timer API for AmigaOS4.0.
*/

#include "SDL_error.h"
#include "SDL_timer.h"
#include "../SDL_timer_c.h"

#include "../../main/amigaos4/SDL_os4timer_c.h"

//#define DEBUG
//#include "../../main/amigaos4/SDL_os4debug.h"

void SDL_StartTicks(void)
{
	/* Do nothing. We set the start time elsewhere */
}

Uint32 SDL_GetTicks(void)
{
	return os4timer_GetTicks();
}

void SDL_Delay(Uint32 ms)
{
	os4timer_WaitUntil(SDL_GetTicks() + ms);
}

#include "SDL_thread.h"

/* Data to handle a single periodic alarm */
static SDL_bool timer_alive = SDL_FALSE;
static SDL_Thread *timer_thread = NULL;

static int RunTimer(void *unused)
{
	Uint32 next_tick = SDL_GetTicks();

	while (timer_alive)
	{
		if (SDL_timer_running)
		{
			SDL_ThreadedTimerCheck();
		}

		os4timer_WaitUntil(++next_tick);
	}

	return 0;
}

/* This is only called if the event thread is not running */
int SDL_SYS_TimerInit(void)
{
	timer_alive = SDL_TRUE;
	timer_thread = SDL_CreateThread(RunTimer, NULL);

	if (timer_thread == NULL)
	{
		SDL_SetError("Failed to create timer thread");
		return -1;
	}

	return SDL_SetTimerThreaded(1);
}

void SDL_SYS_TimerQuit(void)
{
	timer_alive = SDL_FALSE;

	if (timer_thread)
	{
		SDL_WaitThread(timer_thread, NULL);
		timer_thread = NULL;
	}
}

int SDL_SYS_StartTimer(void)
{
	SDL_SetError("Internal logic error: AmigaOS uses threaded timer");
	return -1;
}

void SDL_SYS_StopTimer(void)
{
	return;
}
