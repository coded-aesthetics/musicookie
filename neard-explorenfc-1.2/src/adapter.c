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

#include "adapter.h"
#include "dbus-daemon.h"
#include "dbus-parameters.h"
#include "record-container.h"
#include "tag.h"
#include "device.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#define DEFAULT_POLLING_MODE nfc_mode_initiator

#include "hal.h"

//Local functions
static void adapter_class_init (AdapterClass* pAdapterClass);
static void adapter_init(Adapter* pAdapter);
static void adapter_dispose(GObject* pGObject);
static void adapter_update_tag_list(Adapter* pAdapter);
static void adapter_update_device_list(Adapter* pAdapter);

//GObject implementation
G_DEFINE_TYPE (Adapter, adapter, G_TYPE_OBJECT)

void adapter_class_init (AdapterClass* pAdapterClass)
{
	pAdapterClass->adapter_register = adapter_register;
	pAdapterClass->adapter_unregister = adapter_unregister;

	GObjectClass* pGObjectClass = G_OBJECT_CLASS (pAdapterClass);

	pGObjectClass->dispose = adapter_dispose;

	//Do not overload finalize
	//pGObjectClass->finalize = adapter_finalize;
}

void adapter_init(Adapter* pAdapter)
{
	pAdapter->adapterId = 0;
	pAdapter->objectPath = NULL;
	pAdapter->pDaemon = NULL;
	pAdapter->pNeardAdapter = NULL;
	pAdapter->pObjectSkeleton = NULL;

	pAdapter->pTagTable = g_hash_table_new(g_direct_hash, g_direct_equal);
	pAdapter->pDeviceTable = g_hash_table_new(g_direct_hash, g_direct_equal);
}

void adapter_dispose(GObject* pGObject)
{
	Adapter* pAdapter = ADAPTER(pGObject);

	if( pAdapter->objectPath != NULL )
	{
		adapter_unregister(pAdapter);
	}

	g_hash_table_destroy(pAdapter->pTagTable);
	g_hash_table_destroy(pAdapter->pDeviceTable);

	G_OBJECT_CLASS (adapter_parent_class)->dispose(pGObject);
}

void adapter_update_tag_list(Adapter* pAdapter)
{
	GList* pList = g_hash_table_get_values(pAdapter->pTagTable);
	gsize length = g_list_length(pList);

	const gchar* objectPaths[length + 1];

	for ( guint i = 0; i < length; i++ )
	{
		Tag* pTag = (Tag*)pList->data;

		objectPaths[i] = g_strdup(RECORD_CONTAINER(pTag)->objectPath);
		pList = g_list_next(pList);
	}
	g_list_free(pList);

	objectPaths[length] = NULL;

	neard_adapter_set_tags(pAdapter->pNeardAdapter, objectPaths);
}

void adapter_update_device_list(Adapter* pAdapter)
{
	GList* pList = g_hash_table_get_values(pAdapter->pDeviceTable);
	gsize length = g_list_length(pList);

	const gchar* objectPaths[length + 1];

	for ( guint i = 0; i < length; i++ )
	{
		Device* pDevice = (Device*)pList->data;

		objectPaths[i] = g_strdup(RECORD_CONTAINER(pDevice)->objectPath);
		pList = g_list_next(pList);
	}
	g_list_free(pList);

	objectPaths[length] = NULL;

	neard_adapter_set_devices(pAdapter->pNeardAdapter, objectPaths);
}

//DBUS commands handlers
static gboolean on_start_polling_loop (NeardAdapter *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
                const gchar* mode, gpointer pUserData);
static gboolean on_stop_polling_loop (NeardAdapter *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
                gpointer pUserData);

//Callbacks from HAL
static void adapter_hal_on_mode_changed_cb(hal_t* pHal, GObject* pAdapterObject, nfc_mode_t mode);
static void adapter_hal_on_polling_changed_cb(hal_t* pHal, GObject* pAdapterObject, gboolean polling);
static void adapter_hal_on_tag_detected_cb(hal_t* pHal, GObject* pAdapterObject, guint tagId);
static void adapter_hal_on_tag_lost_cb(hal_t* pHal, GObject* pAdapterObject, guint tagId);
static void adapter_hal_on_device_detected_cb(hal_t* pHal, GObject* pAdapterObject, guint deviceId);
static void adapter_hal_on_device_ndef_received_cb(hal_t* pHal, GObject* pAdapterObject, guint deviceId);
static void adapter_hal_on_device_lost_cb(hal_t* pHal, GObject* pAdapterObject, guint deviceId);

static const gchar* adapterProtocols[] = {"Felica", "MIFARE", "Jewel", "ISO-DEP", "NFC-DEP", NULL};

Adapter* adapter_new()
{
	Adapter* pAdapter = g_object_new(TYPE_ADAPTER, NULL);
	return pAdapter;
}

