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
#include "../../SDL_internal.h"

#include "SDL_scancode.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"

#include "SDL_amigavideo.h"
#include "SDL_amigawindow.h"

#include "SDL_timer.h"
#include "SDL_syswm.h"

#include <devices/rawkeycodes.h>
#include <intuition/extensions.h>
#include <intuition/intuimessageclass.h>
#include <libraries/screennotify.h>
#include <rexx/errors.h>
#include <rexx/storage.h>
#include <workbench/workbench.h>

#include <proto/alib.h>
#include <proto/commodities.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/locale.h>
#include <proto/screennotify.h>

static SDL_Scancode
AMIGA_ScanCodeToSDL(UWORD code)
{
	switch (code)
	{
		case RAWKEY_KP_0: return SDL_SCANCODE_KP_0;
		case RAWKEY_KP_1: return SDL_SCANCODE_KP_1;
		case RAWKEY_KP_2: return SDL_SCANCODE_KP_2;
		case RAWKEY_KP_3: return SDL_SCANCODE_KP_3;
		case RAWKEY_KP_4       : return SDL_SCANCODE_KP_4;
		case RAWKEY_KP_5       : return SDL_SCANCODE_KP_5;
		case RAWKEY_KP_6       : return SDL_SCANCODE_KP_6;
		case RAWKEY_KP_7       : return SDL_SCANCODE_KP_7;
		case RAWKEY_KP_8       : return SDL_SCANCODE_KP_8;
		case RAWKEY_KP_9       : return SDL_SCANCODE_KP_9;
		case RAWKEY_KP_DECIMAL : return SDL_SCANCODE_KP_PERIOD;
		case RAWKEY_KP_ENTER   : return SDL_SCANCODE_KP_ENTER;
		case RAWKEY_KP_MINUS   : return SDL_SCANCODE_KP_MINUS;
		case RAWKEY_KP_DIVIDE  : return SDL_SCANCODE_KP_DIVIDE;
		case RAWKEY_KP_MULTIPLY: return SDL_SCANCODE_KP_MULTIPLY;
		case RAWKEY_KP_PLUS    : return SDL_SCANCODE_KP_PLUS;

		case RAWKEY_BACKSPACE  : return SDL_SCANCODE_BACKSPACE;
		case RAWKEY_TAB        : return SDL_SCANCODE_TAB;
		case RAWKEY_RETURN     : return SDL_SCANCODE_RETURN;
		case RAWKEY_ESCAPE     : return SDL_SCANCODE_ESCAPE;
		case RAWKEY_DELETE     : return SDL_SCANCODE_DELETE;
		case RAWKEY_INSERT     : return SDL_SCANCODE_INSERT;
		case RAWKEY_PAGEUP     : return SDL_SCANCODE_PAGEUP;
		case RAWKEY_PAGEDOWN   : return SDL_SCANCODE_PAGEDOWN;
		case RAWKEY_HOME       : return SDL_SCANCODE_HOME;
		case RAWKEY_END        : return SDL_SCANCODE_END;
		case RAWKEY_UP         : return SDL_SCANCODE_UP;
		case RAWKEY_DOWN       : return SDL_SCANCODE_DOWN;
		case RAWKEY_LEFT       : return SDL_SCANCODE_LEFT;
		case RAWKEY_RIGHT      : return SDL_SCANCODE_RIGHT;

		case RAWKEY_F1         : return SDL_SCANCODE_F1;
		case RAWKEY_F2         : return SDL_SCANCODE_F2;
		case RAWKEY_F3         : return SDL_SCANCODE_F3;
		case RAWKEY_F4         : return SDL_SCANCODE_F4;
		case RAWKEY_F5         : return SDL_SCANCODE_F5;
		case RAWKEY_F6         : return SDL_SCANCODE_F6;
		case RAWKEY_F7         : return SDL_SCANCODE_F7;
		case RAWKEY_F8         : return SDL_SCANCODE_F8;
		case RAWKEY_F9         : return SDL_SCANCODE_F9;
		case RAWKEY_F10        : return SDL_SCANCODE_F10;
		case RAWKEY_F11        : return SDL_SCANCODE_F11;
		case RAWKEY_F12        : return SDL_SCANCODE_F12;

		case RAWKEY_HELP       : return SDL_SCANCODE_HELP;

		case RAWKEY_LSHIFT     : return SDL_SCANCODE_LSHIFT;
		case RAWKEY_RSHIFT     : return SDL_SCANCODE_RSHIFT;
		case RAWKEY_LALT       : return SDL_SCANCODE_LALT;
		case RAWKEY_RALT       : return SDL_SCANCODE_RALT;
		case RAWKEY_LAMIGA     : return SDL_SCANCODE_LGUI;
		case RAWKEY_RAMIGA     : return SDL_SCANCODE_RGUI;
		case RAWKEY_CONTROL    : return SDL_SCANCODE_LCTRL;
		case RAWKEY_CAPSLOCK   : return SDL_SCANCODE_CAPSLOCK;

		case RAWKEY_SCRLOCK    : return SDL_SCANCODE_SCROLLLOCK;
		case RAWKEY_PRTSCREEN  : return SDL_SCANCODE_PRINTSCREEN;
		case RAWKEY_NUMLOCK    : return SDL_SCANCODE_NUMLOCKCLEAR;
		case RAWKEY_PAUSE      : return SDL_SCANCODE_PAUSE;

#if 0
		case RAWKEY_MEDIA1     : return SDL_SCANCODE_VOLUMEUP;
		case RAWKEY_MEDIA2     : return SDL_SCANCODE_VOLUMEDOWN;
		case RAWKEY_MEDIA3     : return SDL_SCANCODE_WWW;
		case RAWKEY_MEDIA4     : return SDL_SCANCODE_MAIL;
		case RAWKEY_MEDIA5     : return SDL_SCANCODE_CALCULATOR;
		case RAWKEY_MEDIA6     : return SDL_SCANCODE_COMPUTER;
#else
		case RAWKEY_CDTV_STOP  : return SDL_SCANCODE_AUDIOSTOP;
		case RAWKEY_CDTV_PLAY  : return SDL_SCANCODE_AUDIOPLAY;
		case RAWKEY_CDTV_PREV  : return SDL_SCANCODE_AUDIOPREV;
		case RAWKEY_CDTV_NEXT  : return SDL_SCANCODE_AUDIONEXT;
		case RAWKEY_CDTV_REW   : return SDL_SCANCODE_AC_BACK;
		case RAWKEY_CDTV_FF    : return SDL_SCANCODE_AC_FORWARD;
#endif
	}

	return SDL_SCANCODE_UNKNOWN;
}

