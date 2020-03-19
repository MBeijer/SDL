/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org

	MorphOS/TinyGL support by LouiSe@louise.amiga.hu

	HISTORY:

	20040507	Window title (SetCaption) fixed for TGL window

	20040505	AmigaMesa code removed
			GLAInitializeContextScreen() added for fullscreen mode

	20040501	Debug lines added

	20040306 	updated for new tinyl (AMIGA_GL_Update)

	20040202	AMIGA_GL_GetAttribute() function fixed

	20040131	code cleanup

	20040130	Sixk's patches added

	20040101	first release

*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_AMIGA

/* TinyGL implementation of SDL OpenGL support */
/* MorphOS/TinyGL support by LouiSe */

#include "SDL_error.h"
#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
/*#include "SDL_cgxgl_c.h"
#include "SDL_cgxvideo.h"
#include "mydebug.h"*/
#include "SDL_amigavideo.h"
#include "SDL_amigawindow.h"
#include "../../core/morphos/SDL_library.h"

#include <proto/exec.h>
#include <proto/tinygl.h>
#include <proto/intuition.h>
#include <tgl/gl.h>
#include <tgl/glu.h>

GLContext *__tglContext;
extern void *AmiGetGLProc(const char *proc);
//struct Screen *DefaultScreen;

//extern struct SDL_Library *PowerSDLBase;
extern struct SDL_Library *SDL2Base;

int AMIGA_GL_LoadLibrary(_THIS, const char *path)
{
	D("[%s]\n", __FUNCTION__);

	if (SDL2Base->MyTinyGLBase) {
		if (!TinyGLBase)
			TinyGLBase = OpenLibrary("tinygl.library", 50); // This is opened only once, closed only at final exit

		if (TinyGLBase) {
			*SDL2Base->MyTinyGLBase = TinyGLBase;
			return 0;
		}
	}

	SDL_SetError("Failed to open minigl.library");

	return -1;
}

void *AMIGA_GL_GetProcAddress(_THIS, const char *proc)
{
	void *func = NULL;

	func = AmiGetGLProc(proc);

	D("[%s] proc %s func 0x%08lx\n", __FUNCTION__, proc, func);

	return func;
}

void AMIGA_GL_UnloadLibrary(_THIS)
{
	D("[%s]\n", __FUNCTION__);

	if (SDL2Base->MyTinyGLBase && *SDL2Base->MyTinyGLBase && TinyGLBase) {
		CloseLibrary(TinyGLBase);
		*SDL2Base->MyTinyGLBase = TinyGLBase = NULL;
	}
}

SDL_GLContext AMIGA_GL_CreateContext(_THIS, SDL_Window * window)
{
	SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
	SDL_VideoData *vd = data->videodata;
	BYTE fullscreen = data->winflags & SDL_AMIGA_WINDOW_FULLSCREEN;

	GLContext *glcont = GLInit();

	D("[%s] context 0x%08lx\n", __FUNCTION__, glcont);

	if (glcont) {
		int success;
		struct TagItem tgltags[] =
		{
			{TAG_IGNORE, 0},
			{TGL_CONTEXT_STENCIL, TRUE}, // TODO check if stencil and depth are needed
			{TAG_DONE}
		};

		if (fullscreen) {
			tgltags[0].ti_Tag = TGL_CONTEXT_SCREEN;
			tgltags[0].ti_Data = (IPTR)vd->CustomScreen;
		} else {
			tgltags[0].ti_Tag = TGL_CONTEXT_WINDOW;
			tgltags[0].ti_Data = (IPTR)data->win;
		}

		success = GLAInitializeContext(glcont, tgltags);

		D("[%s] success %d\n", __FUNCTION__, success);

		if (success) {
			*SDL2Base->MyGLContext = __tglContext = glcont;
			return glcont;
		}

		GLClose(glcont);
	} else {
		SDL_SetError("Couldn't create TinyGL/OpenGL context");
	}

	return NULL;
}

int
AMIGA_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
	D("[%s] context 0x%08lx\n", __FUNCTION__, context);

	if (window && context) {
		*SDL2Base->MyGLContext = __tglContext = context;
		return 0;
	}

	return -1;
}

void AMIGA_GL_GetDrawableSize(_THIS, SDL_Window * window, int *w, int *h)
{
	D("[%s]\n", __FUNCTION__);

	if (window) {
		SDL_WindowData * data = window->driverdata;

		if (w) {
			*w = data->win->Width - data->win->BorderLeft - data->win->BorderRight;
			D("[%s] w %d\n", __FUNCTION__, *w);
		}

		if (h) {
			*h = data->win->Height - data->win->BorderTop - data->win->BorderBottom;
			D("[%s] h %d\n", __FUNCTION__, *h);
		}
	}
}

int AMIGA_GL_SetSwapInterval(_THIS, int interval)
{
	D("[%s]\n", __FUNCTION__);

	return 0; // pretend to succeed
}

int AMIGA_GL_GetSwapInterval(_THIS)
{
	SDL_VideoData *data = _this->driverdata;

	D("[%s]\n", __FUNCTION__);

	// full screen double buffering is always vsynced
	return data->CustomScreen != NULL ? 1 : 0;
}

int AMIGA_GL_SwapWindow(_THIS, SDL_Window * window)
{
	//D("[%s]\n", __FUNCTION__);

	// TODO check the window context
	GLASwapBuffers(__tglContext);
	return 0;
}

void
AMIGA_GL_DeleteContext(_THIS, SDL_GLContext context)
{
	D("[%s] context 0x%08lx\n", __FUNCTION__, context);

	if (context) {
		GLADestroyContext((GLContext *)context);
	}
}

int AMIGA_GL_ResizeContext(_THIS, SDL_WindowData *data)
{
	//SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
	SDL_VideoData *vd = data->videodata;
	int success;

	D("[%s]\n", __FUNCTION__);

	if (__tglContext == NULL || vd->CustomScreen) {
		// only for window contexts and __tglContext exist
		return -1;
	}

	// TODO check the window context
	success = GLAReinitializeContextWindowed(__tglContext, data->win);
	D("[%s] success %d\n", __FUNCTION__, success);

	return success ? 0 : -1;
}

#endif
