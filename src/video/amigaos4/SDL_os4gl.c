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
#include "SDL_config.h"

/*
 * GL support for AmigaOS 4.0 using MiniGL
 *
 * (c) 2005      Thomas Frieden
 * (c) 2005-2006 Richard Drummond
 */

#if SDL_VIDEO_OPENGL

#include "SDL_os4video.h"
#include "SDL_os4utils.h"
#include "SDL_os4blit.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <graphics/blitattr.h>

#include <GL/gl.h>
#include <mgl/gl.h>

//#define DEBUG
#include "../../main/amigaos4/SDL_os4debug.h"

static struct MiniGLIFace *IMiniGL = 0;
static struct Library *MiniGLBase = 0;

extern struct IntuitionIFace *SDL_IIntuition;
extern struct GraphicsIFace  *SDL_IGraphics;

extern void SDL_Quit(void);

/* The client program needs access to this context pointer
 * to be able to make GL calls. This presents no problems when
 * it is statically linked against libSDL, but when linked
 * against a shared library version this will require some
 * trickery.
 */
struct GLContextIFace *mini_CurrentContext = 0;

static int os4video_GetPixelDepth(_THIS)
{
	int depth = _this->gl_config.buffer_size;

	if (depth < 16) {
		depth = 16;
	}

	return depth;
}

static struct BitMap *
os4video_AllocateBitMap(_THIS, int width, int height, int depth)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;

	dprintf("Allocating bitmap %d*%d*%d\n", width, height, depth);

	return SDL_IGraphics->AllocBitMapTags(
		width,
		height,
		depth,
		BMATags_Displayable, TRUE,
		BMATags_Friend, hidden->win->RPort->BitMap,
		TAG_DONE);
}

SDL_bool
os4video_AllocateOpenGLBuffers(_THIS, int width, int height)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;
	int depth = os4video_GetPixelDepth(_this);

	if (hidden->frontBuffer)
	{
		SDL_IGraphics->FreeBitMap(hidden->frontBuffer);
		hidden->frontBuffer = NULL;
	}

	if (hidden->backBuffer)
	{
		SDL_IGraphics->FreeBitMap(hidden->backBuffer);
		hidden->backBuffer = NULL;
	}

	if (!(hidden->frontBuffer = os4video_AllocateBitMap(_this, width, height, depth)))
	{
		dprintf("Fatal error: Can't allocate memory for OpenGL bitmap\n");
		SDL_Quit();
		return SDL_FALSE;
	}

	if (!(hidden->backBuffer = os4video_AllocateBitMap(_this, width, height, depth)))
	{
		SDL_IGraphics->FreeBitMap(hidden->frontBuffer);

		dprintf("Fatal error: Can't allocate memory for OpenGL bitmap\n");
		SDL_Quit();
		return SDL_FALSE;
	}

	hidden->IGL->MGLUpdateContextTags(
					MGLCC_FrontBuffer, hidden->frontBuffer,
					MGLCC_BackBuffer, hidden->backBuffer,
					TAG_DONE);

	hidden->IGL->GLViewport(0, 0, width, height);

	return SDL_TRUE;
}


/*
 * Open MiniGL and initialize GL context
 */
