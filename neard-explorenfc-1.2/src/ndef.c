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

#include "ndef.h"

#include <glib.h>
#include <glib-object.h>
#include "stdio.h"
#include "string.h"

#define RECORD_MB (1<<7)
#define RECORD_ME (1<<6)
#define RECORD_CF (1<<5)
#define RECORD_SR (1<<4)
#define RECORD_IL (1<<3)

#define RECORD_TNF_MASK 	  0x07
#define RECORD_TNF_EMPTY      0
#define RECORD_TNF_WELL_KNOWN 1
#define RECORD_TNF_MEDIA      2
#define RECORD_TNF_URI        3
#define RECORD_TNF_EXTERNAL   4
#define RECORD_TNF_UNKNOWN    5
#define RECORD_TNF_UNCHANGED  6

#define RECORD_RTD_WELL_KNOWN_SMART_POSTER "Sp"
#define RECORD_RTD_WELL_KNOWN_TEXT "T"
#define RECORD_RTD_WELL_KNOWN_URI "U"

#define RECORD_RTD_SP_LOCAL_ACTION "act"
#define RECORD_RTD_SP_LOCAL_SIZE "s"
#define RECORD_RTD_SP_LOCAL_TYPE "t"

#define RECORD_RTD_EXT_AAR "android.com:pkg"

//Local functions
static void ndef_record_class_init(NdefRecordClass* pNdefRecordClass);
static void ndef_record_init(NdefRecord* pNdefRecord);
static void ndef_record_dispose(GObject* pGObject);

static GList* ndef_message_parse_internal(guint8* data, gsize dataLength, gboolean smartPoster);
static NdefRecord* ndef_message_parse_smart_poster_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_text_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_uri_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_handover_request_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_handover_select_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_handover_carrier_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_aar_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_sp_local_action_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_sp_local_size_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_sp_local_type_record(guint8* data, gsize dataLength);
static NdefRecord* ndef_message_parse_mime_record(guint8* type, gsize typeLength, guint8* data, gsize dataLength);

