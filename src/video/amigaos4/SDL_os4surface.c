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

#include "../SDL_sysvideo.h"
#include "../SDL_blit.h"
#include "SDL_os4video.h"
#include "SDL_os4utils.h"
#include "SDL_os4blit.h"

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>

#include <intuition/intuition.h>

//#define DEBUG
#include "../../main/amigaos4/SDL_os4debug.h"

extern struct GraphicsIFace  *SDL_IGraphics;
extern struct LayersIFace    *SDL_ILayers;
extern struct IntuitionIFace *SDL_IIntuition;

#ifdef DEBUG
static char *
get_flags_str(Uint32 flags)
{
	static char buffer[256];

	buffer[0] = '\0';

	if (flags & SDL_HWSURFACE)		SDL_strlcat(buffer, "HWSURFACE ", sizeof(buffer));
	if (flags & SDL_SRCCOLORKEY)	SDL_strlcat(buffer, "SRCCOLORKEY ", sizeof(buffer));
	if (flags & SDL_RLEACCELOK)		SDL_strlcat(buffer, "RLEACCELOK ", sizeof(buffer));
	if (flags & SDL_RLEACCEL)		SDL_strlcat(buffer, "RLEACCEL ", sizeof(buffer));
	if (flags & SDL_SRCALPHA)		SDL_strlcat(buffer, "SRCALPHA ", sizeof(buffer));
	if (flags & SDL_PREALLOC)		SDL_strlcat(buffer, "PREALLOC ", sizeof(buffer));

	return buffer;
}
#endif

int
os4video_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	int result = -1;

	dprintf("Allocating HW surface %p flags:%s\n", surface, get_flags_str(surface->flags));

	/* Create surface hardware record if not already present */
	if (!surface->hwdata)
		surface->hwdata = SaveAllocPooled(_this->hidden, sizeof(struct private_hwdata));

	if (surface->hwdata) {
		struct BitMap *friend_bitmap;

		surface->hwdata->type = hwdata_bitmap;
		surface->hwdata->lock = 0;
		surface->hwdata->colorkey_bm = NULL;

		/* Determine friend bitmap */
		if (_this->hidden->scr == NULL)
		{
			/* Windowed mode - use the off-screen buffer's bitmap */
			friend_bitmap = _this->hidden->offScreenBuffer.bitmap;
		}
		else
		{
			/* Full-screen mode. Use the display's bitmap */
			friend_bitmap = _this->hidden->win->RPort->BitMap;
		}

		dprintf("Trying to create %dx%dx%d bitmap (friend %p)\n",
				surface->w, surface->h, surface->format->BitsPerPixel, friend_bitmap);

		surface->hwdata->bm = SDL_IGraphics->AllocBitMapTags(
			surface->w,
			surface->h,
			surface->format->BitsPerPixel,
			BMATags_Displayable, TRUE,
			BMATags_Friend, friend_bitmap,
			TAG_DONE);

		if (surface->hwdata->bm)
		{
			dprintf("Bitmap %p created. Width %d, height %d, depth %d, bytes %d, bits %d\n",
				surface->hwdata->bm,
				SDL_IGraphics->GetBitMapAttr(surface->hwdata->bm, BMA_WIDTH),
				SDL_IGraphics->GetBitMapAttr(surface->hwdata->bm, BMA_HEIGHT),
				SDL_IGraphics->GetBitMapAttr(surface->hwdata->bm, BMA_DEPTH),
				SDL_IGraphics->GetBitMapAttr(surface->hwdata->bm, BMA_BYTESPERPIXEL),
				SDL_IGraphics->GetBitMapAttr(surface->hwdata->bm, BMA_BITSPERPIXEL));

			surface->flags |= SDL_HWSURFACE | SDL_PREALLOC | SDL_HWACCEL;
			result = 0;
		}
		else
		{
			dprintf("Failed to create bitmap\n");
			SDL_SetError("Failed to create bitmap");

			SaveFreePooled(_this->hidden, surface->hwdata, sizeof(struct private_hwdata));
			surface->hwdata = NULL;
		}
	}

	return result;
}

