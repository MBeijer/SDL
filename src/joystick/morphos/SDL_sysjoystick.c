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

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "SDL_sysjoystick_c.h"

#include <dos/dos.h>
#include <exec/execbase.h>
#include <libraries/sensors.h>
#include <libraries/sensors_hid.h>
#include <proto/exec.h>
#include <proto/sensors.h>
#include <proto/threadpool.h>
#include <proto/utility.h>

#define BUFFER_OFFSET(buffer, offset)   (((int32 *)buffer)[offset])

static const size_t sensortags[] = { SENSORS_Class, SensorClass_HID, SENSORS_Type, SensorType_HID_Gamepad, TAG_DONE };
static struct sensordata *sensors;

extern APTR threadpool;

static void sensor_events(struct sensordata *sensors, struct MsgPort *port)
{
	struct SensorsNotificationMessage *snm;

	sensors->sensorport = port;

	Signal(sensors->notifytask, SIGF_SINGLE);

	for (;;)
	{
		ULONG sigs = Wait(1 << port->mp_SigBit | SIGBREAKF_CTRL_C);

		if (sigs & SIGBREAKF_CTRL_C)
			break;

		snm = (struct SensorsNotificationMessage *)GetMsg(port);

		// a NULL snm->Sensor means it's a class list change notification!
		if (snm->Sensor == NULL)
		{
		}
		else
		{
			struct TagItem *tag, *tags = snm->Notifications;
			struct sensornotify *sn = snm->UserData;

			while ((tag = NextTagItem(&tags)))
			{
				DOUBLE *ptr = (DOUBLE *)tag->ti_Data;

				switch (tag->ti_Tag)
				{
					case SENSORS_HIDInput_Value:
						if (sn->type == SensorType_HIDInput_Trigger)
							sn->values[0] = *ptr;
						break;

					case SENSORS_HIDInput_Y_Index:
					case SENSORS_HIDInput_NS_Value:
						sn->values[1] = *ptr;
						break;

					case SENSORS_HIDInput_X_Index:
					case SENSORS_HIDInput_EW_Value:
						sn->values[0] = *ptr;
						break;

					case SENSORS_HIDInput_Z_Index:
						sn->values[2] = *ptr;
						break;
				}
			}
		}
	
		ReplyMsg(&snm->Msg);
	}

	while ((snm = (APTR)GetMsg(port)))
		ReplyMsg(&snm->Msg);
}

