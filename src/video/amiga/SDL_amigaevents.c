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
#include "../../events/scancodes_amiga.h"

#include "SDL_amigavideo.h"
#include "SDL_amigawindow.h"
#include "SDL_amigaopengl.h"

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

static void
AMIGA_DispatchMouseButtons(const struct IntuiMessage *m, const SDL_WindowData *data)
{
	int i = 1, state = SDL_PRESSED;
	
	switch (m->Code)
	{
		case SELECTUP: 
			state = SDL_RELEASED;
		    // fallthrough
		case SELECTDOWN: 
			i = SDL_BUTTON_LEFT;
			break;
		case MENUUP:
			state = SDL_RELEASED;
			// fallthrough
		case MENUDOWN  :
			i = SDL_BUTTON_RIGHT; 
			break;
		case MIDDLEUP: 
			state = SDL_RELEASED;
			// fallthrough
		case MIDDLEDOWN: 
			i = SDL_BUTTON_MIDDLE; 
			break;
		default: 
			return;
	}
	
	SDL_SendMouseButton(data->window, 0, state, i);
}

static int
AMIGA_TranslateUnicode(struct IntuiMessage *m, char *buffer)
{
	int length;

#ifdef __MORPHOS__
	WCHAR keycode;

	GetAttr(IMSGA_UCS4, m, (ULONG *)&keycode);
	length = UTF8_Encode(keycode, buffer); 
#else
	struct InputEvent ie;

	ie.ie_Class = IECLASS_RAWKEY;
	ie.ie_SubClass = 0;
	ie.ie_Code  = m->Code & ~(IECODE_UP_PREFIX);
	ie.ie_Qualifier = m->Qualifier;
	ie.ie_EventAddress = NULL;

	length = MapRawKey(&ie, buffer, sizeof(buffer), 0);
#endif

	return length;
}

static void
AMIGA_DispatchRawKey(struct IntuiMessage *m, const SDL_WindowData *data)
{
	SDL_Scancode s;
	UWORD code = m->Code;
	UWORD rawkey = m->Code & 0x7F;
	
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
			if (rawkey < sizeof(amiga_scancode_table) / sizeof(amiga_scancode_table[0])) {
				s = amiga_scancode_table[rawkey];
				if (m->Code <= 127) {		
					char text[10];
					int length = AMIGA_TranslateUnicode(m, text);
					SDL_SendKeyboardKey(SDL_PRESSED, s);
					if (length > 0) {
						text[length] = '\0'; 
						SDL_SendKeyboardText(text);
					}
				} else {
					SDL_SendKeyboardKey(SDL_RELEASED, s);
				}
			}
			break;
	}
}

static void
AMIGA_MouseMove(const struct IntuiMessage *m, SDL_WindowData *data)
{
	//SDL_Mouse *mouse = SDL_GetMouse();

	if (!SDL_GetRelativeMouseMode()) 
	{
		struct Screen *s = data->win->WScreen;
		int x =(s->MouseX - data->win->LeftEdge - data->win->BorderLeft);
		int y =(s->MouseY - data->win->TopEdge - data->win->BorderTop);
		SDL_SendMouseMotion(data->window, 0, 0, x, y);

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
AMIGA_ChangeWindow(_THIS, const struct IntuiMessage *m, SDL_WindowData *data)
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
		SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_RESIZED, (data->curr_w - w->BorderLeft - w->BorderRight), (data->curr_h - w->BorderTop - w->BorderBottom));
		/*if (data->glContext)*/ {
			AMIGA_GL_ResizeContext(_this, data->window);
		}
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
AMIGA_HandleActivation(_THIS, struct IntuiMessage *m, SDL_bool activated)
{
	SDL_WindowData *data = (SDL_WindowData *)m->IDCMPWindow->UserData;
	//D("[%s]\n", __FUNCTION__);
	if(data->window) {
		if (activated) {
			SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_SHOWN, 0, 0);
			//OS4_SyncKeyModifiers(_this);
			if (SDL_GetKeyboardFocus() != data->window) {
				SDL_SetKeyboardFocus(data->window);
			}			
			SDL_SetMouseFocus(data->window);
		} else {

			if (SDL_GetKeyboardFocus() == data->window) {
				SDL_SetKeyboardFocus(NULL);
			}	
			if (SDL_GetMouseFocus() == data->window)
			{
				SDL_SetMouseFocus(NULL);
			}
		}		
		
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
			AMIGA_HandleActivation(_this, m, SDL_TRUE);
			break;

		case IDCMP_INACTIVEWINDOW:
			AMIGA_HandleActivation(_this, m, SDL_FALSE);
			break;

		case IDCMP_CHANGEWINDOW:
			AMIGA_ChangeWindow(_this, m, data);
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
