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

#include "tag.h"
#include "dbus-daemon.h"
#include "dbus-parameters.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include "hal.h"
#include "ndef.h"
#include "record.h"
#include "record-container.h"

//Local functions
static void tag_class_init (TagClass* pTagClass);
static void tag_init(Tag* pTag);
static void tag_dispose(GObject* pGObject);

//GObject implementation
G_DEFINE_TYPE (Tag, tag, TYPE_RECORD_CONTAINER)

void tag_class_init (TagClass* pTagClass)
{
	pTagClass->tag_register = tag_register;
	pTagClass->tag_unregister = tag_unregister;

	RecordContainerClass* pRecordContainerClass = RECORD_CONTAINER_CLASS (pTagClass);

	pRecordContainerClass->dispose = tag_dispose;

	//Do not overload finalize
	//pRecordContainerClass->finalize = tag_finalize;
}

void tag_init(Tag* pTag)
{
	RECORD_CONTAINER(pTag)->objectPath = NULL;
	RECORD_CONTAINER(pTag)->pAdapter = NULL;
	pTag->pNeardTag = NULL;
	pTag->pObjectSkeleton = NULL;
	pTag->tagId = 0;
	pTag->pRecordTable = g_hash_table_new(g_direct_hash, g_direct_equal);
	pTag->pRawNDEF = NULL;
}

void tag_dispose(GObject* pGObject)
{
	Tag* pTag = TAG(pGObject);

	if( RECORD_CONTAINER(pTag)->objectPath != NULL )
	{
		tag_unregister(pTag);
	}

	RECORD_CONTAINER_CLASS (tag_parent_class)->dispose(pGObject);
}

//DBUS commands handlers
static gboolean on_write (NeardTag *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							GVariant *arg_attributes, gpointer pUserData);
static gboolean on_get_raw_ndef (NeardTag *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							gpointer pUserData);

Tag* tag_new()
{
	Tag* pTag = g_object_new(TYPE_TAG, NULL);
	return pTag;
}

static GVariant* g_variant_new_from_raw_bytes(const guint8* array, gsize size)
{
	GBytes* pBytes = g_bytes_new(array, size);
	GVariant* pVariant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, pBytes, FALSE);
	g_bytes_unref(pBytes);
	return pVariant; //Floating
}

void tag_register(Tag* pTag, Adapter* pAdapter, guint tagId)
{
	RECORD_CONTAINER(pTag)->pAdapter = pAdapter;
	g_object_ref(pAdapter);

	pTag->tagId = tagId;

	RECORD_CONTAINER(pTag)->objectPath = g_strdup_printf("%s"DBUS_TAG_OBJECT_PATH, pAdapter->objectPath, tagId);
	g_info("Tag at %s\n", RECORD_CONTAINER(pTag)->objectPath);

	pTag->pObjectSkeleton = neard_object_skeleton_new(RECORD_CONTAINER(pTag)->objectPath);
	pTag->pNeardTag = neard_tag_skeleton_new();
	neard_object_skeleton_set_tag(pTag->pObjectSkeleton, pTag->pNeardTag);

	//Connect signals
	g_signal_connect(pTag->pNeardTag, "handle-write",
					G_CALLBACK (on_write), pTag);
	g_signal_connect(pTag->pNeardTag, "handle-get-raw-ndef",
						G_CALLBACK (on_get_raw_ndef), pTag);

    neard_tag_set_name(pTag->pNeardTag, RECORD_CONTAINER(pTag)->objectPath);
	neard_tag_set_adapter(pTag->pNeardTag, pAdapter->objectPath);

	gchar* typeStr;
	gchar* protocolStr;
	switch(hal_tag_get_type(RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, tagId))
	{
	case nfc_tag_type_1:
		typeStr = g_strdup("Type 1");
		protocolStr = g_strdup("Jewel");
		break;
	case nfc_tag_type_2:
		typeStr = g_strdup("Type 2");
		protocolStr = g_strdup("MIFARE");
		break;
	case nfc_tag_type_3:
		typeStr = g_strdup("Type 3");
		protocolStr = g_strdup("Felica");
		break;
	case nfc_tag_type_4:
	default:
		typeStr = g_strdup("Type 4");
		protocolStr = g_strdup("ISO-DEP");
		break;
	}

	neard_tag_set_type_(pTag->pNeardTag, typeStr);
	neard_tag_set_protocol(pTag->pNeardTag, protocolStr);
	g_free(typeStr);
	g_free(protocolStr);

	//Populate records
	tag_populate_records(pTag);

	neard_tag_set_read_only(pTag->pNeardTag, hal_tag_is_readonly(RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, tagId));

	//See if tag is ISO14443A compliant
	if( hal_tag_is_iso14443a( RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, tagId ) )
	{
		guint8 atqa[2];
		guint8 uid[10];
		gsize uidLength;
		guint8 sak;

		hal_tag_get_iso14443a_params( RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, tagId,
				atqa, &sak, uid, &uidLength );

		neard_tag_set_iso14443a_uid(pTag->pNeardTag, g_variant_new_from_raw_bytes(uid, uidLength));
		neard_tag_set_iso14443a_sak(pTag->pNeardTag, g_variant_new_from_raw_bytes(&sak, 1));
		neard_tag_set_iso14443a_atqa(pTag->pNeardTag, g_variant_new_from_raw_bytes(atqa, sizeof(atqa)));
	}

	//See if tag is ISO14443A compliant
	if( hal_tag_is_felica( RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, tagId ) )
	{
		guint8 manufacturer[2];
		guint8 cid[6];
		guint8 ic[2];
		guint8 maxRespTimes[6];

		hal_tag_get_felica_params( RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, tagId,
				manufacturer, cid, ic, maxRespTimes );

		neard_tag_set_felica_manufacturer(pTag->pNeardTag, g_variant_new_from_raw_bytes(manufacturer, sizeof(manufacturer)));
		neard_tag_set_felica_cid(pTag->pNeardTag, g_variant_new_from_raw_bytes(cid, sizeof(cid)));
		neard_tag_set_felica_ic(pTag->pNeardTag, g_variant_new_from_raw_bytes(ic, sizeof(ic)));
		neard_tag_set_felica_max_resp_times(pTag->pNeardTag, g_variant_new_from_raw_bytes(maxRespTimes, sizeof(maxRespTimes)));
	}

	//Export
	g_dbus_object_manager_server_export( RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pObjectManagerServer, G_DBUS_OBJECT_SKELETON(pTag->pObjectSkeleton) );
}

