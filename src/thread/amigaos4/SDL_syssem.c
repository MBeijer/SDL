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
    POSIX-style semaphores for AmigaOS4.

    Richard Drummond
    evilrich@rcdrummond.net
 */

#include "SDL_thread.h"

#include <proto/exec.h>
#include <exec/semaphores.h>
#include <exec/lists.h>
#include <exec/nodes.h>

#include "SDL_systhread_c.h"

#include "../../main/amigaos4/SDL_os4timer_c.h"

//#define DEBUG
#include "../../main/amigaos4/SDL_os4debug.h"

extern os4timer_Instance *os4thread_GetTimer(void);
extern Uint32 SDL_GetTicks(void);

struct SDL_semaphore
{
	BOOL                    live;
	Uint32                  count;
	struct SignalSemaphore  mutex;
	struct List             waiters;
	APTR                    magic;
};

static void validate(SDL_sem *sem)
{
	if (sem->magic != sem)
	{
		dprintf("Semaphore %p: bad magic %p\n", sem, sem->magic);
	}
}

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
	SDL_sem *sem = (SDL_sem *) SDL_malloc(sizeof(*sem));

	if (sem)
	{
		sem->live = TRUE;
		sem->count = initial_value;
		sem->magic = sem;

		IExec->InitSemaphore(&sem->mutex);
		IExec->NewList(&sem->waiters);

		dprintf("Semaphore %p created\n", sem);
	}
	else
	{
		dprintf("Allocation failed\n");
		SDL_SetError("Allocation failed");
	}

	return sem;
}

void SDL_DestroySemaphore(SDL_sem *sem)
{
	if (sem)
	{
		IExec->ObtainSemaphore(&sem->mutex);

		validate(sem);

		BOOL shouldFree = sem->live;

		if (sem->live)
		{
			dprintf("Destroying semaphore %p\n", sem);

			sem->live = FALSE;
			sem->count = 0;
			sem->magic = NULL;

			if (!IsListEmpty(&sem->waiters))
			{
				dprintf("Trying to wake up blocked threads\n");

				/* There are threads blocked on this semaphore.
				 *
				 * According to the Single Unix Specification:
				 * "It is safe to destroy an initialised semaphore upon which no threads
				 *  are currently blocked. The effect of destroying a semaphore upon which
				 *  other threads are currently blocked is undefined."
				 *
				 * However, this is AmigaOS, and we have to be able to clear up
				 * as best we can - since the system doesn't do it for us.
				 */
				struct Node *node;

				while ((node = IExec->RemHead((struct List *)&sem->waiters)))
				{
					/* Ensure node is delinked, so that nobody will try and remove us again. */
					node->ln_Pred = node->ln_Succ = 0;
					dprintf("Sending CTRL-C to process %p\n", node->ln_Name);
					/* Try to interrupt blocking task and hope for the best. */
					IExec->Signal((struct Task *)node->ln_Name, SIGBREAKF_CTRL_C);

					IExec->ReleaseSemaphore(&sem->mutex);

					/* Releasing the mutex above should cause a reschedule and the blocked
					 * thread to run. We shouldn't be able to regain the mutex until that
					 * thread has exitted its block. */

					IExec->ObtainSemaphore(&sem->mutex);
				}
			}

			dprintf("Done\n");
		}

		IExec->ReleaseSemaphore(&sem->mutex);

		if (shouldFree)
		{
			SDL_free(sem);
		}
	}
	else
	{
		dprintf("NULL semaphore\n");
	}
}

