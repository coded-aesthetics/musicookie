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
 * \file adapter.h
 */
/** \defgroup AdapterGp Adapter
 * The DBUS representation of an adapter
 *  @{
 */

#ifndef ADAPTER_H_
#define ADAPTER_H_

//DBUS-accessible methods
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "generated-code.h"

struct dbus_daemon;
typedef struct dbus_daemon DBusDaemon;

#define TYPE_ADAPTER   (adapter_get_type               ())
#define ADAPTER(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ADAPTER, Adapter))
#define ADAPTER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_ADAPTER, AdapterClass))
#define ADAPTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_ADAPTER, AdapterClass))

/** Adapter
 *
 */
struct adapter
{
	GObject parentInstance; ///< GObject instance
	DBusDaemon* pDaemon; ///< Reference to containing DBusDaemon

	guint adapterId; ///< Adapter ID
	NeardObjectSkeleton* pObjectSkeleton; ///< DBUS Object Skeleton
	NeardAdapter* pNeardAdapter; ///< DBUS adapter interface
	gchar* objectPath; ///< Object path

	GHashTable* pTagTable; ///< Table of tags
	GHashTable* pDeviceTable; ///< Table of devices
};
typedef struct adapter Adapter; ///< Adapter

/** Adapter class
 *
 */
struct adapter_class
{
	GObjectClass parentClass; ///< Parent GObjectClass

	void (*adapter_register)(Adapter* pAdapter, DBusDaemon* pDBusDaemon, guint adapterId); ///< Register method
	void (*adapter_unregister)(Adapter* pAdapter); ///< Unregister method
};
typedef struct adapter_class AdapterClass; ///< Adapter class

/** Adapter mode
 * Mode in which the adapter is operating
 */
enum adapter_mode
{
	adapter_mode_initiator, ///< Initiator mode
	adapter_mode_target, ///< Target mode
	adapter_mode_dual, ///< Dual mode
	adapter_mode_idle ///< Idle mode
};
typedef enum adapter_mode adapter_mode_t; ///< Adapter mode

/** Create a new Adapter instance
 * \return new Adapter instance
 */
Adapter* adapter_new();

/** Register the adapter on the following DBusDaemon
 * \param pAdapter Adapter to register
 * \param pDBusDaemon DBusDaemon on which to register
 * \param adapterId id to give the adapter (object path will be /org/neard/nfc[adapterId])
 */
void adapter_register(Adapter* pAdapter, DBusDaemon* pDBusDaemon, guint adapterId);

/** Unregister the adapter from DBusDaemon
 * \param pAdapter Adapter to unregister
 */
void adapter_unregister(Adapter* pAdapter);

#endif /* ADAPTER_H_ */

/**
 * @}
 * */
