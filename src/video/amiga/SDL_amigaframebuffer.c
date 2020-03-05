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

#include "SDL_amigavideo.h"

#include <cybergraphx/cybergraphics.h>
#include <intuition/intuition.h>
#include <proto/cybergraphics.h>


void
AMIGA_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
	SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

	if (data)
	{
		SDL_free(data->fb);
	}
}

int
AMIGA_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format,
                            void ** pixels, int *pitch)
{
	SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
	SDL_VideoData *vd = data->videodata;
	SDL_Framebuffer *fb;
	Uint32 fmt;
	int bpr;

	/* Free the old framebuffer surface */
	AMIGA_DestroyWindowFramebuffer(_this, window);

	switch (vd->sdlpixfmt)
	{
		case SDL_PIXELFORMAT_INDEX8:
			fmt = SDL_PIXELFORMAT_INDEX8;
			bpr = (window->w + 15) & ~15;
			break;

		default:
			fmt = SDL_PIXELFORMAT_ARGB8888;
			bpr = ((window->w * 4) + 15) & ~15;
			break;
	}

	*format = fmt;
	*pitch = bpr;

	data->fb = fb = SDL_malloc(sizeof(SDL_Framebuffer) + bpr * window->h);

	if (fb)
	{
		fb->bpr = bpr;
		fb->pixfmt = fmt;

		*pixels = fb->buffer;
	}
	else
	{
		return SDL_OutOfMemory();
	}

	return 0;
}

int
AMIGA_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
	SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

	if (data->win)
	{
		SDL_Framebuffer *fb = data->fb;
		struct RastPort *rp = data->win->RPort;
		int i, x, y, w, h, offx, offy;

		offx = data->win->BorderLeft;
		offy = data->win->BorderTop;

		for (i = 0; i < numrects; ++i)
		{
			int dx, dy;

			x = rects[i].x;
			y = rects[i].y;
			w = rects[i].w;
			h = rects[i].h;

			/* Clipped? */
			if (w <= 0 || h <= 0 || (x + w) <= 0 || (y + h) <= 0)
				continue;

			if (x < 0)
			{
				x += w;
				w += rects[i].x;
			}

			if (y < 0)
			{
				y += h;
				h += rects[i].y;
			}

			if (x + w > window->w)
				w = window->w - x;

			if (y + h > window->h)
				h = window->h - y;

			if (y + h < 0 || y > h)
				continue;

			if (x + w < 0 || x > w)
				continue;

			dx = x + offx;
			dy = y + offy;

			switch (fb->pixfmt)
			{
				case SDL_PIXELFORMAT_INDEX8:
					if (data->videodata->CustomScreen)
						WritePixelArray(fb->buffer, x, y, fb->bpr, rp, dx, dy, w, h, RECTFMT_RAW);
					else
						WriteLUTPixelArray(fb->buffer, x, y, fb->bpr, rp, data->videodata->coltab, dx, dy, w, h, CTABFMT_XRGB8);
					break;

				default:
				case SDL_PIXELFORMAT_ARGB8888:
					WritePixelArray(fb->buffer, x, y, fb->bpr, rp, dx, dy, w, h, RECTFMT_ARGB);
					break;
			}
		}
	}

	return 0;
}


/* vi: set ts=4 sw=4 expandtab: */