static GBytes* ndef_message_record_make(guint8 tnf, gchar* type, guint8* payload, gsize payloadLength, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_smart_poster_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_text_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_uri_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_handover_request_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_handover_select_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_handover_carrier_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_aar_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_sp_local_action_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_sp_local_size_record(NdefRecord* pRecord, gboolean mb, gboolean me);
static GBytes* ndef_message_generate_sp_local_type_record(NdefRecord* pRecord, gboolean mb, gboolean me);
//static GBytes* ndef_message_generate_wifi_record(NdefRecord* pRecord, gboolean mb, gboolean me);

//GObject implementation
G_DEFINE_TYPE (NdefRecord, ndef_record, G_TYPE_OBJECT)

void ndef_record_class_init (NdefRecordClass* pNdefRecordClass)
{
	GObjectClass* pGObjectClass = G_OBJECT_CLASS (pNdefRecordClass);

	pGObjectClass->dispose = ndef_record_dispose;

	//Do not overload finalize
	//pGObjectClass->finalize = tag_finalize;
}

void ndef_record_init(NdefRecord* pNdefRecord)
{
	//Default values
	pNdefRecord->type = ndef_record_type_smart_poster;
	pNdefRecord->encoding = ndef_record_encoding_utf_8;
	pNdefRecord->language = NULL;
	pNdefRecord->representation = NULL;
	pNdefRecord->uri = NULL;
	pNdefRecord->mimeType = NULL;
	pNdefRecord->size = 0;
	pNdefRecord->action = NULL;
	pNdefRecord->androidPackage = NULL;

	//Internal values
	pNdefRecord->mimePayload = NULL;
}

void ndef_record_dispose(GObject* pGObject)
{
	NdefRecord* pNdefRecord = NDEF_RECORD(pGObject);
	if(pNdefRecord->language != NULL)
	{
		g_free(pNdefRecord->language);
		pNdefRecord->language = NULL;
	}
	if(pNdefRecord->representation != NULL)
	{
		g_free(pNdefRecord->representation);
		pNdefRecord->representation = NULL;
	}
	if(pNdefRecord->uri != NULL)
	{
		g_free(pNdefRecord->uri);
		pNdefRecord->uri = NULL;
	}
	if(pNdefRecord->mimeType != NULL)
	{
		g_free(pNdefRecord->mimeType);
		pNdefRecord->mimeType = NULL;
	}
	if(pNdefRecord->action != NULL)
	{
		g_free(pNdefRecord->action);
		pNdefRecord->action = NULL;
	}
	if(pNdefRecord->androidPackage != NULL)
	{
		g_free(pNdefRecord->androidPackage);
		pNdefRecord->androidPackage = NULL;
	}
	if(pNdefRecord->mimePayload != NULL)
	{
		g_bytes_unref(pNdefRecord->mimePayload);
		pNdefRecord->mimePayload = NULL;
	}
	G_OBJECT_CLASS (ndef_record_parent_class)->dispose(pGObject);
}

NdefRecord* ndef_record_new()
{
	NdefRecord* pNdefRecord = g_object_new(TYPE_NDEF_RECORD, NULL);
	return pNdefRecord;
}

NdefRecord* ndef_record_from_dictionary(GVariant* pVariant)
{
	gboolean valid = TRUE;
	NdefRecord* pNdefRecord = ndef_record_new();

	//Generate dictionary from pVariant
	GVariantDict* pDict = g_variant_dict_new(pVariant);

	//Try to unpack type
	char* typeStr = NULL;
	if( g_variant_dict_lookup(pDict, "Type", "&s", &typeStr) == TRUE ) //Use a pointer to internal data, do not copy
	{
		if(!strcmp(typeStr, "SmartPoster"))
		{
			pNdefRecord->type = ndef_record_type_smart_poster;
		}
		else if(!strcmp(typeStr, "Text"))
		{
			pNdefRecord->type = ndef_record_type_text;
		}
		else if(!strcmp(typeStr, "URI"))
		{
			pNdefRecord->type = ndef_record_type_uri;
		}
		else if(!strcmp(typeStr, "HandoverRequest"))
		{
			pNdefRecord->type = ndef_record_type_handover_request;
		}
		else if(!strcmp(typeStr, "HandoverSelect"))
		{
			pNdefRecord->type = ndef_record_type_handover_select;
		}
		else if(!strcmp(typeStr, "HandoverCarrier"))
		{
			pNdefRecord->type = ndef_record_type_handover_carrier;
		}
		else if(!strcmp(typeStr, "AAR"))
		{
			pNdefRecord->type = ndef_record_type_aar;
		}
		else
		{
			g_warning("Invalid record type %s\r\n", typeStr);
			valid = FALSE;
		}
	}
	else
	{
		g_error("Record type not found\r\n");
		valid = FALSE;
	}

	if(valid)
	{
		//Lookup other fields
		g_variant_dict_lookup(pDict, "Language", "s", &pNdefRecord->language);
		g_variant_dict_lookup(pDict, "Representation", "s", &pNdefRecord->representation);
		g_variant_dict_lookup(pDict, "URI", "s", &pNdefRecord->uri);
		g_variant_dict_lookup(pDict, "MIMEType", "s", &pNdefRecord->mimeType);
		g_variant_dict_lookup(pDict, "Size", "u", &pNdefRecord->size);
		g_variant_dict_lookup(pDict, "Action", "s", &pNdefRecord->action);
		g_variant_dict_lookup(pDict, "AndroidPackage", "s", &pNdefRecord->androidPackage);

		char* encodingStr = NULL;
		if( g_variant_dict_lookup(pDict, "Encoding", "&s", &encodingStr) == TRUE ) //Use a pointer to internal data, do not copy
		{
			if(!strcmp(encodingStr, "UTF-8"))
			{
				pNdefRecord->encoding = ndef_record_encoding_utf_8;
			}
			else if(!strcmp(encodingStr, "UTF-16"))
			{
				pNdefRecord->encoding = ndef_record_encoding_utf_16;
			}
			else
			{
				g_warning("Encoding %s unknown, defaulting to UTF-8\r\n", encodingStr);
				pNdefRecord->encoding = ndef_record_encoding_utf_8;
			}
		}
		else if( (pNdefRecord->type == ndef_record_type_text)
				|| ((pNdefRecord->type == ndef_record_type_smart_poster) && (pNdefRecord->representation != NULL) ) )
		{
			g_warning("Encoding not provided, defaulting to UTF-8\r\n");
			pNdefRecord->encoding = ndef_record_encoding_utf_8;
		}
	}

	//Release memory for dictionary
	g_variant_dict_unref(pDict);

	if(valid)
	{
		valid = ndef_record_validate(pNdefRecord);
		if(!valid)
		{
			g_warning("Record is invalid\r\n");
		}
	}

	if(!valid)
	{
		g_object_unref(pNdefRecord);
		pNdefRecord = NULL;
	}

	return pNdefRecord;
}

gboolean ndef_record_validate(NdefRecord* pRecord)
{
	gboolean valid = FALSE;
	switch(pRecord->type)
	{
	case ndef_record_type_smart_poster:
		valid = (pRecord->uri != NULL);
		if(pRecord->representation != NULL)
		{
			valid &= (pRecord->language != NULL);
		}
		break;
	case ndef_record_type_text:
		valid = (pRecord->representation != NULL)
			&& (pRecord->language != NULL);
		break;
	case ndef_record_type_uri:
		valid = (pRecord->uri != NULL);
		break;
	case ndef_record_type_handover_request:
		valid = TRUE;
		break;
	case ndef_record_type_handover_select:
		valid = TRUE;
		break;
	case ndef_record_type_handover_carrier:
		valid = TRUE;
		break;
	case ndef_record_type_aar:
	default:
		valid = TRUE;
		break;
	}
	return valid;
}

GList* ndef_message_parse(guint8* data, gsize dataLength) //Returns a list of NDEF records
{
	return ndef_message_parse_internal(data, dataLength, FALSE);
}

GList* ndef_message_parse_internal(guint8* data, gsize dataLength, gboolean smartPoster) //Returns a list of NDEF records
{
	GList* pList = NULL; //This is a valid list

	gboolean messageStart = TRUE;

	gboolean messageEnd = FALSE;
	while(!messageEnd && (dataLength > 1))
	{
		//Read first byte
		if(messageStart)
		{
			//Expect MB bit to be set
			if(!(*data & RECORD_MB))
			{
				g_warning("Not the message's start\r\n");
				break;
			}
			messageStart = FALSE;
		}
		if(*data & RECORD_ME)
		{
			messageEnd = TRUE;
		}

		gboolean shortRecord = (*data & RECORD_SR)?TRUE:FALSE;
		gboolean idPresent = (*data & RECORD_IL)?TRUE:FALSE;

		//Get type name format
		guint8 tnf = *data & RECORD_TNF_MASK;

		//Check length
		size_t minLength = 2;
		if(!shortRecord)
		{
			minLength += 3;
		}
		if(idPresent)
		{
			minLength++;
		}

		if( dataLength < minLength )
		{
			g_warning("Buffer is too short\r\n");
			break;
		}

		data++;
		dataLength--;

		//Read type length
		gsize typeLength = *data;

		data++;
		dataLength--;

		//Read payload length
		gsize payloadLength = 0;
		if(shortRecord)
		{
			payloadLength = *data;

			data++;
			dataLength--;
		}
		else
		{
			//24 bits length, big endian
			payloadLength = (data[0] << 16) | (data[1] << 8) | data[2];

			data += 3;
			dataLength -= 3;
		}

		//Read ID length (if relevant)
		gsize idLength = 0;
		if(idPresent)
		{
			idLength = *data;

			data++;
			dataLength--;
		}

		minLength = typeLength + idLength + payloadLength;
		if( dataLength < minLength )
		{
			g_warning("Buffer is too short\r\n");
			break;
		}

		//Read type
		NdefRecord* pRecord = NULL;
		switch(tnf)
		{
		case RECORD_TNF_WELL_KNOWN:
			//Do not accept smart poster record within smart poster record
			if( !smartPoster && (typeLength == strlen(RECORD_RTD_WELL_KNOWN_SMART_POSTER))
								&& (!memcmp(data, RECORD_RTD_WELL_KNOWN_SMART_POSTER, strlen(RECORD_RTD_WELL_KNOWN_SMART_POSTER))) )
			{
				pRecord = ndef_message_parse_smart_poster_record(&data[typeLength + idLength], payloadLength);
			}
			else if( (typeLength == strlen(RECORD_RTD_WELL_KNOWN_TEXT))
					&& (!memcmp(data, RECORD_RTD_WELL_KNOWN_TEXT, strlen(RECORD_RTD_WELL_KNOWN_TEXT))) )
			{
				pRecord = ndef_message_parse_text_record(&data[typeLength + idLength], payloadLength);
			}
			else if( (typeLength == strlen(RECORD_RTD_WELL_KNOWN_URI))
					&& (!memcmp(data, RECORD_RTD_WELL_KNOWN_URI, strlen(RECORD_RTD_WELL_KNOWN_URI))) )
			{
				pRecord = ndef_message_parse_uri_record(&data[typeLength + idLength], payloadLength);
			}
			else if(smartPoster) //Parse additional record types
			{
				if( (typeLength == strlen(RECORD_RTD_SP_LOCAL_ACTION))
					&& (!memcmp(data, RECORD_RTD_SP_LOCAL_ACTION, strlen(RECORD_RTD_SP_LOCAL_ACTION))) )
				{
					pRecord = ndef_message_parse_sp_local_action_record(&data[typeLength + idLength], payloadLength);
				}
				else if( (typeLength == strlen(RECORD_RTD_SP_LOCAL_SIZE))
					&& (!memcmp(data, RECORD_RTD_SP_LOCAL_SIZE, strlen(RECORD_RTD_SP_LOCAL_SIZE))) )
				{
					pRecord = ndef_message_parse_sp_local_size_record(&data[typeLength + idLength], payloadLength);
				}
				else if( (typeLength == strlen(RECORD_RTD_SP_LOCAL_TYPE))
					&& (!memcmp(data, RECORD_RTD_SP_LOCAL_TYPE, strlen(RECORD_RTD_SP_LOCAL_TYPE))) )
				{
					pRecord = ndef_message_parse_sp_local_type_record(&data[typeLength + idLength], payloadLength);
				}
			}
			break;
		case RECORD_TNF_MEDIA:
			pRecord = ndef_message_parse_mime_record(data, typeLength, &data[typeLength + idLength], payloadLength);
#if 0
			if( (typeLength == strlen(RECORD_MIME_WIFI))
								&& (!memcmp(data, RECORD_MIME_WIFI, strlen(RECORD_MIME_WIFI))) )
			{
				pRecord = ndef_message_parse_wifi_record(&data[typeLength + idLength], payloadLength);
			}
#endif
			break;
		case RECORD_TNF_EMPTY:
		case RECORD_TNF_URI:
			break;
		case RECORD_TNF_EXTERNAL:
			if( (typeLength == strlen(RECORD_RTD_EXT_AAR))
					&& (!memcmp(data, RECORD_RTD_EXT_AAR, strlen(RECORD_RTD_EXT_AAR))) )
			{
				pRecord = ndef_message_parse_aar_record(&data[typeLength + idLength], payloadLength);
			}
			break;
		case RECORD_TNF_UNKNOWN:
		case RECORD_TNF_UNCHANGED:
		default:
			break;
		}

		if(pRecord != NULL)
		{
			//Add it to the list
			pList = g_list_append(pList, pRecord);
		}
		else
		{
			//Skip record
			g_debug("Unsupported record, skip\r\n");
		}

		data += typeLength + idLength + payloadLength;
		if(typeLength + idLength + payloadLength > dataLength)
		{
			dataLength -= typeLength + idLength + payloadLength;
		}
		else
		{
			dataLength = 0;
		}
	}


	return pList;
}

NdefRecord* ndef_message_parse_smart_poster_record(guint8* data, gsize dataLength)
{
	gboolean foundURIRecord = FALSE;

	GList* pList = ndef_message_parse(data, dataLength);

	if(pList == NULL)
	{
		return NULL;
	}

	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_smart_poster;

	for (; pList != NULL; pList = g_list_next(pList))
	{
		NdefRecord* pLocalRecord = (NdefRecord*) pList->data;

		//Types allowed in a Smart Poster
		switch( pLocalRecord->type )
		{
		case ndef_record_type_uri:
			if( pRecord->uri == NULL )
			{
				pRecord->uri = g_strdup(pLocalRecord->uri);
				foundURIRecord = TRUE;
			}
			break;
		case ndef_record_type_text:
			if( pRecord->representation == NULL )
			{
				pRecord->language = g_strdup(pLocalRecord->language);
				pRecord->representation = g_strdup(pLocalRecord->representation);
			}
			break;
		case ndef_record_type_sp_local_action:
			if( pRecord->action == NULL )
			{
				pRecord->action = g_strdup(pLocalRecord->action);
			}
			break;
		case ndef_record_type_sp_local_size:
			if( pRecord->size == 0 )
			{
				pRecord->size = pLocalRecord->size;
			}
			break;
		case ndef_record_type_sp_local_type:
			if( pRecord->mimeType == NULL )
			{
				pRecord->mimeType = g_strdup(pLocalRecord->mimeType);
			}
			break;
		default:
			break;
		}

		g_object_unref(G_OBJECT(pLocalRecord));
	}

	g_list_free(pList);

	if(!foundURIRecord)
	{
		g_object_unref(G_OBJECT(pRecord));
		return NULL;
	}

	return pRecord; //TODO
}

NdefRecord* ndef_message_parse_text_record(guint8* data, gsize dataLength)
{
	if( dataLength < 1 )
	{
		return NULL;
	}

	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_text;

	//Check status byte
	if(data[0] & 0x80)
	{
		pRecord->encoding = ndef_record_encoding_utf_16;
	}
	else
	{
		pRecord->encoding = ndef_record_encoding_utf_8;
	}

	gsize languageCodeLength = data[0] & 0x3F;

	if(dataLength < 1 + languageCodeLength)
	{
		g_object_unref(G_OBJECT(pRecord));
		return NULL;
	}

	pRecord->language = g_malloc0(languageCodeLength + 1);
	memcpy(pRecord->language, data + 1, languageCodeLength);

	pRecord->representation = g_malloc0(dataLength - (1 + languageCodeLength) + 1);
	memcpy(pRecord->representation, data + 1 + languageCodeLength, dataLength - (1 + languageCodeLength));

	return pRecord;
}

#define ABBREVIATIONS_COUNT 36
static const gchar* abbreviations[ABBREVIATIONS_COUNT] =
{
		"",
		"http://www.",
		"https://www.",
		"http://",
		"https://",
		"tel:",
		"mailto:",
		"ftp://anonymous:anonymous@",
		"ftp://ftp.",
		"ftps://",
		"sftp://",
		"smb://",
		"nfs://",
		"ftp://",
		"dav://",
		"news:",
		"telnet://",
		"imap:",
		"rtsp://",
		"urn:",
		"pop:",
		"sip:",
		"sips:",
		"tftp:",
		"btspp://",
		"btl2cap://",
		"btgoep://",
		"tcpobex://",
		"irdaobex://",
		"file://",
		"urn:epc:id:",
		"urn:epc:tag:",
		"urn:epc:pat:",
		"urn:epc:raw:",
		"urn:epc:",
		"urn:nfc:",
};

NdefRecord* ndef_message_parse_uri_record(guint8* data, gsize dataLength)
{
	if( dataLength < 1 )
	{
		return NULL;
	}
	if( data[0] >= ABBREVIATIONS_COUNT )
	{
		return NULL;
	}

	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_uri;

	gsize strSize = strlen(abbreviations[data[0]]) + (dataLength - 1) + 1;

	pRecord->uri = g_malloc0( strSize );
	g_strlcpy( pRecord->uri, abbreviations[data[0]], strSize );
	memcpy( pRecord->uri + strlen(abbreviations[data[0]]), &data[1], dataLength - 1);

	return pRecord;
}

NdefRecord* ndef_message_parse_handover_request_record(guint8* data, gsize dataLength)
{
	return NULL; //TODO
}

NdefRecord* ndef_message_parse_handover_select_record(guint8* data, gsize dataLength)
{
	return NULL; //TODO
}

NdefRecord* ndef_message_parse_handover_carrier_record(guint8* data, gsize dataLength)
{
	return NULL; //TODO
}

NdefRecord* ndef_message_parse_aar_record(guint8* data, gsize dataLength)
{
	if( dataLength < 1 )
	{
		return NULL;
	}

	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_aar;

	gsize aarSize = dataLength + 1;

	pRecord->androidPackage = g_malloc0( aarSize );
	memcpy( pRecord->androidPackage, data, dataLength);
	pRecord->androidPackage[dataLength] = 0;

	return pRecord;
}

NdefRecord* ndef_message_parse_sp_local_action_record(guint8* data, gsize dataLength)
{
	if( dataLength != 1 )
	{
		return NULL;
	}

	if(data[0] > 2)
	{
		return NULL;
	}

	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_sp_local_action;

	switch(data[0])
	{
	case 0:
		pRecord->action = g_strdup("Do");
		break;
	case 1:
		pRecord->action = g_strdup("Save");
		break;
	case 2:
	default:
		pRecord->action = g_strdup("Edit");
		break;
	}

	return pRecord;
}

NdefRecord* ndef_message_parse_sp_local_size_record(guint8* data, gsize dataLength)
{
	if( dataLength != 4 )
	{
		return NULL;
	}

	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_sp_local_size;

	pRecord->size = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];

	return pRecord;
}