int
os4video_GL_Init(_THIS)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;
	int width, height, depth;

	dprintf("Initializing MiniGL (window %p)...\n", hidden->win);

	if (hidden->IGL)
	{
		// Happens when toggling fullscreen mode
		dprintf("Old OpenGL context exists\n");
		return 0;
	}

	if (MiniGLBase)
	{
		dprintf("MiniGLBase already open\n");
	}
	else
	{
		MiniGLBase = IExec->OpenLibrary("minigl.library", 2);

		if (!MiniGLBase)
		{
			dprintf("Failed to open minigl.library\n");
			SDL_SetError("Failed to open minigl.library");
			hidden->OpenGL = FALSE;
			return -1;
		}
	}

	if (IMiniGL)
	{
		dprintf("IMiniGL already open\n");
	}
	else
	{
		IMiniGL = (struct MiniGLIFace *)IExec->GetInterface(MiniGLBase, "main", 1, NULL);

		if (!IMiniGL)
		{
			dprintf("Failed to obtain IMiniGL\n");
			SDL_SetError("Failed to obtain minigl.library interface");
			hidden->OpenGL = FALSE;
			IExec->CloseLibrary(MiniGLBase);

			return -1;
		}
	}

	SDL_IIntuition->GetWindowAttrs(hidden->win, WA_InnerWidth, &width, WA_InnerHeight, &height, TAG_DONE);
	
	depth = os4video_GetPixelDepth(_this);

	if (!(hidden->frontBuffer = os4video_AllocateBitMap(_this, width, height, depth)))
	{
		dprintf("Failed to allocate bitmap\n");
		SDL_SetError("Failed to allocate a Bitmap for the front buffer");
		return -1;
	}

	if (!(hidden->backBuffer = os4video_AllocateBitMap(_this, width, height, depth)))
	{
		dprintf("Failed to allocate bitmap\n");
		SDL_SetError("Failed to allocate a Bitmap for the back buffer");
		SDL_IGraphics->FreeBitMap(hidden->frontBuffer);
		return -1;
	}

	hidden->IGL = IMiniGL->CreateContextTags(
		MGLCC_PrivateBuffers,   2,
		MGLCC_FrontBuffer,      hidden->frontBuffer,
		MGLCC_BackBuffer,       hidden->backBuffer,
		MGLCC_Buffers,          2,
		MGLCC_PixelDepth,       depth,
		MGLCC_StencilBuffer,    TRUE,
		MGLCC_VertexBufferSize, 1 << 17,
		TAG_DONE);

	if (hidden->IGL)
	{
		_this->gl_config.driver_loaded = 1;

		hidden->IGL->GLViewport(0, 0, width, height);
		hidden->OpenGL = TRUE; // TODO: is this flag needed at all?

		mglMakeCurrent(hidden->IGL);
		mglLockMode(MGL_LOCK_SMART);

		return 0;
	}
	else
	{
		_this->gl_config.driver_loaded = 0;

		dprintf("Failed to create MiniGL context\n");
		SDL_SetError("Failed to create MiniGL context");
	}

	return -1;
}

void
os4video_GL_Term(_THIS)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;

	dprintf("Here\n");

	if (hidden->OpenGL)
	{
		if (hidden->frontBuffer)
		{
			SDL_IGraphics->FreeBitMap(hidden->frontBuffer);
			hidden->frontBuffer = NULL;
		}

		if (hidden->backBuffer)
		{
			SDL_IGraphics->FreeBitMap(hidden->backBuffer);
			hidden->backBuffer = NULL;
		}

		hidden->IGL->DeleteContext();
		hidden->IGL = NULL;

		IExec->DropInterface((struct Interface *)IMiniGL);
		IExec->CloseLibrary(MiniGLBase);

		IMiniGL = NULL;
		MiniGLBase = NULL;

		_this->gl_config.driver_loaded = 0;

		hidden->OpenGL = FALSE;
	}
}

