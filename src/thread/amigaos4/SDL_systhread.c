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

//#define DEBUG
#include "../../main/amigaos4/SDL_os4debug.h"

/* Thread management routines for SDL */

#include "SDL_mutex.h"
#include "SDL_thread.h"
#include "SDL_stdinc.h"
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"
#include "../../main/amigaos4/SDL_os4timer_c.h"

#include <proto/exec.h>
#include <proto/dos.h>

#include <exec/tasks.h>

static struct PList currentThreads;
static struct PList currentJoins;

struct ThreadNode
{
	struct Node         Node;
	SDL_Thread         *thread;
	os4timer_Instance   timer;
	uint32              envVector;
};

static struct ThreadNode PrimaryThread;

struct JoinNode
{
	struct Node Node;
	struct Task *sigTask;
};

static void plistInit(struct PList *list)
{
	if (list)
	{
		list->sem = IExec->AllocSysObject(ASOT_SEMAPHORE, NULL);

		if (!list->sem)
		{
			dprintf("Failed to allocate semaphore\n");
		}

		IExec->NewList(&list->list);
	}
	else
	{
		dprintf("NULL pointer\n");
	}
}

static void plistTerm(struct PList *list)
{
	if (list && list->sem)
	{
		IExec->FreeSysObject(ASOT_SEMAPHORE, list->sem);
	}
	else
	{
		dprintf("NULL pointer\n");
	}
}

static void plistAdd(struct PList *list, struct Node *node)
{
	if (list && list->sem && node)
	{
		IExec->ObtainSemaphore(list->sem);
		IExec->AddHead(&list->list, node);
		IExec->ReleaseSemaphore(list->sem);
	}
	else
	{
		dprintf("NULL pointer\n");
	}
}

static void plistRemove(struct PList *list, struct Node *node)
{
	if (list && list->sem && node)
	{
		IExec->ObtainSemaphore(list->sem);
		IExec->Remove(node);
		IExec->ReleaseSemaphore(list->sem);
	}
	else
	{
		dprintf("NULL pointer\n");
	}
}

static struct Node *plistForEach(struct PList *list, plistForEachFn hook, struct Node *ref)
{
	struct Node *found = 0;

	if (list && list->sem)
	{
		struct Node *node;

		IExec->ObtainSemaphoreShared(list->sem);

		for (node = list->list.lh_Head;
			 node->ln_Succ;
			 node = node->ln_Succ)
		{
			if (hook(node, ref))
			{
				found = node;
				break;
			}
		}

		IExec->ReleaseSemaphore(list->sem);
	}
	else
	{
		dprintf("NULL pointer\n");
	}

	return found;
}

static BOOL plistIsListEmpty(struct PList *list)
{
	BOOL empty = FALSE;

	if (list && list->sem)
	{
		IExec->ObtainSemaphoreShared(list->sem);

		if (IsListEmpty(&list->list))
		{
			empty = TRUE;
		}

		IExec->ReleaseSemaphore(list->sem);
	}
	else
	{
		dprintf("NULL pointer\n");
	}

	return empty;
}

static inline __attribute__((always_inline)) uint32 get_r2(void)
{
	uint32 r2;
	__asm volatile ("mr %0, 2" : "=r" (r2));
	return r2;
}

static inline __attribute__((always_inline)) void set_r2(uint32 r2)
{
	__asm volatile ("mr 2, %0" :: "r" (r2));
}

void os4thread_initialize(void)
{
	struct Process *me = (struct Process *)IExec->FindTask(NULL);

	plistInit(&currentThreads);
	plistInit(&currentJoins);

	/* Initialize a node for the primary thread, but don't add
	 * it to thread list */
	PrimaryThread.thread    = NULL;
	me->pr_Task.tc_UserData = &PrimaryThread;

	os4timer_Init(&PrimaryThread.timer);

	dprintf("Primary process %p\n", me);
}

static int killThread(struct ThreadNode *node, struct ThreadNode *ref)
{
	SDL_SYS_KillThread(node->thread);

	return 0;
}

void os4thread_quit(void)
{
	dprintf("Killing all remaining processes\n");

	do
	{
		plistForEach(&currentThreads, (plistForEachFn)killThread, 0);
		//dprintf("Done, next try?\n");
	} while (!plistIsListEmpty(&currentThreads));

	dprintf("Terminating lists\n");

	plistTerm(&currentThreads);
	plistTerm(&currentJoins);

	os4timer_Destroy(&PrimaryThread.timer);

	dprintf("Done\n");
}

os4timer_Instance *os4thread_GetTimer(void)
{
	struct ThreadNode *node = (struct ThreadNode *)IExec->FindTask(NULL)->tc_UserData;

	return &node->timer;
}

static LONG RunThread(STRPTR args, LONG length, APTR sysbase)
{
	struct ExecIFace  *iexec;
	struct Process    *me;
	struct ThreadNode *myThread;

	/* When compiled baserel, we must set a private copy of pointer to Exec iface
	 * until r2 is set up. */
	iexec = (struct ExecIFace *) ((struct ExecBase *)sysbase)->MainInterface;

	/* Now find the thread node passed to us by our parent. */
	me = (struct Process *)iexec->FindTask(NULL);
	myThread = me->pr_Task.tc_UserData;

	/* We can now set up the pointer to the data segment and so
	 * access global data when compiled in baserel - including IExec! */
	set_r2(myThread->envVector);

	dprintf("Running process %p (SDL thread %p)\n", me, myThread->thread);

	/* Add ourself to the internal thread list. */
	plistAdd(&currentThreads, (struct Node *)myThread);

	os4timer_Init(&myThread->timer);

	/* Call the thready body. The args are passed to us via NP_EntryData. */
	SDL_RunThread((void *)IDOS->GetEntryData());

	return RETURN_OK;
}