NdefRecord* ndef_message_parse_sp_local_type_record(guint8* data, gsize dataLength)
{
	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_sp_local_type;

	pRecord->mimeType = g_malloc0(dataLength + 1);
	memcpy(pRecord->mimeType, data, dataLength);

	return pRecord;
}

NdefRecord* ndef_message_parse_mime_record(guint8* type, gsize typeLength, guint8* data, gsize dataLength)
{
	NdefRecord* pRecord = ndef_record_new();

	pRecord->type = ndef_record_type_mime;

	pRecord->mimeType = g_malloc0(typeLength + 1);
	memcpy(pRecord->mimeType, type, typeLength);

	pRecord->mimePayload = g_bytes_new(data, dataLength); //Data is copied

	return pRecord;
}

GBytes* ndef_message_generate_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	GBytes* pBytes = NULL;

	switch(pRecord->type)
	{
	case ndef_record_type_smart_poster:
		pBytes = ndef_message_generate_smart_poster_record(pRecord, mb, me);
		break;
	case ndef_record_type_text:
		pBytes = ndef_message_generate_text_record(pRecord, mb, me);
		break;
	case ndef_record_type_uri:
		pBytes = ndef_message_generate_uri_record(pRecord, mb, me);
		break;
	case ndef_record_type_handover_request:
		pBytes = ndef_message_generate_handover_request_record(pRecord, mb, me);
		break;
	case ndef_record_type_handover_select:
		pBytes = ndef_message_generate_handover_select_record(pRecord, mb, me);
		break;
	case ndef_record_type_handover_carrier:
		pBytes = ndef_message_generate_handover_carrier_record(pRecord, mb, me);
		break;
	case ndef_record_type_aar:
		pBytes = ndef_message_generate_aar_record(pRecord, mb, me);
		break;
	//Local smart poster records
	case ndef_record_type_sp_local_action:
		pBytes = ndef_message_generate_sp_local_action_record(pRecord, mb, me);
		break;
	case ndef_record_type_sp_local_size:
		pBytes = ndef_message_generate_sp_local_size_record(pRecord, mb, me);
		break;
	case ndef_record_type_sp_local_type:
		pBytes = ndef_message_generate_sp_local_type_record(pRecord, mb, me);
		break;
	default:
		break;
	}
	return pBytes;
}

