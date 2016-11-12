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
 * \file hal.h
 */
/** \defgroup HalGp HAL
 * Implementation of NFC routines using the NXPRdLib
 *  @{
 */

#ifndef HAL_H_
#define HAL_H_

#include <glib.h>
#include "tag.h"

typedef gpointer hal_t;

/** NFC Mode
 *
 */
enum nfc_mode
{
	nfc_mode_initiator, ///< Initiator mode
	nfc_mode_target, ///< Target mode
	nfc_mode_dual, ///< Dual mode
	nfc_mode_idle ///< Idle mode
};
typedef enum nfc_mode nfc_mode_t; ///< NFC Mode

/** \name Main functions
 */
///\{
/** Instantiate HAL
 * \return hal_t instance
 */
hal_t* hal_impl_new();

/** Initialize HAL
 * \param pHal hal_t instance
 * \param pGMainContext MainContext in which callbacks will be invoked
 * \return 0 on success, 1 on failure
 */
int hal_impl_init(hal_t* pHal, GMainContext* pGMainContext);

/** Free HAL
 * \param pHal hal_t instance to free
 */
void hal_impl_free(hal_t* pHal);
///\}


/** \name Callbacks
 */
///\{
/** On mode changed callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param mode new NFC mode
 */
typedef void (*hal_adapter_on_mode_changed_cb_t)(hal_t* pHal, GObject* pAdapterObject, nfc_mode_t mode);

//typedef void (*hal_adapter_on_powered_changed_cb_t)(hal_t* pHal, GObject* pAdapterObject, gboolean powered);

/** On polling changed callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param polling whether polling is active
 */
typedef void (*hal_adapter_on_polling_changed_cb_t)(hal_t* pHal, GObject* pAdapterObject, gboolean polling);

/** On tag detected callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param tagId id of new tag
 */
typedef void (*hal_adapter_on_tag_detected_cb_t)(hal_t* pHal, GObject* pAdapterObject, guint tagId);

/** On tag lost callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param tagId id of lost tag
 */
typedef void (*hal_adapter_on_tag_lost_cb_t)(hal_t* pHal, GObject* pAdapterObject, guint tagId);

/** On device detected callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param deviceId id of new device
 */
typedef void (*hal_adapter_on_device_detected_cb_t)(hal_t* pHal, GObject* pAdapterObject, guint deviceId);

/** On NDEF received from device callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param deviceId id of device
 */
typedef void (*hal_adapter_on_device_ndef_received_cb_t)(hal_t* pHal, GObject* pAdapterObject, guint deviceId);

/** On device lost callback
 * \param pHal hal_t instance
 * \param pAdapterObject GObject passed in hal_adapter_register()
 * \param deviceId id of lost device
 */
typedef void (*hal_adapter_on_device_lost_cb_t)(hal_t* pHal, GObject* pAdapterObject, guint deviceId);


/** Register callbacks
 * Callbacks will be called from pGMainContext passed in init()
 * \param pHal hal_t instance
 * \param pAdapterObject GObject instance
 * \param onModeChangedCb mode changed callback
 * \param onPollingChangedCb polling changed callback
 * \param onTagDetectedCb tag detected callback
 * \param onTagLostCb mode changed callback
 * \param onDeviceDetectedCb mode changed callback
 * \param onDeviceNDEFReceivedCb mode changed callback
 * \param onDeviceLostCb mode changed callback
 */
void hal_adapter_register(hal_t* pHal, GObject* pAdapterObject,
		hal_adapter_on_mode_changed_cb_t onModeChangedCb,
		hal_adapter_on_polling_changed_cb_t onPollingChangedCb,
		hal_adapter_on_tag_detected_cb_t onTagDetectedCb,
		hal_adapter_on_tag_lost_cb_t onTagLostCb,
		hal_adapter_on_device_detected_cb_t onDeviceDetectedCb,
		hal_adapter_on_device_ndef_received_cb_t onDeviceNDEFReceivedCb,
		hal_adapter_on_device_lost_cb_t onDeviceLostCb
		);
void hal_adapter_unregister(hal_t* pHal, GObject* pAdapterObject);
///\}

/** \name Adapters
 */
///\{
/** Start polling loop
 * \param pHal hal_t instance
 * \param mode polling mode
 */
void hal_adapter_polling_loop_start(hal_t* pHal, nfc_mode_t mode);

/** Stop polling loop
 * \param pHal hal_t instance
 */
void hal_adapter_polling_loop_stop(hal_t* pHal);

/** Get current mode
 * \param pHal hal_t instance
 * \return current mode
 */
nfc_mode_t hal_adapter_get_mode(hal_t* pHal);

/** Get whether polling is active
 * \param pHal hal_t instance
 * \return TRUE if polling is active, FALSE otherwise
 */