static void
AMIGA_DispatchMouseButtons(const struct IntuiMessage *m, const SDL_WindowData *data)
{
	int i = 1, state = SDL_PRESSED;

	switch (m->Code)
	{
		case SELECTUP  : i = 1; state = SDL_RELEASED; break;
		case SELECTDOWN: i = 1; break;
		case MENUUP    : i = 2; state = SDL_RELEASED; break;
		case MENUDOWN  : i = 2; break;
		case MIDDLEUP  : i = 3; state = SDL_RELEASED; break;
		case MIDDLEDOWN: i = 3; break;
		default        : return;
	}

	SDL_SendMouseButton(data->window, 0, state, i);
}

static void
AMIGA_DispatchRawKey(struct IntuiMessage *m, const SDL_WindowData *data)
{
	SDL_Scancode sc;
	UWORD code = m->Code;

	switch (code)
	{
		case RAWKEY_NM_WHEEL_UP:
			SDL_SendMouseWheel(data->window, 0, 0, 1, SDL_MOUSEWHEEL_NORMAL);
			break;

		case RAWKEY_NM_WHEEL_DOWN:
			SDL_SendMouseWheel(data->window, 0, 0, -1, SDL_MOUSEWHEEL_NORMAL);
			break;

		case RAWKEY_NM_WHEEL_LEFT:
			SDL_SendMouseWheel(data->window, 0, -1, 0, SDL_MOUSEWHEEL_NORMAL);
			break;

		case RAWKEY_NM_WHEEL_RIGHT:
			SDL_SendMouseWheel(data->window, 0, 1, 0,  SDL_MOUSEWHEEL_NORMAL);
			break;

		case RAWKEY_NM_BUTTON_FOURTH:
			SDL_SendMouseButton(data->window, 0, SDL_PRESSED, 4);
			break;

		case RAWKEY_NM_BUTTON_FOURTH | IECODE_UP_PREFIX:
			SDL_SendMouseButton(data->window, 0, SDL_RELEASED, 4);
			break;

		default:
			sc = AMIGA_ScanCodeToSDL(code & ~(IECODE_UP_PREFIX));

			if (sc != SDL_SCANCODE_UNKNOWN)
			{
				SDL_SendKeyboardKey(code & IECODE_UP_PREFIX ? SDL_RELEASED : SDL_PRESSED, sc);
			}
			else
			{
				WCHAR keycode;
				TEXT buffer[8];
				ULONG length;

				GetAttr(IMSGA_UCS4, m, (ULONG *)&keycode);
				length = UTF8_Encode(keycode, buffer);

				D("[AMIGA_DispatchRawKey] Converted %ld (UCS-4: %ld) to UTF-8...\n", code, keycode);

				if (length > 0)
				{
					buffer[length] = '\0';
					SDL_SendKeyboardText(buffer);
				}
			}
			break;
	}
}