//Helper
static inline gsize ndef_message_record_length(gsize typeLength, gsize payloadLength)
{
	//Assume short record
	gsize length = 1 /* Header */ + 1 /* Type length */ + 1 /* Payload length */ + 0 /* ID length */ + typeLength + payloadLength;
	if( payloadLength > 0xFF )
	{
		length += 3 /* Full payload length */;
	}
	return length;
}

GBytes* ndef_message_record_make(guint8 tnf, gchar* type, guint8* payload, gsize payloadLength, gboolean mb, gboolean me)
{
	gsize typeLength = strlen(type);

	gsize length = ndef_message_record_length(typeLength, payloadLength);
	if(length == 0)
	{
		return NULL;
	}

	guint8* buffer = g_malloc( length );

	gsize p = 0;
	buffer[p] = (tnf & RECORD_TNF_MASK);
	if(payloadLength <= 0xFF)
	{
		buffer[p] |= RECORD_SR;
	}
	if(mb) //Message begin
	{
		buffer[p] |= RECORD_MB;
	}
	if(me) //Message end
	{
		buffer[p] |= RECORD_ME;
	}
	p++;

	//Type length
	buffer[p++] = typeLength & 0xFF;

	//Payload length
	if(payloadLength <= 0xFF)
	{
		buffer[p++] = payloadLength & 0xFF;
	}
	else
	{
		buffer[p++] = (payloadLength >> 24) & 0xFF;
		buffer[p++] = (payloadLength >> 16) & 0xFF;
		buffer[p++] = (payloadLength >>  8) & 0xFF;
		buffer[p++] = (payloadLength >>  0) & 0xFF;
	}

	//ID length not present

	//Type
	memcpy(&buffer[p], type, typeLength);
	p += typeLength;

	//ID (NULL)

	//Payload
	memcpy(&buffer[p], payload, payloadLength);
	p += payloadLength;

	return g_bytes_new_take(buffer, length);
}

