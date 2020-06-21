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

/* Allow access to a raw mixing buffer (for AmigaOS) */
#include "SDL_endian.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiomem.h"
#include "../SDL_sysaudio.h"
#include "SDL_ahiaudio.h"

/* Audio driver init fuctions */
static int AHI_AudioAvailable(void);
static SDL_AudioDevice *AHI_CreateDevice(int devindex);
static void AHI_DeleteDevice(SDL_AudioDevice *device);

/* Audio driver export functions */
/* pre thread functions */
static int AHI_OpenAudio(SDL_AudioDevice *self, SDL_AudioSpec *spec);
static void AHI_CloseAudio(SDL_AudioDevice *self);

/* thread functions */
static void AHI_ThreadInit(SDL_AudioDevice *self);
static void AHI_WaitDone(SDL_AudioDevice *self);
static void AHI_WaitAudio(SDL_AudioDevice *self);
static void AHI_PlayAudio(SDL_AudioDevice *self);
static Uint8 *AHI_GetAudioBuf(SDL_AudioDevice *self);

/* The tag name used by the AmigaOS audio driver */
#define DRIVER_NAME         "amigaos"

AudioBootStrap AHI_bootstrap = {
   DRIVER_NAME,"AmigaOS3 AHI audio",
   AHI_AudioAvailable, AHI_CreateDevice
};

/* ------------------------------------------ */
/* Audio driver init functions implementation */
/* ------------------------------------------ */

static int AHI_OpenAhiDevice(OS3AudioData *os3data)
{
    int ahi_open = 0;

    /* create our reply port */
    os3data->ahi_ReplyPort = (struct MsgPort*)CreateMsgPort();
    if (os3data->ahi_ReplyPort)
    {
        /* create a iorequest for the device */
        os3data->ahi_IORequest[0] = (struct AHIRequest*)CreateIORequest(os3data->ahi_ReplyPort, sizeof( struct AHIRequest ));
        if (os3data->ahi_IORequest[0])
        {
            /* open the device */
            os3data->ahi_IORequest[0]->ahir_Version = 4;
            if (!OpenDevice(AHINAME, 0, (struct IORequest *)os3data->ahi_IORequest[0], 0))
            {
                /* Create a copy */
                os3data->ahi_IORequest[1] = (struct AHIRequest *)CreateIORequest(os3data->ahi_ReplyPort, sizeof( struct AHIRequest ));
                if (os3data->ahi_IORequest[1])
                {
                    CopyMem(os3data->ahi_IORequest[0], os3data->ahi_IORequest[1], sizeof(struct AHIRequest));

                    D(bug("AHI available.\n"));
                    ahi_open = 1;
                    os3data->currentBuffer = 0;
                    os3data->link = 0;
                }
            }
        }
    }

    return ahi_open;
}

static void AHI_CloseAhiDevice(OS3AudioData *os3data)
{

    if (os3data->ahi_IORequest[0])
    {
        if (os3data->link)
        {
            AbortIO((struct IORequest *)os3data->link);
            WaitIO((struct IORequest *)os3data->link);
        }

		D(bug("Closing device\n"));
        CloseDevice((struct IORequest *)os3data->ahi_IORequest[0]);

		D(bug("Deleting I/O request\n"));
		DeleteIORequest( os3data->ahi_IORequest[0] );
        os3data->ahi_IORequest[0] = NULL;
		DeleteIORequest( os3data->ahi_IORequest[1] );
        os3data->ahi_IORequest[1] = NULL;
    }

    if (os3data->ahi_ReplyPort)
    {
		D(bug("Deleting message port\n"));
        DeleteMsgPort( os3data->ahi_ReplyPort );
        os3data->ahi_ReplyPort = NULL;
    }

	D(bug("done closing\n"));
}

static int  AHI_AudioAvailable(void)
{
   OS3AudioData data;
   int is_available = AHI_OpenAhiDevice(&data);
   if (is_available)
   {
       AHI_CloseAhiDevice(&data);
   }
   D(bug("AHI is %savailable\n", is_available ? "" : "not "));
   return is_available;
}