static void
AMIGA_MouseMove(const struct IntuiMessage *m, SDL_WindowData *data)
{
	SDL_Mouse *mouse = SDL_GetMouse();

	if (!mouse->relative_mode || mouse->relative_mode_warp)
	{
		struct Screen *s = data->win->WScreen;
		SDL_SendMouseMotion(data->window, 0, 0, s->MouseX, s->MouseY);
	}
	else
	{
		if (data->first_deltamove)
		{
			data->first_deltamove = 0;
			return;
		}

		SDL_SendMouseMotion(data->window, 0, 1, m->MouseX, m->MouseY);
	}
}

static void
AMIGA_ChangeWindow(const struct IntuiMessage *m, SDL_WindowData *data)
{
	struct Window *w = data->win;

	if (data->curr_x != w->LeftEdge || data->curr_h != w->TopEdge)
	{
		data->curr_x = w->LeftEdge;
		data->curr_y = w->TopEdge;
		SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_MOVED, data->curr_x, data->curr_y);
	}

	if (data->curr_w != w->Width || data->curr_h != w->Height)
	{
		data->curr_w = w->Width;
		data->curr_h = w->Height;
		SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_RESIZED, data->curr_w, data->curr_h);
	}
}

static void AMIGA_GadgetEvent(_THIS, const struct IntuiMessage *m)
{
	D("[%s]\n", __FUNCTION__);

	switch (((struct Gadget *)m->IAddress)->GadgetID)
	{
		case ETI_Iconify:
			AMIGA_HideApp(_this, TRUE);
			break;
	}
}