GBytes* ndef_message_generate_smart_poster_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	//Split into individual records
	GList* pList = NULL;
	NdefRecord* pLocalRecord;
	if( pRecord->uri != NULL )
	{
		pLocalRecord = ndef_record_new();
		pLocalRecord->type = ndef_record_type_uri;
		pLocalRecord->uri = g_strdup(pRecord->uri);
		pList = g_list_append(pList, pLocalRecord);
	}
	if( pRecord->representation != NULL )
	{
		pLocalRecord = ndef_record_new();
		pLocalRecord->type = ndef_record_type_text;
		pLocalRecord->representation = g_strdup(pRecord->representation);
		pLocalRecord->language = g_strdup(pRecord->language);
		pLocalRecord->encoding = pRecord->encoding;
		pList = g_list_append(pList, pLocalRecord);
	}
	if( pRecord->action != NULL )
	{
		pLocalRecord = ndef_record_new();
		pLocalRecord->type = ndef_record_type_sp_local_action;
		pLocalRecord->action = g_strdup(pRecord->action);
		pList = g_list_append(pList, pLocalRecord);
	}
	if( pRecord->size != 0 )
	{
		pLocalRecord = ndef_record_new();
		pLocalRecord->type = ndef_record_type_sp_local_size;
		pLocalRecord->size = pRecord->size;
		pList = g_list_append(pList, pLocalRecord);
	}
	if( pRecord->mimeType != NULL )
	{
		pLocalRecord = ndef_record_new();
		pLocalRecord->type = ndef_record_type_sp_local_type;
		pLocalRecord->mimeType = g_strdup(pRecord->mimeType);
		pList = g_list_append(pList, pLocalRecord);
	}

	guint8* payload = NULL;
	gsize payloadLength = 0;
	ndef_message_generate(pList, &payload, &payloadLength);

	for (GList* pLocalList = pList; pLocalList != NULL; pLocalList = g_list_next(pLocalList))
	{
		g_object_unref(pList->data);
	}

	g_list_free(pList);

	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_WELL_KNOWN, RECORD_RTD_WELL_KNOWN_SMART_POSTER, payload, payloadLength, mb, me);
	g_free(payload);
	return pBytes;
}

