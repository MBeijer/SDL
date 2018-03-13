/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

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
*/

#include "SDL_config.h"

#include "SDL_os4video.h"

//#define DEBUG
#include "../../main/amigaos4/SDL_os4debug.h"

//#define DEBUG_FINDMODE		/* Define me to get verbose output when searching for a screenmode */

#include <proto/exec.h>
#include <proto/graphics.h>

#include <limits.h>

extern struct GraphicsIFace *SDL_IGraphics;

PIX_FMT
os4video_PFtoPIXF(const SDL_PixelFormat *vf)
{
	if (vf->BitsPerPixel == 8)
		return PIXF_CLUT;
	if (vf->BitsPerPixel == 24)
	{
		if (vf->Rmask == 0x00FF0000 && vf->Gmask == 0x0000FF00 && vf->Bmask == 0x000000FF)
			return PIXF_R8G8B8;
		if (vf->Rmask == 0x000000FF && vf->Gmask == 0x0000FF00 && vf->Bmask == 0x00FF0000)
			return PIXF_B8G8R8;
	}
	else if (vf->BitsPerPixel == 32)
	{
		if (vf->Rmask == 0xFF000000 && vf->Gmask == 0x00FF0000 && vf->Bmask == 0x0000FF00)
			return PIXF_R8G8B8A8;
		if (vf->Rmask == 0x00FF0000 && vf->Gmask == 0x0000FF00 && vf->Bmask == 0x000000FF)
			return PIXF_A8R8G8B8;
		if (vf->Bmask == 0x00FF0000 && vf->Gmask == 0x0000FF00 && vf->Rmask == 0x000000FF)
			return PIXF_A8B8G8R8;
		if (vf->Bmask == 0xFF000000 && vf->Gmask == 0x00FF0000 && vf->Rmask == 0x0000FF00)
			return PIXF_B8G8R8A8;
	}
	else if (vf->BitsPerPixel == 16)
	{
		if (vf->Rmask == 0xf800 && vf->Gmask == 0x07e0 && vf->Bmask == 0x001f)
			return PIXF_R5G6B5;
		if (vf->Rmask == 0x7C00 && vf->Gmask == 0x03e0 && vf->Bmask == 0x001f)
			return PIXF_R5G5B5;
	}
	else if (vf->BitsPerPixel == 15)
	{
		if (vf->Rmask == 0x7C00 && vf->Gmask == 0x03e0 && vf->Bmask == 0x001f)
			return PIXF_R5G5B5;
	}

	return PIXF_NONE;
}