void adapter_register(Adapter* pAdapter, DBusDaemon* pDBusDaemon, guint adapterId)
{
	pAdapter->pDaemon = pDBusDaemon;
	g_object_ref(pDBusDaemon);

	pAdapter->adapterId = adapterId;

	pAdapter->objectPath = g_strdup_printf(DBUS_COMMON_OBJECT_PATH DBUS_ADAPTER_OBJECT_PATH, adapterId);
	g_info("Adapter at %s\n", pAdapter->objectPath);

	pAdapter->pObjectSkeleton = neard_object_skeleton_new(pAdapter->objectPath);
	pAdapter->pNeardAdapter = neard_adapter_skeleton_new();
	neard_object_skeleton_set_adapter(pAdapter->pObjectSkeleton, pAdapter->pNeardAdapter);

	//Connect signals
	g_signal_connect(pAdapter->pNeardAdapter, "handle-start-poll-loop",
			G_CALLBACK (on_start_polling_loop), pAdapter);
	g_signal_connect(pAdapter->pNeardAdapter, "handle-stop-poll-loop",
				G_CALLBACK (on_stop_polling_loop), pAdapter);

	//Set properties
    neard_adapter_set_name(pAdapter->pNeardAdapter, pAdapter->objectPath);
	neard_adapter_set_mode(pAdapter->pNeardAdapter, "Idle");
	neard_adapter_set_polling(pAdapter->pNeardAdapter, FALSE);
	neard_adapter_set_powered(pAdapter->pNeardAdapter, TRUE);
	neard_adapter_set_protocols(pAdapter->pNeardAdapter, adapterProtocols);
	neard_adapter_set_tags(pAdapter->pNeardAdapter, NULL);

	//Export
	g_dbus_object_manager_server_export( pAdapter->pDaemon->pObjectManagerServer, G_DBUS_OBJECT_SKELETON(pAdapter->pObjectSkeleton) );

	//Register callbacks
	hal_adapter_register(pAdapter->pDaemon->pHal, G_OBJECT(pAdapter),
			adapter_hal_on_mode_changed_cb, adapter_hal_on_polling_changed_cb,
			adapter_hal_on_tag_detected_cb, adapter_hal_on_tag_lost_cb,
			adapter_hal_on_device_detected_cb, adapter_hal_on_device_ndef_received_cb, adapter_hal_on_device_lost_cb);

	if(pAdapter->pDaemon->constantPoll)
	{
		hal_adapter_polling_loop_start(pAdapter->pDaemon->pHal, DEFAULT_POLLING_MODE);
	}
}

void adapter_unregister(Adapter* pAdapter)
{
	//Unregister callbacks
	hal_adapter_unregister(pAdapter->pDaemon->pHal, G_OBJECT(pAdapter));

	//Remove all tags
	//g_mutex_lock(&pAdapter->tagTableMutex);

	GHashTableIter iter;
	guint* pTagId;
	Tag* pTag;

	g_hash_table_iter_init (&iter, pAdapter->pTagTable);
	while (g_hash_table_iter_next(&iter, (gpointer*)&pTagId, (gpointer*)&pTag))
	{
		tag_unregister(pTag);

		//Remove from table
		g_hash_table_iter_remove(&iter);

		//Free tag
		g_object_unref(pTag);
	}


	//Disconnect signals
	g_dbus_object_manager_server_unexport( pAdapter->pDaemon->pObjectManagerServer, pAdapter->objectPath );

	g_object_unref(pAdapter->pNeardAdapter);
	g_object_unref(pAdapter->pObjectSkeleton);

	//g_dbus_connection_unregister_object(pAdapter->pDaemon->pConnection, pAdapter->registrationId);

	g_free(pAdapter->objectPath);
	pAdapter->objectPath = NULL;

	//Clear ref to DBusDaemon
	g_clear_object(&pAdapter->pDaemon);
}

gboolean on_start_polling_loop (NeardAdapter *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
                const gchar* mode, gpointer pUserData)
{
	Adapter* pAdapter = ADAPTER(pUserData);

	//Recover mode parameter

	/*	The mode parameter can have the following values:
		"Initiator", "Target" or "Dual". For any other value
		the adapter will fall back to initiator mode. */
	nfc_mode_t m;
	if( !g_strcmp0(mode, "Target") )
	{
		m = nfc_mode_target;
	}
	else if( !g_strcmp0(mode, "Dual") )
	{
		m = nfc_mode_dual;
	}
	else //Initiator
	{
		m = nfc_mode_initiator;
	}

	g_info("Start polling loop in %s mode\n", mode);

	hal_adapter_polling_loop_start(pAdapter->pDaemon->pHal, m);

	neard_adapter_complete_start_poll_loop(pInterfaceSkeleton, pInvocation);

	return TRUE;
}

gboolean on_stop_polling_loop (NeardAdapter *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
                gpointer pUserData)
{
	Adapter* pAdapter = ADAPTER(pUserData);

	g_info("Stop polling loop");

	hal_adapter_polling_loop_stop(pAdapter->pDaemon->pHal);

	neard_adapter_complete_stop_poll_loop(pInterfaceSkeleton, pInvocation);

	return TRUE;
}

