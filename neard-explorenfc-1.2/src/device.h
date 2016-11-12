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
 * \file device.h
 */
/** \defgroup DeviceGp Device
 * The DBUS representation of a P2P device supporting SNEP
 *  @{
 */

#ifndef DEVICE_H_
#define DEVICE_H_

//DBUS-accessible methods
#include <glib.h>

#include "generated-code.h"

#include "record-container.h"

struct adapter;
typedef struct adapter Adapter;

#define TYPE_DEVICE   (device_get_type               ())
#define DEVICE(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DEVICE, Device))
#define DEVICE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_DEVICE, DeviceClass))
#define DEVICE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_DEVICE, DeviceClass))

struct device_descriptor
{ };
typedef struct device_descriptor device_descriptor_t;

/** Device
 *
 */
struct device
{
	RecordContainer parentInstance; ///< RecordContainer instance

	device_descriptor_t descriptor; ///< Descriptor
	guint deviceId; ///< Device ID
	NeardObjectSkeleton* pObjectSkeleton; ///< DBUS Object Skeleton
	NeardDevice* pNeardDevice;///< DBUS device interface

	GBytes* pRawNDEF; ///< Raw NDEF message sent by peer

	GHashTable* pRecordTable; ///< Table of records
};
typedef struct device Device;///< Device

/** Device class
 *
 */
struct device_class
{
	RecordContainerClass parentClass; ///< Parent RecordContainerClass

	void (*device_register)(Device* pDevice, Adapter* pAdapter, guint deviceId); ///< Register method
	void (*device_unregister)(Device* pDevice); ///< Unregister method
};
typedef struct device_class DeviceClass; ///< Device class

/** Create a new Device instance
 * \return new Device instance
 */
Device* device_new();

/** Register the device on the following Adapter
 * \param pDevice Device to register
 * \param pAdapter Adapter on which to register
 * \param deviceId id to give the device (object path will be /org/neard/nfc[adapterId]/device[deviceId])
 */
void device_register(Device* pDevice, Adapter* pAdapter, guint deviceId);

/** Populate records
 * Populate records once NDEF message has been received
 * \param pDevice Device for which to populate records
 */
void device_populate_records(Device* pDevice);

/** Unregister the device from Adapter
 * \param pDevice Device to unregister
 */
void device_unregister(Device* pDevice);

#endif /* DEVICE_H_ */

/**
 * @}
 * */
