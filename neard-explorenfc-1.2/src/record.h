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
 * \file record.h
 */
/** \defgroup RecordGp Record
 * The DBUS representation of a NDEF record
 *  @{
 */

#ifndef RECORD_H_
#define RECORD_H_

//DBUS-accessible methods
#include <glib.h>
#include <glib-object.h>

#include "generated-code.h"

struct record_container;
typedef struct record_container RecordContainer;

#define TYPE_RECORD   (record_get_type               ())
#define RECORD(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_RECORD, Record))
#define RECORD_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_RECORD, RecordClass))
#define RECORD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_RECORD, RecordClass))

#include "ndef.h"

/** Record
 *
 */
struct record
{
	GObject parentInstance; ///< GObject instance
	guint recordId; ///< Record ID
	NeardObjectSkeleton* pObjectSkeleton; ///< DBUS Object Skeleton
	NeardRecord* pNeardRecord; ///< DBUS record interface
	gchar* objectPath; ///< Object path
	RecordContainer* pRecordContainer; ///< Parent (tag or device)
};
typedef struct record Record; ///< Record

/** Record class
 *
 */
struct record_class
{
	GObjectClass parentClass; ///< Parent GObjectClass

	void (*record_register)(Record* pRecord, RecordContainer* pRecordContainer, NdefRecord* pNdefRecord, guint recordId); ///< Register method
	void (*record_unregister)(Record* pRecord); ///< Unregister method
};
typedef struct record_class RecordClass; ///< Record class

/** Create a new Record instance
 * \return new Record instance
 */
Record* record_new();

/** Register the record on the following parent (tag/device)
 * \param pRecord Record to register
 * \param pRecordContainer RecordContainer on which to register
 * \param pNdefRecord NDEF record to use
 * \param recordId id to give the adapter (object path will be /org/neard/nfc[adapterId]/(tag[tagId]/device[deviceId])/record[recordId])
 */
void record_register(Record* pRecord, RecordContainer* pRecordContainer, NdefRecord* pNdefRecord, guint recordId);

/** Unregister the record from parent
 * \param pRecord Record to unregister
 */
void record_unregister(Record* pRecord);

#endif /* RECORD_H_ */

/**
 * @}
 * */