BOOL
os4video_PIXFtoPF(SDL_PixelFormat *vformat, PIX_FMT pixf)
{
	vformat->Rmask = vformat->Gmask = vformat->Bmask = vformat->Amask = 0;
	vformat->Rshift = vformat->Gshift = vformat->Bshift = vformat->Ashift = 0;
	vformat->Rloss = vformat->Gloss = vformat->Bloss = vformat->Aloss = 8;

	switch(pixf)
	{
	case PIXF_CLUT:
		vformat->BitsPerPixel = 8;
		vformat->BytesPerPixel = 1;
		break;

	case PIXF_R8G8B8:
		vformat->BitsPerPixel = 24;
		vformat->BytesPerPixel = 3;

		vformat->Rmask = 0x00FF0000;
		vformat->Rshift = 16;
		vformat->Rloss = 0;

		vformat->Gmask = 0x0000FF00;
		vformat->Gshift = 8;
		vformat->Gloss = 0;

		vformat->Bmask = 0x000000FF;
		vformat->Bshift = 0;
		vformat->Bloss = 0;
		break;

	case PIXF_B8G8R8:
		vformat->BitsPerPixel = 24;
		vformat->BytesPerPixel = 3;

		vformat->Rmask = 0x000000FF;
		vformat->Rshift = 0;
		vformat->Rloss = 0;

		vformat->Gmask = 0x0000FF00;
		vformat->Gshift = 8;
		vformat->Gloss = 0;

		vformat->Bmask = 0x00FF0000;
		vformat->Bshift = 16;
		vformat->Bloss = 0;
		break;

	case PIXF_R5G6B5PC:
	case PIXF_R5G6B5:
		// We handle these equivalent and do swapping elsewhere.
		// PC format cannot be expressed by mask/shift alone
		vformat->BitsPerPixel = 16;
		vformat->BytesPerPixel = 2;

		vformat->Rmask = 0x0000F800;
		vformat->Rshift = 11;
		vformat->Rloss = 3;

		vformat->Gmask = 0x000007E0;
		vformat->Gshift = 5;
		vformat->Gloss = 2;

		vformat->Bmask = 0x0000001F;
		vformat->Bshift = 0;
		vformat->Bloss = 3;
		break;

	case PIXF_R5G5B5PC:
	case PIXF_R5G5B5:
		vformat->BitsPerPixel = 15;
		vformat->BytesPerPixel = 2;

		vformat->Rmask = 0x00007C00;
		vformat->Rshift = 10;
		vformat->Rloss = 3;

		vformat->Gmask = 0x000003E0;
		vformat->Gshift = 5;
		vformat->Gloss = 3;

		vformat->Bmask = 0x0000001F;
		vformat->Bshift = 0;
		vformat->Bloss = 3;
		break;

	case PIXF_A8R8G8B8:
		vformat->BitsPerPixel = 32;
		vformat->BytesPerPixel = 4;

		vformat->Rmask = 0x00FF0000;
		vformat->Rshift = 16;
		vformat->Rloss = 0;

		vformat->Gmask = 0x0000FF00;
		vformat->Gshift = 8;
		vformat->Gloss = 0;

		vformat->Bmask = 0x000000FF;
		vformat->Bshift = 0;
		vformat->Bloss = 0;

		vformat->Amask = 0xFF000000;
		vformat->Ashift = 24;
		vformat->Aloss = 0;
		break;

	case PIXF_A8B8G8R8:
		vformat->BitsPerPixel = 32;
		vformat->BytesPerPixel = 4;

		vformat->Rmask = 0x000000FF;
		vformat->Rshift = 0;
		vformat->Rloss = 0;

		vformat->Gmask = 0x0000FF00;
		vformat->Gshift = 8;
		vformat->Gloss = 0;

		vformat->Bmask = 0x00FF0000;
		vformat->Bshift = 16;
		vformat->Bloss = 0;

		vformat->Amask = 0xFF000000;
		vformat->Ashift = 24;
		vformat->Aloss = 0;
		break;

	case PIXF_R8G8B8A8:
		vformat->BitsPerPixel = 32;
		vformat->BytesPerPixel = 4;

		vformat->Amask = 0x000000FF;
		vformat->Ashift = 0;
		vformat->Aloss = 0;

		vformat->Bmask = 0x0000FF00;
		vformat->Bshift = 8;
		vformat->Bloss = 0;

		vformat->Gmask = 0x00FF0000;
		vformat->Gshift = 16;
		vformat->Gloss = 0;

		vformat->Rmask = 0xFF000000;
		vformat->Rshift = 24;
		vformat->Rloss = 0;
		break;

	case PIXF_B8G8R8A8:
		vformat->BitsPerPixel = 32;
		vformat->BytesPerPixel = 4;

		vformat->Amask = 0x000000FF;
		vformat->Ashift = 0;
		vformat->Aloss = 0;

		vformat->Rmask = 0x0000FF00;
		vformat->Rshift = 8;
		vformat->Rloss = 0;

		vformat->Gmask = 0x00FF0000;
		vformat->Gshift = 16;
		vformat->Gloss = 0;

		vformat->Bmask = 0xFF000000;
		vformat->Bshift = 24;
		vformat->Bloss = 0;
		break;

	default:
		dprintf("Unknown pixel format %d\n", pixf);
		return FALSE;
	}

	return TRUE;
}

