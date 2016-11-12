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
 * \file tag.h
 */
/** \defgroup TagGp Tag
 * The DBUS representation of a NFC tag
 *  @{
 */

#ifndef TAG_H_
#define TAG_H_

//DBUS-accessible methods
#include <glib.h>

#include "generated-code.h"

#include "record-container.h"

struct adapter;
typedef struct adapter Adapter;

#define TYPE_TAG   (tag_get_type               ())
#define TAG(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TAG, Tag))
#define TAG_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_TAG, TagClass))
#define TAG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_TAG, TagClass))

/** Tag NFC Type
 *
 */
enum tag_nfc_type
{
	tag_nfc_type_1, ///< Type 1
	tag_nfc_type_2, ///< Type 2
	tag_nfc_type_3, ///< Type 3
	tag_nfc_type_4, ///< Type 4
	//Note: There is a tag_nfc_type_5 present in the neard doc...
};
typedef enum tag_nfc_type tag_nfc_type_t; ///< Tag NFC Type

/** Tag protocol
 *
 */
enum tag_protocol
{
	tag_protocol_felica, ///< Felica
	tag_protocol_mifare, ///< Mifare
	tag_protocol_jewel, ///< Jewel
	tag_protocol_iso_dep, ///< ISO-DEP
};
typedef enum tag_protocol tag_protocol_t; ///< Tag protocol

/** Tag descriptor
 *
 */
struct tag_descriptor
{
	tag_nfc_type_t type; ///< Tag NFC type
	tag_protocol_t protocol; ///< Tag protocol
	gboolean readOnly; ///< Tag is read only
};
typedef struct tag_descriptor tag_descriptor_t; ///< Tag descriptor

/** Tag
 *
 */
struct tag
{
	RecordContainer parentInstance; ///< RecordContainer instance

	tag_descriptor_t descriptor; ///< Descriptor
	guint tagId; ///< Tag ID
	NeardObjectSkeleton* pObjectSkeleton; ///< DBUS Object Skeleton
	NeardTag* pNeardTag; ///< DBUS tag interface

	GBytes* pRawNDEF; ///< Raw NDEF message on tag

	GHashTable* pRecordTable; ///< Table of records
};
typedef struct tag Tag;

/** Tag class
 *
 */
struct tag_class
{
	RecordContainerClass parentClass; ///< Parent RecordContainerClass

	void (*tag_register)(Tag* pTag, Adapter* pAdapter, guint tagId); ///< Register method
	void (*tag_unregister)(Tag* pTag); ///< Unregister method
};
typedef struct tag_class TagClass; ///< Tag class

/** Create a new Tag instance
 * \return new Tag instance
 */
Tag* tag_new();

/** Register the tag on the following Adapter
 * \param pTag Tag to register
 * \param pAdapter Adapter on which to register
 * \param tagId id to give the device (object path will be /org/neard/nfc[adapterId]/tag[tagId])
 */
void tag_register(Tag* pTag, Adapter* pAdapter, guint tagId);

/** Populate records
 * Populate records once NDEF message has been read
 * \param pTag Tag for which to populate records
 */
void tag_populate_records(Tag* pTag);

/** Unregister the tag from Adapter
 * \param pTag Tag to unregister
 */
void tag_unregister(Tag* pTag);

#endif /* TAG_H_ */

/**
 * @}
 * */