int SDL_SemPost(SDL_sem *sem)
{
	int result = -1;

	if (sem)
	{
		IExec->ObtainSemaphore(&sem->mutex);

		validate(sem);

		if (sem->live)
		{
			result = 0;

			dprintf("Process %p incrementing semaphore %p (count %u)\n",
				IExec->FindTask(NULL), sem, sem->count);

			sem->count++;

			if (sem->count == 1)
			{
				/* Wake up a blocked thread if there is one. */
				struct Node *node = IExec->RemHead(&sem->waiters);

				if (node)
				{
					/* Ensure node is delinked, so that nobody will try and remove us again. */
					node->ln_Pred = node->ln_Succ = 0;

					/* Send wake-up signal. */
					struct Task *task = (struct Task *)node->ln_Name;
					IExec->Signal(task, SIGNAL_SEMAPHORE);
				}
			}
		}
		else
		{
			dprintf("Semaphore destroyed\n");
			SDL_SetError("Semaphore destroyed");
		}

		IExec->ReleaseSemaphore(&sem->mutex);
	}
	else
	{
		dprintf("NULL semaphore\n");
		SDL_SetError("Passed a NULL semaphore");
	}

	return result;
}

/*
 * Wait for semaphore count to go positive or for a time-out to occur.
 *
 * This is called with the semaphore's mutex locked.
 */
static int blockOnSem(SDL_sem *sem, uint32 timeOut)
{
	int result = -1;

	ULONG signalsReceived = 0;
	ULONG alarmSignal = 0;
	ULONG notInterruptedOrTimedOut = 0;

	struct Task *task = IExec->FindTask(NULL);
	struct Node node;
	node.ln_Name = (STRPTR) task;

	if (timeOut)
	{
		os4timer_SetAlarm(os4thread_GetTimer(), timeOut, &alarmSignal);
	}

	/* Ensure we don't react to any spurious signals. */
	signalsReceived = IExec->SetSignal(0, SIGNAL_SEMAPHORE);

	dprintf("Process %p blocked on semaphore %p (signals 0x%X)\n", task, sem, signalsReceived);

	do
	{
		/* Add this task's node to the semaphore's waiting list. */
		IExec->AddTail(&sem->waiters, &node);

		/* Release the mutex, so another thread can do a SemPost(). */
		IExec->ReleaseSemaphore(&sem->mutex);

		dprintf("Process %p starts to wait for signals\n", task);

		/* Wait for the unblock signal, a time-out or to be interrupted. */
		signalsReceived = IExec->Wait(SIGNAL_SEMAPHORE | SIGBREAKF_CTRL_C | alarmSignal);

		dprintf("Process %p wait over\n", task);

		IExec->ObtainSemaphore(&sem->mutex);

		dprintf("Process %p obtained semaphore\n", task);

		validate(sem);

		/* There's a potential race condition here. Another task
		 * could jump in and decrement the semaphore after we receive an
		 * unblock signal in the Wait() above and before we gain exclusive
		 * access again by locking the mutex.
		 *
		 * We defuse this time-bomb by testing the semaphore's count and
		 * and waiting for another unblock signal if it's not now positive
		 * and we haven't been interrupted or timed-out.
		 */

		notInterruptedOrTimedOut = !(signalsReceived & (SIGBREAKF_CTRL_C | alarmSignal));

#ifdef DEBUG
		if (sem->count == 0 && notInterruptedOrTimedOut)
		{
			dprintf("Process %p usurped on semaphore %p. Blocking again\n", task, sem);
		}
#endif
	} while (sem->count == 0 && notInterruptedOrTimedOut);

	if (timeOut)
	{
		os4timer_ClearAlarm(os4thread_GetTimer());
	}

#ifdef DEBUG
	if (sem->count == 0 && notInterruptedOrTimedOut)
	{
		dprintf("Process %p Error: inconsistent semaphore state!\n", task);
	}
#endif

	/* Ensure that our node really is removed from the list. If we've been interrupted
	 * or a time-out has occurred, then we probably haven't been removed by a call to
	 ( SDL_SemPost(). */
	if (node.ln_Succ)
	{
		IExec->Remove(&node);
	}

	if (signalsReceived & SIGBREAKF_CTRL_C)
	{
		dprintf("Process %p wait interrupted by CTRL-C\n", task);
	}
	else if (signalsReceived & alarmSignal)
	{
		dprintf("Process %p wait timed out\n", task);

		result = SDL_MUTEX_TIMEDOUT;
	}
	else
	{
		/* Successfully unblocked. Decrement count. */
		dprintf("Process %p decrementing semaphore %p (count %u)\n", task, sem, sem->count);

		--sem->count;
		result = 0;
	}

	return result;
}

