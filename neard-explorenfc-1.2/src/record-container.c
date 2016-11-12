/*
*         Copyright (c), NXP Semiconductors Gratkorn / Austria
*
*                     (C)NXP Semiconductors
*       All rights are reserved. Reproduction in whole or in part is
*      prohibited without the written consent of the copyright owner.
*  NXP reserves the right to make changes without notice at any time.
* NXP makes no warranty, expressed, implied or statutory, including but
* not limited to any implied warranty of merchantability or fitness for any
*particular purpose, or that the use will not infringe any third party patent,
* copyright or trademark. NXP must not be liable for any loss or damage
*                          arising from its use.
*/


#include "record-container.h"
#include "dbus-daemon.h"
#include "dbus-parameters.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include "hal.h"
#include "ndef.h"
#include "record.h"

//Local functions
static void record_container_class_init (RecordContainerClass* pRecordContainerClass);
static void record_container_init(RecordContainer* pRecordContainer);
static void record_container_dispose(GObject* pGObject);

//GObject implementation
G_DEFINE_TYPE (RecordContainer, record_container, G_TYPE_OBJECT)

void record_container_class_init (RecordContainerClass* pRecordContainerClass)
{
	GObjectClass* pGObjectClass = G_OBJECT_CLASS (pRecordContainerClass);

	pGObjectClass->dispose = record_container_dispose;

	//Do not overload finalize
	//pGObjectClass->finalize = tag_finalize;
}

void record_container_init(RecordContainer* pRecordContainer)
{
	pRecordContainer->objectPath = NULL;
	pRecordContainer->pAdapter = NULL;
}

void record_container_dispose(GObject* pGObject)
{
	G_OBJECT_CLASS (record_container_parent_class)->dispose(pGObject);
}