//Callbacks
void adapter_hal_on_mode_changed_cb(hal_t* pHal, GObject* pAdapterObject, nfc_mode_t mode)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);

	const char* str;
	switch(mode)
	{
	case nfc_mode_idle:
		str = "Idle";
		break;
	case nfc_mode_target:
		str = "Target";
		break;
	case nfc_mode_initiator:
	default:
		str = "Initiator";
		break;
	}
	neard_adapter_set_mode(pAdapter->pNeardAdapter, str);
}

void adapter_hal_on_polling_changed_cb(hal_t* pHal, GObject* pAdapterObject, gboolean polling)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);
	neard_adapter_set_polling(pAdapter->pNeardAdapter, polling);
}

void adapter_hal_on_tag_detected_cb(hal_t* pHal, GObject* pAdapterObject, guint tagId)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);

	//Instantiate and register a new tag
	Tag* pTag = tag_new();

	//Make sure we keep a reference to the HAL impl of tag
	hal_tag_ref(pHal, tagId);

	tag_register(pTag, pAdapter, tagId);

	g_info("New tag %s", RECORD_CONTAINER(pTag)->objectPath);

	//MTX
	//g_mutex_lock(&pAdapter->tagTableMutex);
	g_hash_table_insert(pAdapter->pTagTable, GUINT_TO_POINTER(pTag->tagId), pTag);
	//g_mutex_unlock(&pAdapter->tagTableMutex);

	adapter_update_tag_list(pAdapter);

	//Send signal
	//KLUDGE: NeardAL generates this signal internally (diverges from spec)
	//neard_adapter_emit_tag_found(pAdapter->pNeardAdapter, RECORD_CONTAINER(pTag)->objectPath);
}

void adapter_hal_on_tag_lost_cb(hal_t* pHal, GObject* pAdapterObject, guint tagId)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);

	//Recover tag from hash table
	Tag* pTag = g_hash_table_lookup(pAdapter->pTagTable, GUINT_TO_POINTER(tagId));
	g_assert_nonnull(pTag);

	g_hash_table_remove(pAdapter->pTagTable, GUINT_TO_POINTER(tagId));

	adapter_update_tag_list(pAdapter);

	//KLUDGE: NeardAL generates this signal internally (diverges from spec)
	//Send signal
	//neard_adapter_emit_tag_lost(pAdapter->pNeardAdapter, RECORD_CONTAINER(pTag)->objectPath);

	g_info("Lost tag %s", RECORD_CONTAINER(pTag)->objectPath);

	tag_unregister(pTag);

	//Free tag
	g_object_unref(pTag);

	//Deref HAL impl of tag
	hal_tag_unref(pHal, tagId);

	//Check if we should restart polling
	if(pAdapter->pDaemon->constantPoll)
	{
		hal_adapter_polling_loop_start(pAdapter->pDaemon->pHal, DEFAULT_POLLING_MODE);//nfc_mode_initiator);
		g_info("Restarting polling loop");
	}
}

void adapter_hal_on_device_detected_cb(hal_t* pHal, GObject* pAdapterObject, guint deviceId)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);

	//Instantiate and register a new device
	Device* pDevice = device_new();

	//Make sure we keep a reference to the HAL impl of tag
	hal_device_ref(pHal, deviceId);

	device_register(pDevice, pAdapter, deviceId);

	g_info("New device %s", RECORD_CONTAINER(pDevice)->objectPath);

	g_hash_table_insert(pAdapter->pDeviceTable, GUINT_TO_POINTER(pDevice->deviceId), pDevice);

	adapter_update_tag_list(pAdapter);

	//Send signal
	//KLUDGE: NeardAL generates this signal internally (diverges from spec)
	//neard_adapter_emit_tag_found(pAdapter->pNeardAdapter, RECORD_CONTAINER(pDevice)->objectPath);
}

void adapter_hal_on_device_ndef_received_cb(hal_t* pHal, GObject* pAdapterObject, guint deviceId)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);

	//Recover device from hash table
	Device* pDevice = g_hash_table_lookup(pAdapter->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_assert_nonnull(pDevice);

	device_populate_records(pDevice);
}

void adapter_hal_on_device_lost_cb(hal_t* pHal, GObject* pAdapterObject, guint deviceId)
{
	Adapter* pAdapter = ADAPTER(pAdapterObject);

	//Recover device from hash table
	Device* pDevice = g_hash_table_lookup(pAdapter->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_assert_nonnull(pDevice);

	g_hash_table_remove(pAdapter->pDeviceTable, GUINT_TO_POINTER(deviceId));

	adapter_update_device_list(pAdapter);

	//KLUDGE: NeardAL generates this signal internally (diverges from spec)
	//Send signal
	//neard_adapter_emit_device_lost(pAdapter->pNeardAdapter, RECORD_CONTAINER(pDevice)->objectPath);

	g_info("Lost device %s", RECORD_CONTAINER(pDevice)->objectPath);

	device_unregister(pDevice);

	//Free device
	g_object_unref(pDevice);

	//Deref HAL impl of tag
	hal_device_unref(pHal, deviceId);
	//Check if we should restart polling

	if(pAdapter->pDaemon->constantPoll)
	{
		hal_adapter_polling_loop_start(pAdapter->pDaemon->pHal, DEFAULT_POLLING_MODE);
		g_info("Restarting polling loop");
	}
}