int
os4video_GL_GetAttribute(_THIS, SDL_GLattr attrib, int* value)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;

	SDL_PixelFormat pf;
	PIX_FMT pixelFormat;

	pixelFormat = hidden->screenNativeFormat;

	if (!os4video_PIXFtoPF(&pf, pixelFormat))
	{
		dprintf("Pixel format conversion failed\n");
		SDL_SetError("Pixel format conversion failed");
		return -1;
	}

	switch (attrib)
	{
		case SDL_GL_RED_SIZE:
			*value = 8-pf.Rloss;
			return 0;

		case SDL_GL_GREEN_SIZE:
			*value = 8-pf.Gloss;
			return 0;

		case SDL_GL_BLUE_SIZE:
			*value = 8-pf.Bloss;
			return 0;

		case SDL_GL_ALPHA_SIZE:
			if (pixelFormat == PIXF_A8R8G8B8 || pixelFormat == PIXF_A8B8G8R8
			 || pixelFormat == PIXF_R8G8B8A8 || pixelFormat == PIXF_B8G8R8A8)
				*value = 8;
			else
				*value = 0;
			return 0;

		case SDL_GL_DOUBLEBUFFER:
			*value = _this->gl_config.double_buffer;
			return 0;

		case SDL_GL_BUFFER_SIZE:
			*value = pf.BitsPerPixel;
			return 0;

		case SDL_GL_DEPTH_SIZE:
			*value = _this->gl_config.depth_size;
			return 0;

		case SDL_GL_STENCIL_SIZE:
			*value = _this->gl_config.stencil_size;
			return 0;

		case SDL_GL_STEREO:
		case SDL_GL_MULTISAMPLEBUFFERS:
		case SDL_GL_MULTISAMPLESAMPLES:
		case SDL_GL_ACCUM_RED_SIZE:
		case SDL_GL_ACCUM_GREEN_SIZE:
		case SDL_GL_ACCUM_BLUE_SIZE:
		case SDL_GL_ACCUM_ALPHA_SIZE:
		case SDL_GL_ACCELERATED_VISUAL:
		case SDL_GL_SWAP_CONTROL:
			*value = 0;
			break;
	}

	dprintf("Not supported attribute %d\n", attrib);
	SDL_SetError("Not supported attribute");

	return -1;
}

int
os4video_GL_MakeCurrent(_THIS)
{
	dprintf("Here\n");
	return 0;
}

void
os4video_GL_SwapBuffers(_THIS)
{
	SDL_Surface *video = SDL_VideoSurface;

	if (video)
	{
		struct SDL_PrivateVideoData *hidden = _this->hidden;

		int width, height;
		GLint buf;
		struct BitMap *bitmap;

		mglUnlockDisplay();

		SDL_IIntuition->GetWindowAttrs(hidden->win, WA_InnerWidth, &width, WA_InnerHeight, &height, TAG_DONE);

		hidden->IGL->MGLWaitGL(); /* besure all has finished before we start blitting (testing to find lockup cause */

		glGetIntegerv(GL_DRAW_BUFFER, &buf);

		bitmap = (buf == GL_BACK) ? hidden->backBuffer : hidden->frontBuffer;

		SDL_IGraphics->BltBitMapRastPort(bitmap, 0, 0, hidden->win->RPort,
			hidden->win->BorderLeft, hidden->win->BorderTop, width, height, 0xC0);

		/* copy back into front */
		SDL_IGraphics->BltBitMapTags(
			BLITA_Source,   hidden->backBuffer,
			BLITA_SrcType,  BLITT_BITMAP,
			BLITA_SrcX,     0,
			BLITA_SrcY,     0,
			BLITA_Dest,     hidden->frontBuffer,
			BLITA_DestType, BLITT_BITMAP,
			BLITA_DestX,    0,
			BLITA_DestY,    0,
			BLITA_Width,    width,
			BLITA_Height,   height,
			BLITA_Minterm,  0xC0,
			TAG_DONE);

		bitmap = hidden->frontBuffer;
		hidden->frontBuffer = hidden->backBuffer;
		hidden->backBuffer = bitmap;

		hidden->IGL->MGLUpdateContextTags(
			MGLCC_FrontBuffer,hidden->frontBuffer,
			MGLCC_BackBuffer, hidden->backBuffer,
			TAG_DONE);
	}
}

extern void *AmiGetGLProc(const char *proc);

void *
os4video_GL_GetProcAddress(_THIS, const char *proc) {
	void *func = NULL;

	if (!_this->gl_config.driver_loaded)
	{
		if (os4video_GL_Init(_this) != 0)
		{
			return NULL;
		}
	}

	func = AmiGetGLProc(proc);

	if (func)
	{
		dprintf("Function '%s' loaded\n", proc);
	}
	else
	{
		dprintf("Failed to load function '%s'\n", proc);
	}

	return func;
}

int
os4video_GL_LoadLibrary(_THIS, const char *path) {
	/* Library is always open (kuin Muumitalo) */
	_this->gl_config.driver_loaded = 1;

	return 0;
}

#endif