static int signalJoins(struct Node *node, struct Node *dummy)
{
	IExec->Signal(((struct JoinNode *)node)->sigTask, SIGNAL_CHILD_TERM);
	return 0;
}

static void ExitThread(LONG retVal, LONG finalCode)
{
	struct Process    *me       = (struct Process *)IExec->FindTask(NULL);
	struct ThreadNode *myThread = me->pr_Task.tc_UserData;

	dprintf("Exiting process %p (SDL thread %p) with return value %d\n", me, myThread, retVal);

	/* Remove ourself from the internal list. */
	plistRemove(&currentThreads, (struct Node *)myThread);

	/* Signal all joiners that we're done. */
	plistForEach(&currentJoins, signalJoins, NULL);

	os4timer_Destroy(&myThread->timer);

	IExec->FreeVec(myThread);

	dprintf("Farewell from process %p\n", me);
}

int SDL_SYS_CreateThread(SDL_Thread *thread, void *args)
{
	struct Process *child;
	struct Process *me = (struct Process *)IExec->FindTask(NULL);
	struct ThreadNode *node;
	char buffer[128];

	BPTR inputStream, outputStream, errorStream;

	dprintf("Creating child thread %p with args %p\n", thread, args);

	/* Make a "meanignful" name */
	SDL_snprintf(buffer, sizeof(buffer), "SDL thread %p", thread);

	if (!(node = (struct ThreadNode *) IExec->AllocVecTags( sizeof( struct ThreadNode ),
		AVT_Type, MEMF_SHARED,
		AVT_ClearWithValue, 0,
		TAG_DONE ) ))
	{
		dprintf("Failed to allocate thread node\n");
		SDL_OutOfMemory();
		return -1;
	}

	node->thread = thread;

	/* When compiled baserel, the new thread needs a copy of the data
	 * segment pointer. It doesn't hurt to do this when not baserel. */
	node->envVector = get_r2();

	/* Try to clone parent streams */
	inputStream  = IDOS->DupFileHandle(IDOS->Input());
	outputStream = IDOS->DupFileHandle(IDOS->Output());
	errorStream  = IDOS->DupFileHandle(IDOS->ErrorOutput());

	/* Launch the child */
	child = IDOS->CreateNewProcTags(
					NP_Child,		TRUE,
					NP_Entry,		RunThread,
					NP_EntryData,	args,
					NP_FinalCode,	ExitThread,
					NP_Input,		inputStream,
					NP_CloseInput,	inputStream != 0,
					NP_Output,		outputStream,
					NP_CloseOutput,	outputStream != 0,
					NP_Error,		errorStream,
					NP_CloseError,	errorStream != 0,
					NP_Name, 		buffer,
					NP_Priority,	me->pr_Task.tc_Node.ln_Pri,
					NP_UserData,	(APTR)node,
					TAG_DONE);

	dprintf("Child process %p (%s)\n", child, buffer);

	if (!child)
	{
		SDL_SetError("Not enough resources to create thread");
		return -1;
	}

	return 0;
}

void SDL_SYS_SetupThread(void)
{
}

Uint32 SDL_ThreadID(void)
{
	return (Uint32)IExec->FindTask(NULL);
}

static int threadCmp(struct ThreadNode *node, struct ThreadNode *ref)
{
	return (node->thread == ref->thread) ? 1 : 0;
}

static BOOL KillThreadOrFail(SDL_Thread *thread)
{
	BOOL success = FALSE;
	struct Process *child;
	char buffer[128];

	/* Create the name to look for, and search the task */
	SDL_snprintf(buffer, sizeof(buffer), "SDL thread %p", thread);

	IExec->Forbid();

	child = (struct Process *)IExec->FindTask(buffer);

	if (child)
	{
		dprintf("Signalling process %p\n", child);
		IExec->Signal((struct Task *)child, SIGBREAKF_CTRL_C);
		success =TRUE;
	}
#ifdef DEBUG
	else
	{
		static uint32 counter = 0;
		if (counter++ < 10)
		{
			dprintf("Thread '%s' not found\n", buffer);
		}
	}
#endif

	IExec->Permit();

	return success;
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
	uint32 sigRec;
	struct JoinNode join;
	struct ThreadNode ref;

	/* Build reference and join nodes */
	ref.thread = thread;
	join.sigTask = IExec->FindTask(NULL);

	// Clear CTRL-C, otherwise we might not really wait for other threads
	sigRec = IExec->SetSignal(0L, SIGBREAKF_CTRL_C);

	dprintf("Waiting on thread %p to terminate (current signals %u)\n", thread, sigRec);

	/* Check if the thread is still active */
	while (plistForEach(&currentThreads, (plistForEachFn)threadCmp, (struct Node *)&ref))
	{
		/* Still there, join */
		dprintf("Thread %p joining...\n", thread);

		plistAdd(&currentJoins, (struct Node *)&join);

		sigRec = IExec->Wait(SIGNAL_CHILD_TERM | SIGBREAKF_CTRL_C);

		plistRemove(&currentJoins, (struct Node *)&join);

		if (sigRec & SIGBREAKF_CTRL_C)
		{
			dprintf("CTRL-C pressed, signalling waited thread %p\n", thread);

			if (!KillThreadOrFail(thread))
			{
				break;
			}

			continue;
		}

		dprintf("Some thread has exited\n");
	}

	dprintf("Thread %p exited\n", thread);
}

void SDL_SYS_KillThread(SDL_Thread *thread)
{
	KillThreadOrFail(thread);
}