/* Function to scan the system for joysticks.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
	struct sensordata *s = SDL_malloc(sizeof(*s));
	int rc = -1;
	printf("[SDL_SYS_JoystickInit]\n");

	if (s)
	{
		s->notifytask = SysBase->ThisTask;
		printf("[SDL_SYS_JoystickInit] Create sensor events worker thread...\n");

		if (QueueWorkItem(threadpool, (THREADFUNC)sensor_events, s) == WORKITEM_INVALID)
		{
			SDL_free(s);
		}
		else
		{
			APTR sensorlist, sensor = NULL;
			ULONG count = 0;

			sensors = s;

			NEWLIST(&s->sensorlist);

			while (s->sensorport == NULL)
				Wait(SIGF_SINGLE);

			printf("[SDL_SYS_JoystickInit] Obtain sensor list...\n");
			sensorlist = ObtainSensorsList((struct TagItem *)&sensortags), sensor = NULL;

			while ((sensor = NextSensor(sensor, sensorlist, NULL)))
			{
				CONST_STRPTR name = "<unknown>", serial = NULL;
				size_t qt[] = { SENSORS_HIDInput_Name_Translated, SENSORS_HID_Serial, (size_t)&serial, TAG_DONE };
		
				if (GetSensorAttr(sensor, (struct TagItem *)&qt) > 0)
				{
					size_t namelen = strlen(name) + 1;
					struct joystick_hwdata *hwdata = SDL_malloc(sizeof(*hwdata) + namelen);

					if (hwdata)
					{
						count++;

						ADDTAIL(&s->sensorlist, hwdata);
						NEWLIST(&hwdata->notifylist);

						hwdata->attached = 1;

						// Using strncpy() is intentional
						strncpy((char *)&hwdata->guid, serial, sizeof(hwdata->guid));
						strcpy(hwdata->name, name);
					}
				}	
			}

			ReleaseSensorsList(sensorlist, NULL);

			printf("[SDL_SYS_JoystickInit] Found %ld joysticks...\n", count);

			s->joystick_count = count;
			rc = count;
		}
	}

	return rc;
}

int SDL_SYS_NumJoysticks()
{
	return sensors->joystick_count;
}

static struct joystick_hwdata *GetByIndex(int index)
{
	struct joystick_hwdata *hwdata = NULL, *ptr;


	ForeachNode(&sensors->sensorlist, ptr)
	{
		if (index == 0)
		{
			hwdata = ptr;
			break;
		}

		index--;
	};

	return hwdata;
}

void SDL_SYS_JoystickDetect()
{
}

SDL_bool SDL_SYS_JoystickNeedsPolling()
{
	return SDL_FALSE;
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
	struct joystick_hwdata *hwdata = GetByIndex(device_index);
	return hwdata->name;
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
	return device_index;
}

static void set_notify(struct sensordata *s, struct joystick_hwdata *hwdata, APTR sensor, ULONG type)
{
	struct sensornotify *sn;
	int count = 1;

	switch (type)
	{
		case SensorType_HIDInput_Stick:
		case SensorType_HIDInput_AnalogStick:
			count = 2;
			break;

		case SensorType_HIDInput_3DStick:
			count = 3;
			break;
	}

	sn = SDL_malloc(sizeof(*sn) + count * sizeof(DOUBLE));

	if (sn)
	{
		size_t notifytags[] = { SENSORS_Notification_Destination, (size_t)s->sensorport, SENSORS_Notification_SendInitialValue, TRUE, SENSORS_Notification_UserData, (size_t)sn, TAG_DONE };
		struct TagItem tags[4];
		int i;

		sn->type = type;

		for (i = 0; i < count;  i++)
			sn->values[i] = 0.0;

		tags[0].ti_Data = TRUE;
		tags[1].ti_Tag  = TAG_IGNORE;
		tags[1].ti_Data = TRUE;
		tags[2].ti_Tag  = TAG_IGNORE;
		tags[2].ti_Data = TRUE;
		tags[3].ti_Tag  = TAG_MORE;
		tags[3].ti_Data = (size_t)&notifytags;

		switch (type)
		{
			case SensorType_HIDInput_Trigger:
				tags[0].ti_Tag  = SENSORS_HIDInput_Value;
				break;

			case SensorType_HIDInput_Stick:
			case SensorType_HIDInput_AnalogStick:
				tags[0].ti_Tag  = SENSORS_HIDInput_NS_Value;
				tags[1].ti_Tag  = SENSORS_HIDInput_EW_Value;
				break;

			case SensorType_HIDInput_3DStick:
				tags[0].ti_Tag  = SENSORS_HIDInput_X_Index;
				tags[1].ti_Tag  = SENSORS_HIDInput_Y_Index;
				tags[2].ti_Tag  = SENSORS_HIDInput_Z_Index;
				break;
		}

		sn->notifyptr = StartSensorNotify(sensor, (struct TagItem *)&tags);

		ADDTAIL(&hwdata->notifylist, sn);
	}
}

static void GetJoyData(SDL_Joystick * joystick, struct joystick_hwdata *hwdata, APTR parent)
{
	struct sensordata *s = sensors;
	size_t sensortags[] = { SENSORS_Parent, (size_t)parent, SENSORS_Class, SensorClass_HID, TAG_DONE };
	size_t buttons = 0, naxes = 0;
	APTR sensorlist = ObtainSensorsList((struct TagItem *)&sensortags), sensor = NULL;

	while ((sensor = NextSensor(sensor, sensorlist, NULL)) != NULL)
	{
		SensorType_HIDInput type = SensorType_HIDInput_Unknown;
		size_t qt[] = { SENSORS_Type, (size_t)&type, TAG_DONE };
		
		if (GetSensorAttr(sensor, (struct TagItem *)&qt) > 0)
		{
			switch (type)
			{
				case SensorType_HIDInput_Trigger:
					buttons++;
					break;
				
				case SensorType_HIDInput_Stick:
				case SensorType_HIDInput_AnalogStick:
					naxes += 2;
					break;

				case SensorType_HIDInput_3DStick:
					naxes += 3;
					break;

				default:
					continue;
			}

			set_notify(s, hwdata, sensor, type);
		}	
	}

	ReleaseSensorsList(sensorlist, NULL);

	joystick->naxes = naxes;
	joystick->nbuttons = buttons;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
	struct joystick_hwdata *hwdata = GetByIndex(device_index);
	int rc = -1;

	if (hwdata)
	{
		APTR sensorlist = ObtainSensorsList((struct TagItem *)&sensortags), sensor = NULL;

		while ((sensor = NextSensor(sensor, sensorlist, NULL)))
		{
			CONST_STRPTR name = "<unknown>", serial = NULL;
			size_t qt[] = { SENSORS_HIDInput_Name_Translated, SENSORS_HID_Serial, (size_t)&serial, TAG_DONE };
		
			if (GetSensorAttr(sensor, (struct TagItem *)&qt) > 0 && strcmp(hwdata->name, name) == 0)
			{
				SDL_JoystickGUID guid;

				strncpy((char *)&guid, serial, sizeof(guid));

				if (memcmp(&hwdata->guid, &guid, sizeof(guid)) == 0)
				{
					GetJoyData(joystick, hwdata, sensor);
					rc = 0;
					break;
				}
			}
		}	

		ReleaseSensorsList(sensorlist, NULL);
	}

	return rc;
}

/* Function to determine is this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
	return SDL_TRUE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
	struct joystick_hwdata *hwdata = joystick->hwdata;
	struct sensornotify *sn;
	void                   *buffer;
	int naxe = 0, nbutton = 0;

	ForeachNode(&hwdata->notifylist, sn)
	{
		DOUBLE val;
		WORD sval;

		switch (sn->type)
		{
			case SensorType_HIDInput_Trigger:
				val = sn->values[0];
			    int buttondata = BUFFER_OFFSET(buffer, hwdata->buttonBufferOffset[nbutton]);
				if ((joystick->buttons[nbutton] && val == 0.0) || (joystick->buttons[nbutton] == 0 && val > 0.0)) {
					SDL_PrivateJoystickButton(joystick, nbutton, val == 0.0 ? 0 : 1);
					hwdata->buttonData[nbutton] = (val == 0.0 ? 0 : 1);
			   }

				nbutton++;
				break;

			case SensorType_HIDInput_Stick:
			case SensorType_HIDInput_AnalogStick:
			case SensorType_HIDInput_3DStick:
				sval = (WORD)(sn->values[0] * 32768.0);

				if (sval !=hwdata->axisData[naxe])	{
					SDL_PrivateJoystickAxis(joystick, naxe, sval);
					hwdata->axisData[naxe] = sval;
				}

				naxe++;

				sval = (WORD)(sn->values[1] * 32768.0);

				if (sval != hwdata->axisData[naxe]) {			
					SDL_PrivateJoystickAxis(joystick, naxe, sval);
					hwdata->axisData[naxe] = sval;
				}

				// TODO : sn->values[1] ?? for SANSORS_HIDInput_Z_Index
				
				naxe++;
				break;
		}
	}
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
	struct joystick_hwdata *hwdata = joystick->hwdata;

	if (hwdata)
	{
		struct sensornotify *sn, *tmp;

		ForeachNodeSafe(&hwdata->notifylist, sn, tmp)
		{
			if (sn->notifyptr)
				EndSensorNotify(sn->notifyptr, NULL);

			SDL_free(sn);
		}
	}
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
	struct joystick_hwdata *hwdata, *tmp;
	struct sensordata *s = sensors;

	ForeachNodeSafe(&s->sensorlist, hwdata, tmp)
	{
		SDL_free(hwdata);
	}

	Signal(s->sensorport->mp_SigTask, SIGBREAKF_CTRL_C);
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID( int device_index )
{
	struct joystick_hwdata *hwdata = GetByIndex(device_index);
	return hwdata->guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
	return joystick->hwdata->guid;
}
