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

#include "record.h"
#include "dbus-daemon.h"
#include "dbus-parameters.h"
#include "record-container.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

//Local functions
static void record_class_init (RecordClass* pRecordClass);
static void record_init(Record* pRecord);
static void record_dispose(GObject* pGObject);
static gchar* ndef_record_type_strdup(NdefRecord* pNdefRecord);
static gchar* ndef_record_encoding_strdup(NdefRecord* pNdefRecord);


//GObject implementation
G_DEFINE_TYPE (Record, record, G_TYPE_OBJECT)

void record_class_init (RecordClass* pRecordClass)
{
	pRecordClass->record_register = record_register;
	pRecordClass->record_unregister = record_unregister;

	GObjectClass* pGObjectClass = G_OBJECT_CLASS (pRecordClass);

	pGObjectClass->dispose = record_dispose;

	//Do not overload finalize
	//pGObjectClass->finalize = record_finalize;
}

void record_init(Record* pRecord)
{
	pRecord->objectPath = NULL;
	pRecord->pRecordContainer = NULL;
	pRecord->pNeardRecord = NULL;
	pRecord->pObjectSkeleton = NULL;
	pRecord->recordId = 0;
}

void record_dispose(GObject* pGObject)
{
	Record* pRecord = RECORD(pGObject);

	if( pRecord->objectPath != NULL )
	{
		record_unregister(pRecord);
	}

	G_OBJECT_CLASS (record_parent_class)->dispose(pGObject);
}

gchar* ndef_record_type_strdup(NdefRecord* pNdefRecord)
{
	gchar* str = NULL;
	switch(pNdefRecord->type)
	{
	case ndef_record_type_smart_poster:
		str = "SmartPoster";
		break;
	case ndef_record_type_text:
		str = "Text";
		break;
	case ndef_record_type_uri:
		str = "URI";
		break;
	case ndef_record_type_handover_request:
		str = "HandoverRequest";
		break;
	case ndef_record_type_handover_select:
		str = "HandoverSelect";
		break;
	case ndef_record_type_handover_carrier:
		str = "HandoverCarrier";
		break;
	case ndef_record_type_aar:
		str = "AAR";
		break;
	case ndef_record_type_mime:
		str = "MIME";
		break;
	default:
		str = "Unknown";
		break;
	}
	return g_strdup(str);
}

gchar* ndef_record_encoding_strdup(NdefRecord* pNdefRecord)
{
	gchar* str = NULL;
	switch(pNdefRecord->encoding)
	{
	case ndef_record_encoding_utf_8:
		str = "UTF-8";
		break;
	case ndef_record_encoding_utf_16:
	default:
		str = "UTF-16";
		break;
	}
	return g_strdup(str);
}

Record* record_new()
{
	Record* pRecord = g_object_new(TYPE_RECORD, NULL);
	return pRecord;
}

void record_register(Record* pRecord, RecordContainer* pRecordContainer, NdefRecord* pNdefRecord, guint recordId)
{
	pRecord->pRecordContainer = pRecordContainer;
	g_object_ref(pRecordContainer);

	pRecord->recordId = recordId;

	pRecord->objectPath = g_strdup_printf("%s"DBUS_RECORD_OBJECT_PATH, pRecordContainer->objectPath, recordId);
	g_info("Record at %s\r\n", pRecord->objectPath);

	pRecord->pObjectSkeleton = neard_object_skeleton_new(pRecord->objectPath);
	pRecord->pNeardRecord = neard_record_skeleton_new();
	neard_object_skeleton_set_record(pRecord->pObjectSkeleton, pRecord->pNeardRecord);

	gchar* typeStr = ndef_record_type_strdup(pNdefRecord);
    neard_record_set_type_(pRecord->pNeardRecord, typeStr);
    g_free(typeStr);

    gchar* encodingStr = ndef_record_encoding_strdup(pNdefRecord);
    neard_record_set_encoding(pRecord->pNeardRecord, encodingStr);
    g_free(encodingStr);

    neard_record_set_name(pRecord->pNeardRecord, pRecord->objectPath);
    neard_record_set_language(pRecord->pNeardRecord, pNdefRecord->language);
    neard_record_set_representation(pRecord->pNeardRecord, pNdefRecord->representation);
    neard_record_set_uri(pRecord->pNeardRecord, pNdefRecord->uri);
    neard_record_set_mimetype(pRecord->pNeardRecord, pNdefRecord->mimeType);
    neard_record_set_size(pRecord->pNeardRecord, pNdefRecord->size);
    neard_record_set_action(pRecord->pNeardRecord, pNdefRecord->action);
    neard_record_set_android_package(pRecord->pNeardRecord, pNdefRecord->androidPackage);

	//Export
	g_dbus_object_manager_server_export( pRecord->pRecordContainer->pAdapter->pDaemon->pObjectManagerServer, G_DBUS_OBJECT_SKELETON(pRecord->pObjectSkeleton) );
}

void record_unregister(Record* pRecord)
{
	g_dbus_object_manager_server_unexport( pRecord->pRecordContainer->pAdapter->pDaemon->pObjectManagerServer, pRecord->objectPath );

	g_object_unref(pRecord->pNeardRecord);
	g_object_unref(pRecord->pObjectSkeleton);

	g_free(pRecord->objectPath);
	pRecord->objectPath = NULL;

	//Clear ref to RecordContainer
	g_clear_object(&pRecord->pRecordContainer);
}