void
os4video_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	dprintf("Freeing HW surface %p\n", surface);

	if (surface)
	{
		/* Check if this is surface allocated by AllocHWSurface */
		if (surface->hwdata && surface->hwdata->type == hwdata_bitmap)
		{
			dprintf("Freeing bitmap %p\n", surface->hwdata->bm);

			SDL_IGraphics->FreeBitMap(surface->hwdata->bm);

			if (surface->hwdata->colorkey_bm)
			{
			    SDL_IGraphics->FreeBitMap(surface->hwdata->colorkey_bm);
			}

			/*  Free surface hardware record */
			SaveFreePooled(_this->hidden, surface->hwdata, sizeof(struct private_hwdata));
			surface->hwdata = NULL;
		}
	}

	return;
}

int
os4video_LockHWSurface(_THIS, SDL_Surface *surface)
{
	int success = -1;
	struct private_hwdata *hwdata = surface->hwdata;

	surface->pixels = (uint8*)0xcccccccc;

	if (hwdata->bm)
	{
		APTR base_address;
		uint32 bytes_per_row;

		/* We're locking a surface in video memory (either a full-screen hardware display
		 * surface or a hardware off-screen surface). We need to get P96 to lock that the
		 * corresponding bitmap in memory so that we can access the pixels directly
		 */
		hwdata->lock = SDL_IGraphics->LockBitMapTags(
			hwdata->bm,
			LBM_BaseAddress, &base_address,
			LBM_BytesPerRow, &bytes_per_row,
			TAG_DONE);

		if (hwdata->lock)
		{
			/* Bitmap was successfully locked */
			surface->pitch  = bytes_per_row;
			surface->pixels = base_address;

			if (surface->hwdata->type == hwdata_display_hw)
			{
				/* In full-screen mode, set up offset to pixels (SDL offsets the surface
				 * when the surface size doesn't match a real resolution, but the value it
				 * sets is wrong, because it doesn't know the surface pitch).
				 */
				if  (surface->flags & SDL_FULLSCREEN)
					surface->offset = _this->offset_y * surface->pitch
									+ _this->offset_x * surface->format->BytesPerPixel;
			}

			success = 0;
		}
		else
		{
			dprintf("Failed to lock bitmap: %p\n", surface->hwdata->bm);
			SDL_SetError("Failed to lock bitmap");
		}
	}

	return success;
}

void
os4video_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	struct private_hwdata *hwdata = surface->hwdata;

	if (hwdata->bm)
		SDL_IGraphics->UnlockBitMap(surface->hwdata->lock);

	surface->hwdata->lock = 0;
	surface->pixels = (uint8*)0xcccccccc;
}

int
os4video_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *rect, Uint32 color)
{
    //dprintf("x=%d y=%d w=%d h=%d color=%08x\n", rect->x, rect->y, rect->w, rect->h, color);

	int xmin, ymin;
	int xmax, ymax;

	struct RastPort *rp = NULL;

	static struct RastPort tmpras;
	static int tmpras_initialized = 0;

	if (dst->hwdata && dst->hwdata->bm)
	{
		xmin = rect->x; ymin = rect->y;
		xmax = xmin + rect->w - 1;
		ymax = ymin + rect->h - 1;

		if (dst->hwdata->type == hwdata_display_hw)
		{
			if (!(dst->flags & SDL_DOUBLEBUF) && (dst->flags & SDL_FULLSCREEN))
			{
				rp = _this->hidden->win->RPort;
			}
		}

		/* We need to set up a rastport for rendering to off-screen surfaces */
		if (!rp)
		{
			if (!tmpras_initialized) {
				SDL_IGraphics->InitRastPort(&tmpras);
				tmpras_initialized = 1;
			}

			tmpras.BitMap = dst->hwdata->bm;
			rp = &tmpras;
		}

		if (dst->format->BitsPerPixel > 8)
		{
			/* Convert hi/true-colour pixel format to ARGB32 format for P96 fill.
			 * Bloody stupid if you ask me, 'cos P96 will have to convert it
			 * back again */
			struct SDL_PixelFormat *format = dst->format;
			uint32 argb_colour;

			argb_colour = (((color & format->Amask) >> format->Ashift) << (format->Aloss + 24))
						| (((color & format->Rmask) >> format->Rshift) << (format->Rloss + 16))
						| (((color & format->Gmask) >> format->Gshift) << (format->Gloss + 8))
						| (((color & format->Bmask) >> format->Bshift) <<  format->Bloss);

			SDL_IGraphics->RectFillColor(rp, xmin, ymin, xmax, ymax, argb_colour);
		}
		else
		{
			/* Fall back on graphics lib for palette-mapped surfaces */
			SDL_IGraphics->SetAPen(rp, color);
			SDL_IGraphics->RectFill(rp, xmin, ymin, xmax, ymax);
		}

		return 0;
	}

	dprintf("HW data missing\n");
	SDL_SetError("HW data missing");

	return -1;
}