static void AHI_DeleteDevice(SDL_AudioDevice *device)
{
    if (device)
    {
        if (device->hidden)
        {
            SDL_free(device->hidden);
        }
        SDL_free(device);
    }
}

static SDL_AudioDevice *AHI_CreateDevice(int devindex)
{
    SDL_AudioDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
    if ( device )
    {
        SDL_memset(device, 0, sizeof(SDL_AudioDevice));
        device->hidden = (OS3AudioData *)SDL_malloc(sizeof(OS3AudioData));
        if (device->hidden)
        {
            SDL_memset(device->hidden, 0, sizeof(OS3AudioData));

            /* no AHI device access, global process context */
            device->OpenAudio   = AHI_OpenAudio;
            device->CloseAudio  = AHI_CloseAudio;

            /* AHI access allowed, audio thread context */
            device->ThreadInit  = AHI_ThreadInit; 	/* thread enter (open device) */
            device->WaitAudio   = AHI_WaitAudio;
            device->PlayAudio   = AHI_PlayAudio;
            device->GetAudioBuf = AHI_GetAudioBuf;
            device->WaitDone    = AHI_WaitDone; 	/* thread exit (close device) */

            device->free        = AHI_DeleteDevice;
        }
        else
        {
            /* error on allocating private data */
            SDL_OutOfMemory();
           /* clean up on error */
           SDL_free(device);
           device = NULL;
           D(bug("Can't allocate private data\n"));
        }
    }
    else
    {
        /* error on allocating device structure */
        SDL_OutOfMemory();
        D(bug("Can't allocate device\n"));
    }

    return device;
}

/* ---------------------------------------------- */
/* Audio driver exported functions implementation */
/* ---------------------------------------------- */

static int AHI_OpenAudio(SDL_AudioDevice *self, SDL_AudioSpec *spec)
{
    int result = 0;
    OS3AudioData * os3data = self->hidden;

    /* Determine the audio parameters from the AudioSpec */
    switch ( spec->format & 0xFF ) {
        case 8: {
            /* Signed 8 bit audio data */
            D(bug("Samples a 8 bit...\n"));
            spec->format = AUDIO_S8;
            if ( spec->channels < 2 )
				os3data->ahi_Type = AHIST_M8S;
            else
				os3data->ahi_Type = AHIST_S8S;
        }
            break;

        case 16: { /* Signed 16 bit audio data */
            D(bug("Samples a 16 bit...\n"));
            spec->format = AUDIO_S16MSB;
            if ( spec->channels < 2 )
				os3data->ahi_Type = AHIST_M16S;
            else
				os3data->ahi_Type = AHIST_S16S;
        }
            break;

        default: {
            SDL_SetError("Unsupported audio format");
            return (-1);
        }
    }

    if ( spec->channels != 1 && spec->channels != 2 ) {
        D(bug("Wrong channel number!\n"));
        SDL_SetError("Channel number non supported");
        return -1;
    }

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(spec);

    /* Allocate mixing buffer */
    os3data->audio_MixBufferSize = spec->size;
    os3data->audio_MixBuffer[0] = (Uint8 *)SDL_AllocAudioMem(spec->size);
    os3data->audio_MixBuffer[1] = (Uint8 *)SDL_AllocAudioMem(spec->size);
    if ( os3data->audio_MixBuffer[0] == NULL || os3data->audio_MixBuffer[1] == NULL )
    {
        AHI_CloseAudio(self);
        D(bug("No memory for audio buffer\n"));
        return -1;
    }
    SDL_memset(os3data->audio_MixBuffer[0], spec->silence, spec->size);
    SDL_memset(os3data->audio_MixBuffer[1], spec->silence, spec->size);

    os3data->audio_IsOpen = 1;

    return result;
}

