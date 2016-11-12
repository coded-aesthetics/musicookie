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
/**
 * \file ndef.h
 */
/** \defgroup NDEFGp NDEF
 * NDEF messages parsing and generating
 *  @{
 */

#ifndef NDEF_H_
#define NDEF_H_

#include <glib.h>
#include <glib-object.h>

#define TYPE_NDEF_RECORD   (ndef_record_get_type               ())
#define NDEF_RECORD(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NDEF_RECORD, NdefRecord))
#define NDEF_RECORD_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_NDEF_RECORD, NdefRecordClass))
#define NDEF_RECORD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_NDEF_RECORD, NdefRecordClass))

/** NDEF Record Type
 *
 */
enum ndef_record_type
{
	ndef_record_type_smart_poster, ///< Smart Poster
	ndef_record_type_text, ///< Text
	ndef_record_type_uri, ///< URI
	ndef_record_type_handover_request, ///< Handover request
	ndef_record_type_handover_select, ///< Handover select
	ndef_record_type_handover_carrier, ///< Handover carrier
	ndef_record_type_aar, ///< Android Application Record
	ndef_record_type_mime, ///< MIME

	//Local smart poster records
	ndef_record_type_sp_local_action, /// Action (within Smart Poster)
	ndef_record_type_sp_local_size, /// Size (within Smart Poster)
	ndef_record_type_sp_local_type /// Type (within Smart Poster)
};
typedef enum ndef_record_type ndef_record_type_t; ///< NDEF Record Type

/** NDEF Record Encoding
 *
 */
enum ndef_record_encoding
{
	ndef_record_encoding_utf_8, ///< UTF-8
	ndef_record_encoding_utf_16 ///< UTF-16
};
typedef enum ndef_record_encoding ndef_record_encoding_t; ///< NDEF Record Encoding

/** NDEF Record
 *
 */
struct ndef_record
{
	GObject parentInstance; ///< GObject instance
	ndef_record_type_t type; ///< Record type
	ndef_record_encoding_t encoding; ///< Record encoding
	gchar* language; ///< Record language (can be NULL)
	gchar* representation; ///< Record representation (can be NULL)
	gchar* uri; ///< Record URI (can be NULL)
	gchar* mimeType; ///< Record MIME type (can be NULL)
	gsize size; ///< Record size (0 if unset)
	gchar* action; ///< Record Action (can be NULL)
	gchar* androidPackage; ///< Record Android Package (can be NULL)

	//Internal fields
	GBytes* mimePayload; ///< MIME Payload
};
typedef struct ndef_record NdefRecord; ///< NDEF Record

/** NDEF Record class
 *
 */
struct ndef_record_class
{
	GObjectClass parentClass; ///< Parent GObjectClass
};
typedef struct ndef_record_class NdefRecordClass; ///< NDEF Record class

/** Create a new NDEF Record instance
 * \return new NDEF Record instance
 */
NdefRecord* ndef_record_new();

/** Create a new NDEF Record instance based on dictionary (variant)
 * \param pVariant variant dictionary (a{sv}) containing record values according to the neard DBUS spec
 * \return new NDEF Record instance
 */
NdefRecord* ndef_record_from_dictionary(GVariant* pVariant);

/** Checks whether this record is consistent
 * \param pRecord NDEF Record instance
 * \return TRUE if valid, FALSE otherwise
 */
gboolean ndef_record_validate(NdefRecord* pRecord);

/** Parse the NDEF message into list of records
 * \param data NDEF message buffer
 * \param dataLength NDEF message buffer's length
 * \return list of NDEF Record instances (1 per record)
 */
GList* ndef_message_parse(guint8* data, gsize dataLength); //Returns a list of NDEF records

/** Generate a NDEF message from list of records
 * \param pList list of NDEF Record instances
 * \param pData will return NDEF message buffer
 * \param dataLength will return NDEF message buffer's length
 */
void ndef_message_generate(GList* pList, guint8** pData, gsize* pDataLength);

#endif /* NDEF_H_ */

/**
 * @}
 * */
