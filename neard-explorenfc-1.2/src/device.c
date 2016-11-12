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

#include "device.h"
#include "dbus-daemon.h"
#include "dbus-parameters.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include "hal.h"
#include "ndef.h"
#include "record.h"
#include "record-container.h"
#include "dbus-daemon.h"


//Local functions
static void device_class_init (DeviceClass* pDeviceClass);
static void device_init(Device* pDevice);
static void device_dispose(GObject* pGObject);

//GObject implementation
G_DEFINE_TYPE (Device, device, TYPE_RECORD_CONTAINER)

void device_class_init (DeviceClass* pDeviceClass)
{
	pDeviceClass->device_register = device_register;
	pDeviceClass->device_unregister = device_unregister;

	RecordContainerClass* pRecordContainerClass = RECORD_CONTAINER_CLASS (pDeviceClass);

	pRecordContainerClass->dispose =device_dispose;

	//Do not overload finalize
	//pRecordContainerClass->finalize = device_finalize;
}

void device_init(Device* pDevice)
{
	RECORD_CONTAINER(pDevice)->objectPath = NULL;
	RECORD_CONTAINER(pDevice)->pAdapter = NULL;
	pDevice->pNeardDevice = NULL;
	pDevice->pObjectSkeleton = NULL;
	pDevice->deviceId = 0;
	pDevice->pRecordTable = g_hash_table_new(g_direct_hash, g_direct_equal);
	pDevice->pRawNDEF = NULL;
}

void device_dispose(GObject* pGObject)
{
	Device* pDevice = DEVICE(pGObject);

	if( RECORD_CONTAINER(pDevice)->objectPath != NULL )
	{
		device_unregister(pDevice);
	}

	G_OBJECT_CLASS (device_parent_class)->dispose(pGObject);
}

//DBUS commands handlers
static gboolean on_push (NeardDevice *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							GVariant *arg_attributes, gpointer pUserData);
static gboolean on_get_raw_ndef (NeardDevice *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							gpointer pUserData);

Device* device_new()
{
	Device* pDevice = g_object_new(TYPE_DEVICE, NULL);
	return pDevice;
}

void device_register(Device* pDevice, Adapter* pAdapter, guint deviceId)
{
	RECORD_CONTAINER(pDevice)->pAdapter = pAdapter;
	g_object_ref(pAdapter);

	pDevice->deviceId = deviceId;

	RECORD_CONTAINER(pDevice)->objectPath = g_strdup_printf("%s"DBUS_DEVICE_OBJECT_PATH, pAdapter->objectPath, deviceId);
	g_info("Device at %s\n", RECORD_CONTAINER(pDevice)->objectPath);

	pDevice->pObjectSkeleton = neard_object_skeleton_new(RECORD_CONTAINER(pDevice)->objectPath);
	pDevice->pNeardDevice = neard_device_skeleton_new();
	neard_object_skeleton_set_device(pDevice->pObjectSkeleton, pDevice->pNeardDevice);

	//Connect signals
	g_signal_connect(pDevice->pNeardDevice, "handle-push",
					G_CALLBACK (on_push), pDevice);
	g_signal_connect(pDevice->pNeardDevice, "handle-get-raw-ndef",
						G_CALLBACK (on_get_raw_ndef), pDevice);

    neard_device_set_name(pDevice->pNeardDevice, RECORD_CONTAINER(pDevice)->objectPath);
	neard_device_set_adapter(pDevice->pNeardDevice, pAdapter->objectPath);

	//Export
	g_dbus_object_manager_server_export( RECORD_CONTAINER(pDevice)->pAdapter->pDaemon->pObjectManagerServer, G_DBUS_OBJECT_SKELETON(pDevice->pObjectSkeleton) );
}

