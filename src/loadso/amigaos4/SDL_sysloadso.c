/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

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

#ifdef SDL_LOADSO_AMIGAOS4

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent library loading routines                           */

#include <proto/elf.h>
#include <proto/dos.h>

static struct Library *elfBase;
static struct Library *dosBase;

static struct ElfIFace *iElf;
static struct DOSIFace *iDOS;

#include "SDL_loadso.h"

//#define DEBUG
#include "../../main/amigaos4/SDL_os4debug.h"

typedef struct {
    Elf32_Handle elf_handle;
    APTR shared_object;
} OS4_ObjectHandle;

static void
OS4_CloseLibs()
{
    struct ExecIFace* iExec = ((struct ExecIFace *)((*(struct ExecBase **)4)->MainInterface));

    iExec->DropInterface((void *)iElf);
    iExec->DropInterface((void *)iDOS);

    iElf = NULL;
    iDOS = NULL;

    iExec->CloseLibrary(elfBase);
    iExec->CloseLibrary(dosBase);

    elfBase = NULL;
    dosBase = NULL;
}

static BOOL
OS4_OpenLibs()
{
    BOOL result = FALSE;

    struct ExecIFace* iExec = ((struct ExecIFace *)((*(struct ExecBase **)4)->MainInterface));

    if (!elfBase) {
        elfBase = iExec->OpenLibrary("elf.library", 52);

        if (elfBase) {
            iElf = (struct ElfIFace *) iExec->GetInterface(elfBase, "main", 1, NULL);
        }
    }

    if (!dosBase) {
        dosBase = iExec->OpenLibrary("dos.library", 51);

        if (dosBase) {
            iDOS = (struct DOSIFace *) iExec->GetInterface(dosBase, "main", 1, NULL);
        }
    }

    if (iElf && iDOS) {
        result = TRUE;
    } else {
        OS4_CloseLibs();
    }

    return result;
}

void *
SDL_LoadObject(const char *sofile)
{
    if (OS4_OpenLibs()) {
        OS4_ObjectHandle *handle = SDL_malloc(sizeof(OS4_ObjectHandle));

        if (handle) {
            BPTR seglist = iDOS->GetProcSegList(NULL, GPSLF_RUN);

            if (seglist) {
                Elf32_Handle eh = NULL;
                iDOS->GetSegListInfoTags(seglist, GSLI_ElfHandle, &eh, TAG_DONE);

                dprintf("Elf handle %p\n", eh);

                if (eh) {
                    APTR so = iElf->DLOpen(eh, sofile, 0);

                    if (so) {
                        dprintf("'%s' loaded\n", sofile);

                        handle->elf_handle = eh;
                        handle->shared_object = so;

                        return handle;
                    } else {
                        dprintf("DLOpen failed for '%s'\n", sofile);
                        SDL_SetError("DLOpen failed for '%s'", sofile);
                    }
                } else {
                    dprintf("Failed to get elf handle of running task\n");
                    SDL_SetError("Failed to get elf handle");
                }
            } else {
                dprintf("Failed to get seglist\n");
                SDL_SetError("Failed to get seglist");
            }

            SDL_free(handle);
        }
    }

    return NULL;
}

void *
SDL_LoadFunction(void *handle, const char *name)
{
    void *symbol = NULL;

    if (OS4_OpenLibs() && handle) {
        APTR address;
        OS4_ObjectHandle *oh = handle;

        Elf32_Error result = iElf->DLSym(oh->elf_handle, oh->shared_object, name, &address);

        if (result == ELF32_NO_ERROR) {
            symbol = address;
            dprintf("Symbol '%s' found at %p\n", name, address);
        } else {
            dprintf("Symbol '%s' not found\n", name);
            SDL_SetError("Symbol '%s' not found", name);
        }
    }

    return symbol;
}

void
SDL_UnloadObject(void *handle)
{
    if (OS4_OpenLibs() && handle) {
        OS4_ObjectHandle *oh = handle;

        Elf32_Error result = iElf->DLClose(oh->elf_handle, oh->shared_object);

        dprintf("DLClose %s\n", (result == ELF32_NO_ERROR) ? "OK" : "failed" );

/* BUG: testloadso crashes on Final Edition...removed this block for now, until a solution is found.
        IElf->CloseElfTags(
            oh->elf_handle,
            TAG_DONE);
*/
        SDL_free(handle);
    }

    OS4_CloseLibs();
}

#endif /* SDL_LOADSO_AMIGAOS4 */