static void
AMIGA_DispatchEvent(_THIS, struct IntuiMessage *m)
{
	SDL_WindowData *data = (SDL_WindowData *)m->IDCMPWindow->UserData;

	switch (m->Class)
	{
		case IDCMP_REFRESHWINDOW:
			BeginRefresh(m->IDCMPWindow);
			SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_EXPOSED, 0, 0);
			EndRefresh(m->IDCMPWindow, TRUE);
			break;

		case IDCMP_CLOSEWINDOW:
			SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_CLOSE, 0, 0);
         break;

		case IDCMP_MOUSEMOVE:
			AMIGA_MouseMove(m, data);
			break;

		case IDCMP_MOUSEBUTTONS:
			AMIGA_DispatchMouseButtons(m, data);
			break;

		case IDCMP_RAWKEY:
			AMIGA_DispatchRawKey(m, data);
			break;

		case IDCMP_ACTIVEWINDOW:
			SDL_SetKeyboardFocus(data->window);
			SDL_SetMouseFocus(data->window);
			break;

		case IDCMP_INACTIVEWINDOW:
			if (SDL_GetKeyboardFocus() == data->window)
			{
				SDL_SetKeyboardFocus(NULL);
			}

			if (SDL_GetMouseFocus() == data->window)
			{
				SDL_SetMouseFocus(NULL);
			}
			break;

		case IDCMP_CHANGEWINDOW:
			AMIGA_ChangeWindow(m, data);
			break;

		case IDCMP_GADGETUP:
			AMIGA_GadgetEvent(_this, m);
			break;
	}
}

static void
AMIGA_CheckBrokerMsg(_THIS)
{
	SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
	CxMsg *msg;

	while ((msg = (CxMsg *)GetMsg(&data->BrokerPort)))
	{
		size_t id = CxMsgID(msg);
		size_t tp = CxMsgType(msg);

		D("[%s] check CxMsg\n", __FUNCTION__);

		ReplyMsg((APTR)msg);

		if (tp == CXM_COMMAND)
		{
			switch (id)
			{
				case CXCMD_KILL:
					SDL_SendAppEvent(SDL_QUIT);
					break;

				case CXCMD_APPEAR:
					AMIGA_ShowApp(_this);
					break;

				case CXCMD_DISAPPEAR:
					AMIGA_HideApp(_this, TRUE);
					break;
			}
		}
	}
}

static void
AMIGA_CheckScreenEvent(_THIS)
{
	SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

	for (;;)
	{
		struct ScreenNotifyMessage *snm;

		while ((snm = (struct ScreenNotifyMessage *)GetMsg(&data->ScreenNotifyPort)) != NULL)
		{
			D("[%s] check ScreenNotifyMessage\n", __FUNCTION__);

			switch ((size_t)snm->snm_Value)
			{
				case FALSE:
					AMIGA_HideApp(_this, FALSE);
					break;

				case TRUE:
					AMIGA_ShowApp(_this);
					break;
			}
		}

		if (data->WScreen)
			break;

		WaitPort(&data->ScreenNotifyPort);
	}
}

static void
AMIGA_CheckWBEvents(_THIS)
{
	SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
	struct AppMessage *msg;

	while ((msg = (struct AppMessage *)GetMsg(&data->WBPort)) != NULL)
	{
		D("[%s] check AppMessage\n", __FUNCTION__);

		if (msg->am_NumArgs == 0 && msg->am_ArgList == NULL)
			AMIGA_ShowApp(_this);

#warning object dragn drop
	}
}

void
AMIGA_PumpEvents(_THIS)
{
	SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
	struct IntuiMessage *m;

	size_t sigs = SetSignal(0, data->ScrNotifySig | data->BrokerSig | data->WBSig | data->WinSig | SIGBREAKF_CTRL_C);

	if (sigs & data->WinSig)
	{
		while ((m = (struct IntuiMessage *)GetMsg(&data->WinPort)))
		{
			AMIGA_DispatchEvent(_this, m);
			ReplyMsg((struct Message *)m);
		}
	}

	if (sigs & data->ScrNotifySig && data->ScreenNotifyHandle)
		AMIGA_CheckScreenEvent(_this);

	if (sigs & data->BrokerSig)
		AMIGA_CheckBrokerMsg(_this);

	if (sigs & data->WBSig)
		AMIGA_CheckWBEvents(_this);

	if (sigs & SIGBREAKF_CTRL_C)
		SDL_SendAppEvent(SDL_QUIT);
}


/* vi: set ts=4 sw=4 expandtab: */