static APTR
os4video_GetDisplayInfoHandle(uint32 displayId)
{
	APTR handle = SDL_IGraphics->FindDisplayInfo(displayId);

	if (!handle)
	{
		dprintf("FindDisplayInfo() failed\n");
	}

	return handle;
}

static BOOL
os4video_ReadDisplayInfo(uint32 displayId, struct DisplayInfo *dispInfo)
{
	APTR handle = os4video_GetDisplayInfoHandle(displayId);

	if (handle)
	{
		if (SDL_IGraphics->GetDisplayInfoData(handle, (UBYTE *)dispInfo, sizeof(*dispInfo), DTAG_DISP, 0) > 0)
		{
			return TRUE;
		}
		else
		{
			dprintf("GetDisplayInfoData() failed\n");
		}
	}

	return FALSE;
}

static BOOL
os4video_ReadDimensionInfo(uint32 displayId, struct DimensionInfo *dimInfo)
{
	APTR handle = os4video_GetDisplayInfoHandle(displayId);

	if (handle)
	{
		if (SDL_IGraphics->GetDisplayInfoData(handle, (UBYTE *)dimInfo, sizeof(*dimInfo), DTAG_DIMS, 0) > 0)
		{
			return TRUE;
		}
		else
		{
			dprintf("GetDisplayInfoData() failed\n");
		}
	}

	return FALSE;
}

static BOOL
os4video_IsRtgMode(uint32 displayId)
{
	struct DisplayInfo dispInfo;
	
	if (os4video_ReadDisplayInfo(displayId, &dispInfo))
	{
		if (dispInfo.PropertyFlags & DIPF_IS_RTG)
		{
			return TRUE;
		}	 
	}

	return FALSE;
}

PIX_FMT
os4video_GetPixelFormatFromMode(uint32 displayId)
{
	struct DisplayInfo dispInfo;

	if (os4video_ReadDisplayInfo(displayId, &dispInfo))
	{
		return dispInfo.PixelFormat;
	}

	return PIXF_NONE;
}

uint32
os4video_GetWidthFromMode(uint32 displayId)
{
	struct DimensionInfo dimInfo;

	if (os4video_ReadDimensionInfo(displayId, &dimInfo))
	{
		return dimInfo.Nominal.MaxX - dimInfo.Nominal.MinX + 1;
	}

	return 0;
}

uint32
os4video_GetHeightFromMode(uint32 displayId)
{
	struct DimensionInfo dimInfo;

	if (os4video_ReadDimensionInfo(displayId, &dimInfo))
	{
		return dimInfo.Nominal.MaxY - dimInfo.Nominal.MinY + 1;
	}

	return 0;
}

BOOL
os4video_PixelFormatFromModeID(SDL_PixelFormat *vformat, uint32 displayId)
{
	if (!os4video_IsRtgMode(displayId))
	{
		//dprintf("Skipping non-RTG mode ID 0x%X\n", displayId);
		return FALSE;
	}

	return os4video_PIXFtoPF(vformat, os4video_GetPixelFormatFromMode(displayId));
}