GBytes* ndef_message_generate_text_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	if( strlen(pRecord->language) > 0x1F)
	{
		return NULL; //Language name too long
	}
	gsize payloadLength = 1 + strlen(pRecord->language) + strlen(pRecord->representation);
	guint8* payload = g_malloc(payloadLength);

	//First byte is status byte
	gsize p = 0;
	if(pRecord->encoding == ndef_record_encoding_utf_16)
	{
		payload[p] = 0x80;
	}
	else //if(pRecord->encoding == ndef_record_encoding_utf_8)
	{
		payload[p] = 0x00;
	}

	//Language length
	payload[p] |= strlen(pRecord->language) & 0x1F;
	p++;

	//Language code
	memcpy(&payload[p], pRecord->language, strlen(pRecord->language));
	p += strlen(pRecord->language);

	//Text
	memcpy(&payload[p], pRecord->representation, strlen(pRecord->representation));
	p += strlen(pRecord->representation);

	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_WELL_KNOWN, RECORD_RTD_WELL_KNOWN_TEXT, payload, payloadLength, mb, me);
	g_free(payload);
	return pBytes;
}

GBytes* ndef_message_generate_uri_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	//Find best abbreviation
	const gchar* abbreviation = abbreviations[0];
	int code = 0;
	for(int i = 1; i < ABBREVIATIONS_COUNT; i++)
	{
		if( ( strlen(pRecord->uri) >= strlen(abbreviations[i]) )
				&& ( strlen(abbreviations[i]) > strlen(abbreviation) )
				&& ( !memcmp(pRecord->uri, abbreviations[i], strlen(abbreviations[i])) )
				)
		{
			abbreviation = abbreviations[i];
			code = i;
		}
	}

	//Compute length
	gsize payloadLength = 1 + strlen(pRecord->uri) - strlen(abbreviation);
	guint8* payload = g_malloc(payloadLength);
	payload[0] = code & 0xFF;
	memcpy(&payload[1], pRecord->uri + strlen(abbreviation), strlen(pRecord->uri) - strlen(abbreviation));

	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_WELL_KNOWN, RECORD_RTD_WELL_KNOWN_URI, payload, payloadLength, mb, me);
	g_free(payload);
	return pBytes;
}