static BOOL
os4video_composite(struct BitMap *src_bm, struct BitMap *dst_bm,
	float surface_alpha, struct BitMap *colorkey_bm,
	uint32 src_x, uint32 src_y,
	uint32 width, uint32 height,
	uint32 dst_x, uint32 dst_y,
	uint32 flags)
{
	uint32 ret_code = SDL_IGraphics->CompositeTags(
		COMPOSITE_Src_Over_Dest,
		src_bm,
		dst_bm,
		COMPTAG_SrcAlpha, COMP_FLOAT_TO_FIX(surface_alpha),
		COMPTAG_SrcAlphaMask, colorkey_bm,
		COMPTAG_SrcX,		src_x,
		COMPTAG_SrcY,		src_y,
		COMPTAG_SrcWidth,   width,
		COMPTAG_SrcHeight,  height,
		COMPTAG_OffsetX,    dst_x,
		COMPTAG_OffsetY,    dst_y,
		COMPTAG_Flags,      COMPFLAG_IgnoreDestAlpha | COMPFLAG_HardwareOnly | flags,
		TAG_END);

	if (ret_code)
	{
#ifdef DEBUG
		static uint32 counter = 0;

		if ((counter++ % 100) == 0)
		{
			dprintf("CompositeTags failed (%u): %d\n", counter, ret_code);
		}
#endif
		SDL_SetError("CompositeTags failed");

		return FALSE;
	}

	return TRUE;
}

static BOOL
os4video_hasAlphaBlending(SDL_Surface *src)
{
	return ((src->flags & SDL_SRCALPHA) == SDL_SRCALPHA);
}

static BOOL
os4video_hasAlphaChannel(SDL_Surface *src)
{
	return (src->format->Amask == 0xFF000000 || src->format->Amask == 0x000000FF);
}

static BOOL
os4video_hasColorKey(SDL_Surface *src)
{
	return ((src->flags & SDL_SRCCOLORKEY) == SDL_SRCCOLORKEY);
}

static float
os4video_getSurfaceAlpha(SDL_Surface *src)
{
	if (os4video_hasAlphaChannel(src))
	{
		//dprintf("Surface has alpha channel, per-surface alpha %d ignored\n",
		//	  src->format->alpha);

		return 1.0f;
	}

	//dprintf("Using per-surface alpha %d\n", src->format->alpha);

	return src->format->alpha / 255.0f;
}

static uint32
os4video_getOverrideFlag(SDL_Surface *src)
{
	if (os4video_hasAlphaChannel(src) || os4video_hasColorKey(src))
	{
		return 0;
	}

	return COMPFLAG_SrcAlphaOverride;
}

static int
os4video_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect,
	SDL_Surface *dst, SDL_Rect *dstrect)
{
	struct BitMap *src_bm = src->hwdata->bm;

	if (src_bm)
	{
		if (os4video_hasAlphaBlending(src))
		{
			BOOL ret = os4video_composite(src_bm, dst->hwdata->bm,
				os4video_getSurfaceAlpha(src),
				src->hwdata->colorkey_bm,
				srcrect->x, srcrect->y,
				srcrect->w, srcrect->h,
				dstrect->x, dstrect->y,
				os4video_getOverrideFlag(src));

			if (!ret)
			{
				return -1;
			}
		}
		else
		{
			if (os4video_hasColorKey(src))
			{
				BOOL ret = os4video_composite(src_bm, dst->hwdata->bm,
					1.0f,
					src->hwdata->colorkey_bm,
					srcrect->x, srcrect->y,
					srcrect->w, srcrect->h,
					dstrect->x, dstrect->y,
					0);

				if (!ret)
				{
					return -1;
				}
			}
			else
			{
				int32 error = SDL_IGraphics->BltBitMapTags(
					BLITA_Source, src_bm,
					BLITA_Dest, dst->hwdata->bm,
					BLITA_SrcX, srcrect->x,
					BLITA_SrcY, srcrect->y,
					BLITA_DestX, dstrect->x,
					BLITA_DestY, dstrect->y,
					BLITA_Width, srcrect->w,
					BLITA_Height, srcrect->h,
					TAG_DONE);

				if (error != -1)
				{
					dprintf("BltBitMapTags() returned %d\n", error);
					SDL_SetError("BltBitMapTags failed");
					return -1;
				}
			}
		}
	}
	else
	{
		dprintf("NULL bitmap\n");
	}

	return 0;
}

