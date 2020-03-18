/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

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

	20040131 - code cleanup

*/

/* TinyGL implementation of SDL OpenGL support */

#include "../../SDL_internal.h"

#ifndef _SDL_amigaopengl_h
#define _SDL_amigaopengl_h

/*#include <tgl/gl.h>
#include <tgl/glu.h>
#include <proto/tinygl.h>*/

/* OpenGL functions */
extern int AMIGA_GL_LoadLibrary(_THIS, const char *path);
extern void *AMIGA_GL_GetProcAddress(_THIS, const char *proc);
extern void AMIGA_GL_UnloadLibrary(_THIS);
extern SDL_GLContext AMIGA_GL_CreateContext(_THIS, SDL_Window * window);
extern int AMIGA_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);
extern void AMIGA_GL_GetDrawableSize(_THIS, SDL_Window * window, int *w, int *h);
extern int AMIGA_GL_SetSwapInterval(_THIS, int interval);
extern int AMIGA_GL_GetSwapInterval(_THIS);
extern int AMIGA_GL_SwapWindow(_THIS, SDL_Window * window);
extern void AMIGA_GL_DeleteContext(_THIS, SDL_GLContext context);

/* Non-SDL functions */
extern int AMIGA_GL_ResizeContext(_THIS, SDL_WindowData *data);

#endif /* _SDL_amigaopengl_h */