static BOOL
os4video_CmpPixelFormat(const SDL_PixelFormat *f, const SDL_PixelFormat *g)
{
	if (f->BitsPerPixel != g->BitsPerPixel)
	{
		return FALSE;
	}

	if (f->BytesPerPixel != g->BytesPerPixel)
	{
	    return FALSE;
	}

	if (f->Amask != g->Amask)
	{
		// HACK: SDL ignores alpha channel during VideoInit procedure and doesn't store it.
		// If SDL_ListModes() is called without format parameter, we have mismatching formats,
		// that's why alpha check is ignored here
		dprintf("Alpha masks differ\n");
	}

	if (/*f->Amask != g->Amask ||*/
		f->Rmask != g->Rmask ||
		f->Gmask != g->Gmask ||
		f->Bmask != g->Bmask)
	{
		return FALSE;
	}

	if (f->Ashift != g->Ashift)
	{
		dprintf("Alpha shifts differ\n");
	}

	if (/*f->Ashift != g->Ashift ||*/
		f->Rshift != g->Rshift ||
		f->Gshift != g->Gshift ||
		f->Bshift != g->Bshift)
	{
	    return FALSE;
	}

	if (f->Aloss != g->Aloss)
	{
		dprintf("Alpha losses differ\n");
	}

	if (/*f->Aloss != g->Aloss ||*/
		f->Rloss != g->Rloss ||
		f->Gloss != g->Gloss ||
		f->Bloss != g->Bloss)
	{
	    return FALSE;
	}

	return TRUE;
}

static uint32
os4video_CountModes(const SDL_PixelFormat *format)
{
	uint32 mode = INVALID_ID;
	uint32 mode_count = 0;

	/* TODO: We need to handle systems with multiple monitors -
	 * this should probably count modes for a specific monitor
	 */
    while ((mode = SDL_IGraphics->NextDisplayInfo(mode)) != INVALID_ID)
	{
		SDL_PixelFormat pf;

		if (FALSE == os4video_PixelFormatFromModeID(&pf, mode))
			continue;

		if (FALSE == os4video_CmpPixelFormat(format, &pf))
			continue;

		mode_count++;
    }

    return mode_count;
}

uint32
os4video_PIXF2Bits(PIX_FMT rgbfmt)
{
	switch (rgbfmt)
	{
		case PIXF_CLUT:
			return 8;

		case PIXF_R8G8B8:
		case PIXF_B8G8R8:
			return 24;

		case PIXF_R5G6B5PC:
		case PIXF_R5G6B5:
		case PIXF_B5G6R5PC:
			return 16;

		case PIXF_B5G5R5PC:
		case PIXF_R5G5B5PC:
		case PIXF_R5G5B5:
			return 15;

		case PIXF_A8R8G8B8:
		case PIXF_A8B8G8R8:
		case PIXF_R8G8B8A8:
		case PIXF_B8G8R8A8:
			return 32;

		default:
			dprintf("Unknown pixelformat %d\n", rgbfmt);
			break;
	}

	return 0;
}

static void
logMode(uint32 mode, uint32 width, uint32 height, uint32 format, const char *message)
{
#ifdef DEBUG_FINDMODE
    dprintf("Mode:%08lx (%4d x %4d x %2d : format = %2d) - %s\n",
		mode, width, height, os4video_RTGFB2Bits(format), format, message);
#endif
}

/*
 * Search display database for a screenmode with desired pixel format
 * and a default size of least <width> x <height> pixels.
 *
 * Todo: better handle cases when there are multiple matching modes.
 * For now we simply pick the last mode in the display database which matches.
 */
static uint32
findModeWithPixelFormat(uint32 width, uint32 height, PIX_FMT pixelFormat, uint32 *outWidth, uint32 *outHeight)
{
	uint32 bestMode   = INVALID_ID;
	uint32 bestWidth  = UINT_MAX;
	uint32 bestHeight = UINT_MAX;

	uint32 mode = INVALID_ID;

	dprintf("Looking for mode %dx%d with pixel format %d\n", width, height, pixelFormat);

	while ((mode = SDL_IGraphics->NextDisplayInfo(mode)) != INVALID_ID)
	{
		PIX_FMT modeFormat;
		uint32 modeWidth;
		uint32 modeHeight;

		if (!os4video_IsRtgMode(mode))
		{
			//dprintf("Skipping non-RTG mode: %08lx\n", mode);
			continue;
		}

		modeWidth  = os4video_GetWidthFromMode(mode);
		modeHeight = os4video_GetHeightFromMode(mode);
		modeFormat = os4video_GetPixelFormatFromMode(mode);

		if (pixelFormat != modeFormat)
		{
			logMode (mode, modeWidth, modeHeight, modeFormat, "wrong pixel format!");
			continue;
		}

		if (width > modeWidth || height > modeHeight)
		{
			logMode (mode, modeWidth, modeHeight, modeFormat, "too small!");
			continue;
		}

		if (modeWidth > bestWidth || modeHeight > bestHeight)
		{
			logMode (mode, modeWidth, modeHeight, modeFormat, "too big!");
			continue;
		}

		logMode (mode, modeWidth, modeHeight, modeFormat, "okay");

		bestWidth  = modeWidth;
		bestHeight = modeHeight;
		bestMode   = mode;
	}

	if (bestMode != INVALID_ID)
	{
		dprintf("Found mode:%08lx\n", bestMode);

		*outWidth  = bestWidth;
		*outHeight = bestHeight;
	}

	return bestMode;
}