void tag_populate_records(Tag* pTag)
{
	//Get NDEF
	guint8* buffer = NULL;
	gsize bufferLength = 0;
	hal_tag_get_ndef(RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, pTag->tagId, &buffer, &bufferLength);

	if(buffer != NULL)
	{
		GList* pList = ndef_message_parse(buffer, bufferLength);
		pTag->pRawNDEF = g_bytes_new_take(buffer, bufferLength); //buffer is now owned by pRawNDEF
		gsize length = g_list_length(pList);

		const gchar* objectPaths[length + 1];
		guint recordId = 0;

		GVariantBuilder variantBld;
		g_variant_builder_init(&variantBld, G_VARIANT_TYPE("ao"));

		for (; pList != NULL; pList = g_list_next(pList))
		{
			NdefRecord* pNdefRecord = (NdefRecord*) pList->data;

			//Check various agents that might have been registered
			dbus_daemon_check_ndef_record(RECORD_CONTAINER(pTag)->pAdapter->pDaemon, pNdefRecord);

			Record* pRecord = record_new();
			record_register(pRecord, RECORD_CONTAINER(pTag), pNdefRecord, recordId);

			g_object_unref(G_OBJECT(pNdefRecord));

			g_hash_table_insert(pTag->pRecordTable, GUINT_TO_POINTER(pRecord->recordId), pRecord);

			//Add object path to list
			objectPaths[recordId] = g_strdup(pRecord->objectPath);
			g_variant_builder_add(&variantBld, "o", pRecord->objectPath);

			recordId++;
		}

		g_list_free(pList);

		objectPaths[length] = NULL;
		//neard_tag_emit_property_changed(pTag->pNeardTag, "Records", g_variant_new_variant(g_variant_new("(ao)", &variantBld)));

		neard_tag_set_records(pTag->pNeardTag, objectPaths);
	}
}

void tag_unregister(Tag* pTag)
{
	g_dbus_object_manager_server_unexport( RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pObjectManagerServer, RECORD_CONTAINER(pTag)->objectPath );

	GHashTableIter iter;
	guint* pRecordId;
	Record* pRecord;

	//Remove & free records
	g_hash_table_iter_init (&iter, pTag->pRecordTable);
	while (g_hash_table_iter_next(&iter, (gpointer*)&pRecordId, (gpointer*)&pRecord))
	{
		record_unregister(pRecord);

		//Remove from table
		g_hash_table_iter_remove(&iter);

		//Free record
		g_object_unref(pRecord);
	}

	g_object_unref(pTag->pNeardTag);
	g_object_unref(pTag->pObjectSkeleton);

	//g_dbus_connection_unregister_object(pAdapter->pDaemon->pConnection, pAdapter->registrationId);

	if( pTag->pRawNDEF != NULL )
	{
		g_bytes_unref(pTag->pRawNDEF);
		pTag->pRawNDEF = NULL;
	}

	g_free(RECORD_CONTAINER(pTag)->objectPath);
	RECORD_CONTAINER(pTag)->objectPath = NULL;

	//Clear ref to Adapter
	g_clear_object(&RECORD_CONTAINER(pTag)->pAdapter);
}

//Local functions
gboolean on_write (NeardTag *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							GVariant *arg_attributes, gpointer pUserData)
{
	Tag* pTag = (Tag*) pUserData;

	//Build NdefRecord instance based on array of dictionaries
	NdefRecord* pNdefRecord = ndef_record_from_dictionary(arg_attributes);

	//Create a 1-long list
	GList* pList = g_list_append(NULL, pNdefRecord);

	guint8* buffer = NULL;
	gsize bufferLength = 0;
	ndef_message_generate(pList, &buffer, &bufferLength);
	hal_tag_write_ndef(RECORD_CONTAINER(pTag)->pAdapter->pDaemon->pHal, pTag->tagId, buffer, bufferLength);

	g_list_free(pList);

	if( buffer != NULL )
	{
		g_free(buffer);
	}

	neard_tag_complete_write(pInterfaceSkeleton, pInvocation);
	return TRUE;
}

gboolean on_get_raw_ndef (NeardTag *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
							gpointer pUserData)
{
	Tag* pTag = (Tag*) pUserData;

	GBytes* pRawNDEF = pTag->pRawNDEF;

	if(pRawNDEF == NULL)
	{
		pRawNDEF = g_bytes_new(NULL, 0);
	}
	else
	{
		g_bytes_ref(pRawNDEF);
	}

	GVariant* pNdefVariant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, pRawNDEF, FALSE);

	neard_tag_complete_get_raw_ndef(pInterfaceSkeleton, pInvocation, pNdefVariant);

	g_object_unref(pNdefVariant);
	g_bytes_unref(pRawNDEF);

	return TRUE;
}