int
os4video_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst)
{
	dprintf("src flags:%s dst flags:%s\n", get_flags_str(src->flags), get_flags_str(dst->flags));

	int accelerated = 0;

	if (src->hwdata && dst->hwdata)
	{
		if (src->format->BitsPerPixel > 8)
		{
			/* With compositing feature we can accelerate alpha blending and color key too */
			accelerated = 1;
		}
		else
		{
			if (!(src->flags & (SDL_SRCALPHA | SDL_SRCCOLORKEY)))
			{
				accelerated = 1;
			}
		}
	}

	if (accelerated)
	{
		src->flags |= SDL_HWACCEL;
		src->map->hw_blit = os4video_HWAccelBlit;

		dprintf("Hardware blitting supported\n");
	}
	else
	{
		src->flags &= ~SDL_HWACCEL;

		dprintf("Hardware blitting not supported\n");
	}

	return accelerated;
}

int
os4video_SetHWAlpha(_THIS, SDL_Surface *src, Uint8 value)
{
	if (src->hwdata && (src->format->BitsPerPixel > 8))
	{
		return 0;
	}

	dprintf("Failed to set HW alpha\n");
	SDL_SetError("Failed to set HW alpha");

	return -1;
}

static void
os4video_CreateAlphaMask(struct BitMap *src_bm, struct BitMap *mask_bm, Uint32 key)
{
	APTR src_base_address;
	uint32 src_bytes_per_row;

	APTR src_lock = SDL_IGraphics->LockBitMapTags(
		src_bm,
		LBM_BaseAddress, &src_base_address,
		LBM_BytesPerRow, &src_bytes_per_row,
		TAG_DONE);

	if (src_lock)
	{
		APTR mask_base_address;
		uint32 mask_bytes_per_row;

		APTR mask_lock = SDL_IGraphics->LockBitMapTags(
			mask_bm,
			LBM_BaseAddress, &mask_base_address,
			LBM_BytesPerRow, &mask_bytes_per_row,
			TAG_DONE);

		if (mask_lock)
		{
			Uint32 bytes_per_pixel = SDL_IGraphics->GetBitMapAttr(src_bm, BMA_BYTESPERPIXEL);

			int y;

			for (y = 0; y < mask_bm->Rows; y++)
			{
				int x;
				uint8 *mask_ptr = (uint8 *)mask_base_address + y * mask_bytes_per_row;

				switch(bytes_per_pixel)
				{
					case 2:
					{
						uint16 key16 = key;

						uint16 *src_ptr = (uint16 *)((uint8 *)src_base_address + y * src_bytes_per_row);

						for (x = 0; x < mask_bytes_per_row; x++)
						{
							mask_ptr[x] = (src_ptr[x] == key16) ? 0 : 0xFF;
						}
					} break;

					case 4:
					{
						uint32 *src_ptr = (uint32 *)((uint8 *)src_base_address + y * src_bytes_per_row);

						for (x = 0; x < mask_bytes_per_row; x++)
						{
							mask_ptr[x] = (src_ptr[x] == key) ? 0 : 0xFF;
						}
					} break;

					// TODO: Should we support 24-bit modes?
					default:
						dprintf("Unknown pixel format!\n");
						break;
				}
			}

			SDL_IGraphics->UnlockBitMap(mask_lock);
		}

		SDL_IGraphics->UnlockBitMap(src_lock);
	}
}