GBytes* ndef_message_generate_handover_request_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	return NULL; //TODO
}

GBytes* ndef_message_generate_handover_select_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	return NULL; //TODO
}

GBytes* ndef_message_generate_handover_carrier_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	return NULL; //TODO
}

GBytes* ndef_message_generate_aar_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_EXTERNAL, RECORD_RTD_EXT_AAR, (guint8*)pRecord->androidPackage, strlen(pRecord->androidPackage), mb, me);
	return pBytes;
}

GBytes* ndef_message_generate_sp_local_action_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	gsize payloadLength = 1;
	guint8* payload = g_malloc(payloadLength);
	if( !strcmp(pRecord->action, "Do") )
	{
		payload[0] = 0;
	}
	else if( !strcmp(pRecord->action, "Save") )
	{
		payload[0] = 1;
	}
	else //if( !strcmp(pRecord->action, "Edit") )
	{
		payload[0] = 2;
	}

	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_WELL_KNOWN, RECORD_RTD_SP_LOCAL_ACTION, payload, payloadLength, mb, me);
	g_free(payload);
	return pBytes;
}

GBytes* ndef_message_generate_sp_local_size_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	gsize payloadLength = 4;
	guint8* payload = g_malloc(payloadLength);
	payload[0] = (pRecord->size >> 24) & 0xFF;
	payload[1] = (pRecord->size >> 16) & 0xFF;
	payload[2] = (pRecord->size >>  8) & 0xFF;
	payload[3] = (pRecord->size >>  0) & 0xFF;

	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_WELL_KNOWN, RECORD_RTD_SP_LOCAL_SIZE, payload, payloadLength, mb, me);
	g_free(payload);
	return pBytes;
}