gboolean hal_adapter_is_polling(hal_t* pHal);
///\}


/** \name Tags
 */
///\{

/** Tag type
 *
 */
enum nfc_tag_type
{
	nfc_tag_type_1, ///<Type 1
	nfc_tag_type_2, ///<Type 2
	nfc_tag_type_3, ///<Type 3
	nfc_tag_type_4, ///<Type 4
};
typedef enum nfc_tag_type nfc_tag_type_t; ///< Tag type

/** Increase reference count for tag
 * \param pHal hal_t instance
 * \param tagId id of tag
 */
void hal_tag_ref(hal_t* pHal, guint tagId);

/** Decrease reference count for tag (and free if it reaches 0)
 * \param pHal hal_t instance
 * \param tagId id of tag
 */
void hal_tag_unref(hal_t* pHal, guint tagId);

/** Get tag type
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \return tag type
 */
nfc_tag_type_t hal_tag_get_type(hal_t* pHal, guint tagId);

/** Check if tag is connected
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \return TRUE if tag is connected, FALSE otherwise
 */
gboolean hal_tag_is_connected(hal_t* pHal, guint tagId);

/** Get NDEF message from tag
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \param pBuffer will return buffer (or NULL)
 * \param pBufferLength will return buffer's length
 */
void hal_tag_get_ndef(hal_t* pHal, guint tagId, guint8** pBuffer, gsize* pBufferLength);

/** Write NDEF message to tag
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \param buffer buffer to write
 * \param bufferLength buffer's length
 */
void hal_tag_write_ndef(hal_t* pHal, guint tagId, guint8* buffer, gsize bufferLength);

/** Check whether the tag can be written
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \return TRUE if tag is read-only, FALSE otherwise
 */
gboolean hal_tag_is_readonly(hal_t* pHal, guint tagId);

/** Check whether the tag is ISO14443A-compliant
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \return TRUE if tag is ISO14443A compliant, FALSE otherwise
 */
gboolean hal_tag_is_iso14443a(hal_t* pHal, guint tagId);

/** Get ISO14443A-specific parameters for tag
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \param atqa (2 bytes long)
 * \param sak sak (1 byte long)
 * \param uid buffer to store uid (max 10 bytes long)
 * \param pUidLength uid's length
 */
void hal_tag_get_iso14443a_params(hal_t* pHal, guint tagId, guint8* atqa, guint8* sak, guint8* uid, gsize* pUidLength);

/** Check whether the tag is Felica-compliant
 * \param pHal hal_t instance
 * \param tagId id of tag
 * \return TRUE if tag is Felica compliant, FALSE otherwise
 */
gboolean hal_tag_is_felica(hal_t* pHal, guint tagId);

/** Get Felica-specific parameters for tag
 * \param pHal hal_t instance
 * \param manufacturer manufacturer information field (2 bytes long)
 * \param cid card ID (6 bytes long)
 * \param ic card IC code (2 bytes long)
 * \param maxRespTimes card maximum response times (6 bytes long)
 */
void hal_tag_get_felica_params(hal_t* pHal, guint tagId, guint8* manufacturer, guint8* cid, guint8* ic, guint8* maxRespTimes);

///\}

/** \name Devices
 */
///\{
/** Device type
 *
 */
enum nfc_device_type
{
	nfc_device_nfc_dep_f, ///< NFC-DEP (Felica) device
	nfc_device_nfc_dep_a, ///< NFC-DEP (ISO A) device
};
typedef enum nfc_device_type nfc_device_type_t; ///< Device type

/** Increase reference count for device
 * \param pHal hal_t instance
 * \param deviceId id of device
 */
void hal_device_ref(hal_t* pHal, guint deviceId);

/** Decrease reference count for device (and free if it reaches 0)
 * \param pHal hal_t instance
 * \param deviceId id of device
 */
void hal_device_unref(hal_t* pHal, guint deviceId);

/** Check if device is connected
 * \param pHal hal_t instance
 * \param deviceId id of device
 * \return TRUE if device is connected, FALSE otherwise
 */
gboolean hal_device_is_connected(hal_t* pHal, guint deviceId);

/** Get NDEF message sent by peer
 * \param pHal hal_t instance
 * \param deviceId id of device
 * \param pBuffer will return buffer (or NULL)
 * \param pBufferLength will return buffer's length
 */
void hal_device_get_ndef(hal_t* pHal, guint deviceId, guint8** pBuffer, gsize* pBufferLength);

/** Push NDEF message to peer
 * \param pHal hal_t instance
 * \param deviceId id of device
 * \param buffer buffer to write
 * \param bufferLength buffer's length
 */
void hal_device_push_ndef(hal_t* pHal, guint deviceId, guint8* buffer, gsize bufferLength);
///\}

#endif /* HAL_H_ */

/**
 * @}
 * */