/*
 * Get a list of suitable pixel formats for this bpp sorted by preference.
 * For example, we prefer big-endian modes over little-endian modes
 */
const PIX_FMT *
os4video_GetPixelFormatsForBpp(uint32 bpp)
{
	static const PIX_FMT preferredFmts_8bit[]  = { PIXF_CLUT, PIXF_NONE };
	static const PIX_FMT preferredFmts_15bit[] = { PIXF_R5G5B5, PIXF_R5G5B5PC, PIXF_NONE };
	static const PIX_FMT preferredFmts_16bit[] = { PIXF_R5G6B5, PIXF_R5G6B5PC, PIXF_NONE };
	static const PIX_FMT preferredFmts_24bit[] = { PIXF_R8G8B8, PIXF_B8G8R8, PIXF_NONE };
	static const PIX_FMT preferredFmts_32bit[] = { PIXF_A8R8G8B8, PIXF_R8G8B8A8, PIXF_B8G8R8A8, PIXF_NONE };
	static const PIX_FMT unsupportedFmt[]      = { PIXF_NONE };

	switch (bpp)
	{
		case 8:  return preferredFmts_8bit;
		case 15: return preferredFmts_15bit;
		case 16: return preferredFmts_16bit;
		case 24: return preferredFmts_24bit;
		case 32: return preferredFmts_32bit;
		default: return unsupportedFmt;
	}
}

/*
 * Find a suitable AmigaOS screenmode to use for an SDL
 * screen of the specified width, height, bits-per-pixel
 * and SDL flags.
 *
 * Actually we ignore the flags for now, but it may come
 * in handy later.
 */
uint32
os4video_FindMode(uint32 width, uint32 height, uint32 bpp, uint32 flags)
{
	const PIX_FMT *format = os4video_GetPixelFormatsForBpp(bpp);

	uint32 foundWidth = 0;
	uint32 foundHeight = 0;
	uint32 foundMode = INVALID_ID;

	while (*format != PIXF_NONE)
	{
		/* And see if there is a mode of sufficient size for the format */
		foundMode = findModeWithPixelFormat(width, height, *format, &foundWidth, &foundHeight);

		if (foundMode != INVALID_ID)
		{
			dprintf("Found mode %08lx with height:%d width:%d\n", foundMode, foundWidth, foundHeight);

			return foundMode;
		}

		/* Otherwise try the next format */
		format++;
	}

	return INVALID_ID;
}

/*
 * Insert mode <rect> in <rectArray> queued by reverse order of size
 */
static void
queueMode(const SDL_Rect **rectArray, const SDL_Rect *rect, uint32 count)
{
	uint32 i = 0;

	while (i < count)
	{
		if ((rectArray[i]->w <= rect->w) && (rectArray[i]->h <= rect->h))
			break;
		i++;
	}

	if (i < count)
	{
		int j;

		for (j = count - 1; j >= (int)i; j--)
			rectArray[j + 1] = rectArray[j];
	}

	rectArray[i] = (SDL_Rect *) rect;
}