GBytes* ndef_message_generate_sp_local_type_record(NdefRecord* pRecord, gboolean mb, gboolean me)
{
	//Compute length
	gsize payloadLength = strlen(pRecord->mimeType);
	guint8* payload = g_malloc(payloadLength);
	memcpy(&payload[0], pRecord->mimeType, strlen(pRecord->mimeType));

	GBytes* pBytes = ndef_message_record_make(RECORD_TNF_WELL_KNOWN, RECORD_RTD_SP_LOCAL_TYPE, payload, payloadLength, mb, me);
	g_free(payload);
	return pBytes;
}


void ndef_message_generate(GList* pList, guint8** pData, gsize* pDataLength)
{
	GList* pBytesList = NULL;

	*pDataLength = 0;

	gboolean mb = TRUE;
	gboolean me = FALSE;
	for (; pList != NULL; pList = g_list_next(pList))
	{
		if( g_list_next(pList) == NULL ) //Last record?
		{
			me = TRUE;
		}

		NdefRecord* pRecord = NDEF_RECORD(pList->data);
		GBytes* pBytes = ndef_message_generate_record(pRecord, mb, me);

		if( pBytes == NULL )
		{
			g_warning("Generating empty record\r\n");
			pBytes = ndef_message_record_make(RECORD_TNF_EMPTY, NULL, NULL, 0, mb, me);
		}

		pBytesList = g_list_append(pBytesList, pBytes);
		*pDataLength += g_bytes_get_size(pBytes);

		mb = FALSE;
	}

	*pData = g_malloc(*pDataLength);
	gsize pos = 0;

	GList* pBytesListStart = pBytesList;

	for (; pBytesList != NULL; pBytesList = g_list_next(pBytesList))
	{
		GBytes* pBytes = (GBytes*) pBytesList->data;
		memcpy(*pData + pos, g_bytes_get_data(pBytes, NULL), g_bytes_get_size(pBytes));
		pos += g_bytes_get_size(pBytes);
		g_bytes_unref(pBytes);
	}

	g_list_free(pBytesListStart);

}