static void AHI_CloseAudio(SDL_AudioDevice *self)
{
    OS3AudioData * os3data = self->hidden;

    if (os3data->audio_MixBuffer[0])
    {
        SDL_FreeAudioMem(os3data->audio_MixBuffer[0]);
        os3data->audio_MixBuffer[0] = NULL;
    }

     if (os3data->audio_MixBuffer[1])
    {
        SDL_FreeAudioMem(os3data->audio_MixBuffer[1]);
        os3data->audio_MixBuffer[1] = NULL;
    }
    os3data->audio_IsOpen = 0;
}

static void AHI_ThreadInit(SDL_AudioDevice *self)
{
    OS3AudioData      *os3data = self->hidden;
    SDL_AudioSpec     *spec    = &self->spec;

    if (os3data->audio_IsOpen)
    {
        // Open ahi.device
        AHI_OpenAhiDevice(os3data);
        switch( spec->format )
        {
        	case AUDIO_S8:
			case AUDIO_U8:
				os3data->ahi_Type = (spec->channels<2) ? AHIST_M8S : AHIST_S8S;
				break;

			default:
				os3data->ahi_Type = (spec->channels<2) ? AHIST_M16S : AHIST_S16S;
				break;
		}

        SetTaskPri(FindTask(0), 15);
    }
}

static void AHI_WaitDone(SDL_AudioDevice *self)
{
    AHI_CloseAhiDevice(self->hidden);
}

static void AHI_WaitAudio(SDL_AudioDevice *self)
{
  /* Dummy - AHI_PlayAudio handles the waiting */
}

static void AHI_PlayAudio(SDL_AudioDevice *self)
{
    struct AHIRequest  *ahi_IORequest;
    SDL_AudioSpec      *spec    = &self->spec;
    OS3AudioData       *os3data = self->hidden;

    ahi_IORequest = os3data->ahi_IORequest[os3data->currentBuffer];

    ahi_IORequest->ahir_Std.io_Message.mn_Node.ln_Pri = 60;
    ahi_IORequest->ahir_Std.io_Data    = os3data->audio_MixBuffer[os3data->currentBuffer];
    ahi_IORequest->ahir_Std.io_Length  = os3data->audio_MixBufferSize;
    ahi_IORequest->ahir_Std.io_Offset  = 0;
    ahi_IORequest->ahir_Std.io_Command = CMD_WRITE;
    ahi_IORequest->ahir_Volume         = 0x10000;
    ahi_IORequest->ahir_Position       = 0x8000;
    ahi_IORequest->ahir_Link           = os3data->link;
    ahi_IORequest->ahir_Frequency      = spec->freq;
    ahi_IORequest->ahir_Type           = os3data->ahi_Type;

	// Convert to signed?
	if( spec->format == AUDIO_U8 )
	{
#if POSSIBLY_DANGEROUS_OPTIMISATION
		int i, n;
		uint32 *mixbuf = (uint32 *)os3data->audio_MixBuffer[os3data->currentBuffer];
		n = os3data->audio_MixBufferSize/4; // let the gcc optimiser decide the best way to divide by 4
		for( i=0; i<n; i++ )
			*(mixbuf++) ^= 0x80808080;
		if (0 != (n = os3data->audio_MixBufferSize & 3))
		{
			uint8  *mixbuf8 = (uint8*)mixbuf;
			for( i=0; i<n; i++ )
				*(mixbuf8++) -= 128;
		}
#else
		int i;
		for( i=0; i<os3data->audio_MixBufferSize; i++ )
			os3data->audio_MixBuffer[os3data->currentBuffer][i] -= 128;
#endif
	}

    SendIO((struct IORequest*)ahi_IORequest);

    if (os3data->link)
        WaitIO((struct IORequest*)os3data->link);

    os3data->link = ahi_IORequest;

    os3data->currentBuffer = 1-os3data->currentBuffer;
}

static Uint8 *AHI_GetAudioBuf(SDL_AudioDevice *self)
{
    return self->hidden->audio_MixBuffer[self->hidden->currentBuffer];
}