int
os4video_SetHWColorKey(_THIS, SDL_Surface *src, Uint32 key)
{
	if (src->hwdata && (src->format->BitsPerPixel > 8))
	{
		if (src->hwdata->colorkey_bm == NULL)
		{
			dprintf("Creating new alpha mask\n");

			src->hwdata->colorkey_bm = SDL_IGraphics->AllocBitMapTags(
				src->w,
				src->h,
				8,
				BMATags_PixelFormat, PIXF_ALPHA8,
				BMATags_Displayable, TRUE,
				TAG_DONE);
		}

		if (src->hwdata->colorkey_bm)
		{
			os4video_CreateAlphaMask(
				src->hwdata->bm,
				src->hwdata->colorkey_bm,
				key);

			return 0;
		}
	}

	dprintf("Failed to set HW color key\n");
	SDL_SetError("Failed to set HW color key");
	return -1;
}

int
os4video_FlipHWSurface(_THIS, SDL_Surface *surface)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;
	struct private_hwdata       *hwdata = &hidden->screenHWData;

	if (hwdata->type == hwdata_display_hw)
	{
		struct DoubleBufferData *dbData = &hidden->dbData;

		//dprintf("Flipping double-buffered surface\n");

		if (!dbData->SafeToFlip)
		{
			/* If the screen buffer has been flipped previously, then wait
			 * for a message from gfx.lib telling us that the new buffer has
			 * been made visible.
			 */
			IExec->Wait(1 << (dbData->SafeToFlip_MsgPort->mp_SigBit));
			while (IExec->GetMsg(dbData->SafeToFlip_MsgPort) != NULL)
				;
			dbData->SafeToFlip = TRUE;
		}
		/*
		 * Now try to flip the screen buffers
		 */
		if (SDL_IIntuition->ChangeScreenBuffer(hidden->scr, dbData->sb[dbData->currentSB]))
		{
			/* The flip was succesful. Update our pointer to the off-screen buffer */
			dbData->currentSB   = 1 - dbData->currentSB;
			hwdata->bm = dbData->sb[dbData->currentSB]->sb_BitMap;

			/* A successful flip means that we need to wait for a signal before writing
			 * to the new off-screen buffer and to wait for a signal before
			 * flipping again.
			 */
			dbData->SafeToWrite = FALSE;
			dbData->SafeToFlip  = FALSE;
		}
		else
		{
			dprintf("ChangeScreenBuffer() failed\n");
		}

		if (!dbData->SafeToWrite)
		{
			/* If the screen has just been flipped, wait until gfx.lib signals us
			 * that it is safe to write to the new off-screen buffer
			 */
			IExec->Wait(1 << (dbData->SafeToWrite_MsgPort->mp_SigBit));
			while (IExec->GetMsg(dbData->SafeToWrite_MsgPort) != NULL)
				;
			dbData->SafeToWrite = TRUE;
		}
	}

	return 0;
}

void
os4video_UpdateRectsFullscreenDB(_THIS, int numrects, SDL_Rect *rects)
{
	// NOP
}

void
os4video_UpdateRectsNone(_THIS, int numrects, SDL_Rect *rects)
{
}

static void
os4video_OffscreenHook_8bit (struct Hook *hook, struct RastPort *rp, int *message)
{
	SDL_VideoDevice *_this = (SDL_VideoDevice *)hook->h_Data;
	struct SDL_PrivateVideoData *hidden = _this->hidden;

	struct Rectangle *rect = (struct Rectangle *)(message + 1);
	uint32 offsetX         = message[3];
	uint32 offsetY         = message[4];

	/* Source bitmap (off-screen) is private, so does not need locking */
	UBYTE *srcMem   = hidden->offScreenBuffer.pixels;
	ULONG  srcPitch = hidden->offScreenBuffer.pitch;

	/* Attempt to lock destination bitmap (screen) */
	APTR dst_base_address;
	uint32 dst_bytes_per_row;

	APTR dst_lock = SDL_IGraphics->LockBitMapTags(
		rp->BitMap,
		LBM_BaseAddress, &dst_base_address,
		LBM_BytesPerRow, &dst_bytes_per_row,
		TAG_DONE);

	if (dst_lock)
	{
		uint32 dst;
		uint32 src;
		uint32 BytesPerPixel;
		int32  line = rect->MinY;

		offsetX -= hidden->win->BorderLeft;
		offsetY -= hidden->win->BorderTop;

		BytesPerPixel = SDL_IGraphics->GetBitMapAttr(
			rp->BitMap,
			BMA_BYTESPERPIXEL);

		dst = (uint32)dst_base_address + dst_bytes_per_row * rect->MinY + BytesPerPixel * rect->MinX;
		src = (uint32)srcMem  + srcPitch * offsetY + offsetX;

		switch (BytesPerPixel)
		{
			case 2:
				while (line <= rect->MaxY)
				{
					cpy_8_16((void *)dst, (void *)src, rect->MaxX - rect->MinX + 1, hidden->offScreenBuffer.palette);
					dst += dst_bytes_per_row;
					src += srcPitch;
					line ++;
				}
				break;
			case 3:
				while (line <= rect->MaxY)
				{
					cpy_8_24((void *)dst, (void *)src, rect->MaxX - rect->MinX + 1, hidden->offScreenBuffer.palette);
					dst += dst_bytes_per_row;
					src += srcPitch;
					line ++;
				}
				break;
			case 4:
				while (line <= rect->MaxY)
				{
					cpy_8_32((void *)dst, (void *)src, rect->MaxX - rect->MinX + 1, hidden->offScreenBuffer.palette);
					dst += dst_bytes_per_row;
					src += srcPitch;
					line ++;
				}
				break;
		}

		SDL_IGraphics->UnlockBitMap(dst_lock);
	}
	else
	{
		dprintf("Bitmap lock failed\n");
	}
}

#ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
#endif

/*
 * Flush rectanges from off-screen buffer to the screen
 * Special case for palettized off-screen buffers
 */
void
os4video_UpdateRectsOffscreen_8bit(_THIS, int numrects, SDL_Rect *rects)
{
#ifdef PROFILE_UPDATE_RECTS
	uint32 start;
	uint32 end;
#endif

	struct SDL_PrivateVideoData *hidden = _this->hidden;
	struct Window 				*w      = hidden->win;

	/* We don't want Intuition dead-locking us while we try to do this... */
	SDL_ILayers->LockLayerInfo(&w->WScreen->LayerInfo);

	{
		/* Current bounds of inner window */
		const struct Rectangle windowRect = {
			w->BorderLeft,
			w->BorderTop,
			w->Width  - w->BorderRight  - 1,
			w->Height - w->BorderBottom - 1
		};

		struct Rectangle clipRect;
		struct Hook      clipHook;

		const SDL_Rect *r = &rects[0];

		for ( ; numrects > 0; r++, numrects--)
		{
			clipHook.h_Entry = (uint32(*)())os4video_OffscreenHook_8bit;
			clipHook.h_Data  = _this;

			clipRect.MinX    = windowRect.MinX + r->x;
			clipRect.MinY    = windowRect.MinY + r->y;
			clipRect.MaxX    = MIN(windowRect.MaxX, clipRect.MinX + r->w - 1);
			clipRect.MaxY    = MIN(windowRect.MaxY, clipRect.MinY + r->h - 1);

#ifdef PROFILE_UPDATE_RECTS
			start = SDL_GetTicks();
#endif

			SDL_ILayers->DoHookClipRects(&clipHook, w->RPort, &clipRect);

#ifdef PROFILE_UPDATE_RECTS
			end = SDL_GetTicks();
			dprintf("DoHookClipRects took %d microseconds\n", end-start);
#endif
		}
	}

	SDL_ILayers->UnlockLayerInfo(&w->WScreen->LayerInfo);
}

/*
 * Flush rectanges from off-screen buffer to the screen
 */
void
os4video_UpdateRectsOffscreen(_THIS, int numrects, SDL_Rect *rects)
{
	struct SDL_PrivateVideoData *hidden = _this->hidden;
	struct Window               *w      = hidden->win;

	/* We don't want our window changing size while we're doing this */
	SDL_ILayers->LockLayer(0, w->WLayer);

	{
		/* Current dimensions of inner window */
		const struct IBox windowBox = {
			w->BorderLeft, w->BorderTop, w->Width, w->Height
		};

		const SDL_Rect *r = &rects[0];

		for ( ; numrects > 0; r++, numrects--)
		{
			/* Blit rect to screen, constraing rect to bounds of inner window */
			SDL_IGraphics->BltBitMapRastPort(hidden->offScreenBuffer.bitmap,
								 r->x,
								 r->y,
								 w->RPort,
								 r->x + windowBox.Left,
								 r->y + windowBox.Top,
								 MIN(r->w, windowBox.Width),
								 MIN(r->h, windowBox.Height),
								 0xC0);
		}
	}

	SDL_ILayers->UnlockLayer(w->WLayer);
}