void device_populate_records(Device* pDevice)
{
	//Get NDEF
	guint8* buffer = NULL;
	gsize bufferLength = 0;
	hal_device_get_ndef(RECORD_CONTAINER(pDevice)->pAdapter->pDaemon->pHal, pDevice->deviceId, &buffer, &bufferLength);

	if(buffer != NULL)
	{
		GList* pList = ndef_message_parse(buffer, bufferLength);
		pDevice->pRawNDEF = g_bytes_new_take(buffer, bufferLength); //buffer is now owned by pRawNDEF
		gsize length = g_list_length(pList);

		const gchar* objectPaths[length + 1];
		guint recordId = 0;

		GVariantBuilder variantBld;
		g_variant_builder_init(&variantBld, G_VARIANT_TYPE("ao"));


		for (; pList != NULL; pList = g_list_next(pList))
		{
			NdefRecord* pNdefRecord = (NdefRecord*) pList->data;

			//Check various agents that might have been registered
			dbus_daemon_check_ndef_record(RECORD_CONTAINER(pDevice)->pAdapter->pDaemon, pNdefRecord);

			Record* pRecord = record_new();
			record_register(pRecord, RECORD_CONTAINER(pDevice), pNdefRecord, recordId);

			g_object_unref(G_OBJECT(pNdefRecord));

			g_hash_table_insert(pDevice->pRecordTable, GUINT_TO_POINTER(pRecord->recordId), pRecord);

			//Add object path to list
			objectPaths[recordId] = g_strdup(pRecord->objectPath);
			g_variant_builder_add(&variantBld, "o", pRecord->objectPath);

			recordId++;
		}

		g_list_free(pList);

		objectPaths[length] = NULL;
		//neard_device_emit_property_changed(pDevice->pNeardDevice, "Records", g_variant_new_variant(g_variant_new("(ao)", &variantBld)));

		neard_device_set_records(pDevice->pNeardDevice, objectPaths);
	}
}

void device_unregister(Device* pDevice)
{
	g_dbus_object_manager_server_unexport( RECORD_CONTAINER(pDevice)->pAdapter->pDaemon->pObjectManagerServer, RECORD_CONTAINER(pDevice)->objectPath );

	GHashTableIter iter;
	guint* pRecordId;
	Record* pRecord;

	//Remove & free records
	g_hash_table_iter_init (&iter, pDevice->pRecordTable);
	while (g_hash_table_iter_next(&iter, (gpointer*)&pRecordId, (gpointer*)&pRecord))
	{
		record_unregister(pRecord);

		//Remove from table
		g_hash_table_iter_remove(&iter);

		//Free record
		g_object_unref(pRecord);
	}


	g_object_unref(pDevice->pNeardDevice);
	g_object_unref(pDevice->pObjectSkeleton);

	//g_dbus_connection_unregister_object(pAdapter->pDaemon->pConnection, pAdapter->registrationId);

	if( pDevice->pRawNDEF != NULL )
	{
		g_bytes_unref(pDevice->pRawNDEF);
		pDevice->pRawNDEF = NULL;
	}

	g_free(RECORD_CONTAINER(pDevice)->objectPath);
	RECORD_CONTAINER(pDevice)->objectPath = NULL;

	//Clear ref to Adapter
	g_clear_object(&RECORD_CONTAINER(pDevice)->pAdapter);
}

//Local functions
gboolean on_push (NeardDevice *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							GVariant *arg_attributes, gpointer pUserData)
{
	Device* pDevice = (Device*) pUserData;

	//Build NdefRecord instance based on array of dictionaries
	NdefRecord* pNdefRecord = ndef_record_from_dictionary(arg_attributes);

	//Create a 1-long list
	GList* pList = g_list_append(NULL, pNdefRecord);

	guint8* buffer = NULL;
	gsize bufferLength = 0;
	ndef_message_generate(pList, &buffer, &bufferLength);
	hal_device_push_ndef(RECORD_CONTAINER(pDevice)->pAdapter->pDaemon->pHal, pDevice->deviceId, buffer, bufferLength);

	g_list_free(pList);

	if( buffer != NULL )
	{
		g_free(buffer);
	}

	neard_device_complete_push(pInterfaceSkeleton, pInvocation);
	return TRUE;
}

gboolean on_get_raw_ndef (NeardDevice *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							gpointer pUserData)
{
	Device* pDevice = (Device*) pUserData;

	GBytes* pRawNDEF = pDevice->pRawNDEF;

	if(pRawNDEF == NULL)
	{
		pRawNDEF = g_bytes_new(NULL, 0);
	}
	else
	{
		g_bytes_ref(pRawNDEF);
	}

	GVariant* pNdefVariant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, pRawNDEF, FALSE);

	neard_device_complete_get_raw_ndef(pInterfaceSkeleton, pInvocation, pNdefVariant);

	g_object_unref(pNdefVariant);
	g_bytes_unref(pRawNDEF);

	return TRUE;
}