int SDL_SemWait(SDL_sem *sem)
{
	int retval = -1;

	if (sem)
	{
		IExec->ObtainSemaphore(&sem->mutex);

		validate(sem);

		if (sem->live)
		{
			if (sem->count > 0)
			{
				/* The semaphore count is positive so it can be decremented
				 * without blocking. */
				dprintf("Process %p decrementing semaphore %p (count %u)\n",
					IExec->FindTask(NULL), sem, sem->count);

				--sem->count;
				retval = 0;
			}
			else
			{
				/* Block and wait for count to go positive. */
				retval = blockOnSem(sem, 0);
			}
		}
		else
		{
			dprintf("Semaphore destroyed\n");
			SDL_SetError("Semaphore destroyed");
		}

		IExec->ReleaseSemaphore(&sem->mutex);
	}
	else
	{
		dprintf("NULL semaphore\n");
		SDL_SetError("Passed a NULL semaphore");
	}

	return retval;
}

int SDL_SemTryWait(SDL_sem *sem)
{
	int retval = -1;

	if (sem)
	{
		IExec->ObtainSemaphore(&sem->mutex);

		validate(sem);

		if (sem->live)
		{
			if (sem->count > 0)
			{
				/* The semaphore count is positive so it can be decrememted
				 * without blocking. */
				dprintf("Process %p decrementing semaphore %p\n", IExec->FindTask(NULL), sem);

				--sem->count;
				retval = 0;
			}
			else
			{
				/* Count is not positive, so cannot be decremented without blocking. */
				retval = SDL_MUTEX_TIMEDOUT;
			}
		}
		else
		{
			dprintf("Semaphore destroyed\n");
			SDL_SetError("Semaphore destroyed");
		}

		IExec->ReleaseSemaphore(&sem->mutex);
	}
	else
	{
		dprintf("NULL semaphore\n");
		SDL_SetError("Passed a NULL semaphore");
	}

	return retval;
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
	int retval = -1;

	if (sem)
	{
		IExec->ObtainSemaphore(&sem->mutex);

		validate(sem);

		if (sem->live)
		{
			if (sem->count > 0)
			{
				/* The semaphore count is positive, so it can be decremented
				 * without blocking. */
				dprintf("Process %p decrementing semaphore %p\n", IExec->FindTask(NULL), sem);

				--sem->count;
				retval = 0;
			}
			else
			{
				/* Block and wait for count to go positive or for time-out. */
				retval = blockOnSem(sem, SDL_GetTicks() + timeout);
			}
		}
		else
		{
			dprintf("Semaphore destroyed\n");
			SDL_SetError("Semaphore destroyed");
		}

		IExec->ReleaseSemaphore(&sem->mutex);
	}
	else
	{
		SDL_SetError("Passed a NULL semaphore");
	}

	return retval;
}

Uint32 SDL_SemValue(SDL_sem *sem)
{
	Uint32 value = 0;

	if (sem)
	{
		IExec->ObtainSemaphore(&sem->mutex);

		validate(sem);

		if (sem->live)
		{
			value = sem->count;
		}
		else
		{
			dprintf("Semaphore destroyed\n");
			SDL_SetError("Semaphore destroyed");
		}

		IExec->ReleaseSemaphore(&sem->mutex);
	}
	else
	{
		dprintf("NULL semaphore\n");
		SDL_SetError("Passed a NULL semaphore");
	}

	return value;
}