static void
fillModeArray(const SDL_PixelFormat *format, const SDL_Rect **rectArray, SDL_Rect *rects, uint32 maxCount)
{
	uint32 mode = INVALID_ID;
	uint32 width = 0;
	uint32 height = 0;
	uint32 count = 0;

	/*
	 * SDL wants the mode list in reverse order by mode size, so we queue
	 * the modes in rectArray accordingly since we cannot rely on the ordering
	 * of modes in graphic.lib's database.
	 *
	 * We should really pay attention to other criteria, not just mode size.
	 * In particular, we need a way to predictably handle systems with
	 * multiple montors. The current implementation can't do that.
	 */
	while (maxCount && (mode = SDL_IGraphics->NextDisplayInfo(mode)) != INVALID_ID)
	{
		SDL_PixelFormat pf;

		if (FALSE == os4video_PixelFormatFromModeID(&pf, mode))
			continue;
		if (FALSE == os4video_CmpPixelFormat(format, &pf))
			continue;

		width  = os4video_GetWidthFromMode(mode);
		height = os4video_GetHeightFromMode(mode);

		(*rects).x = 0;
		(*rects).y = 0;
		(*rects).w = (Uint16)width;
		(*rects).h = (Uint16)height;

		queueMode (rectArray, rects, count++);

		dprintf("Added mode %dx%d\n", width, height);
		maxCount--;
		rects++;
	}
}

SDL_Rect **
os4video_MakeResArray(const SDL_PixelFormat *f)
{
	uint32 allocSize;
	uint32 modeCnt;

	SDL_Rect *rectBase = NULL;
	SDL_Rect **rectPtr = NULL;

	modeCnt = os4video_CountModes(f);

	if (modeCnt) {
		allocSize = (modeCnt+1) * sizeof(SDL_Rect **) + modeCnt * sizeof(SDL_Rect);
		dprintf("%d video modes (allocating a %d-byte array)\n", modeCnt, allocSize);

		rectPtr = (SDL_Rect **)IExec->AllocVecTags(allocSize, AVT_Type, MEMF_SHARED, TAG_DONE);
		if (rectPtr)
		{
			rectBase = (SDL_Rect *)((uint8*)rectPtr + (modeCnt+1)*sizeof(SDL_Rect **));
			fillModeArray(f, (const SDL_Rect **)rectPtr, rectBase, modeCnt);
			rectPtr[modeCnt] = NULL;
		}
	}
	return rectPtr;
}

void *
SaveAllocPooled(struct SDL_PrivateVideoData *hidden, uint32 size)
{
	void *res;

	IExec->ObtainSemaphore(hidden->poolSemaphore);
	res = IExec->AllocPooled(hidden->pool, size);
	IExec->ReleaseSemaphore(hidden->poolSemaphore);

	return res;
}

void *
SaveAllocVecPooled(struct SDL_PrivateVideoData *hidden, uint32 size)
{
	void *res;

	IExec->ObtainSemaphore(hidden->poolSemaphore);
	res = IExec->AllocVecPooled(hidden->pool, size);
	IExec->ReleaseSemaphore(hidden->poolSemaphore);

	return res;
}

void
SaveFreePooled(struct SDL_PrivateVideoData *hidden, void *mem, uint32 size)
{
	IExec->ObtainSemaphore(hidden->poolSemaphore);
	IExec->FreePooled(hidden->pool, mem, size);
	IExec->ReleaseSemaphore(hidden->poolSemaphore);
}

void
SaveFreeVecPooled(struct SDL_PrivateVideoData *hidden, void *mem)
{
	IExec->ObtainSemaphore(hidden->poolSemaphore);
	IExec->FreeVecPooled(hidden->pool, mem);
	IExec->ReleaseSemaphore(hidden->poolSemaphore);
}
