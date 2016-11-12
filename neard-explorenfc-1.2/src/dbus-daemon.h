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
 * \file dbus-daemon.h
 */
/** \defgroup DBusDaemonGp DBus Daemon
 * The core DBUS Daemon
 *  @{
 */

#ifndef DBUS_DAEMON_H_
#define DBUS_DAEMON_H_

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include "adapter.h"
#include "handover-agent.h"
#include "hal.h"

#include "generated-code.h"

struct ndef_record;
typedef struct ndef_record NdefRecord;

//Core D-Bus daemon

#define TYPE_DBUS_DAEMON   (dbus_daemon_get_type               ())
#define DBUS_DAEMON(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DBUS_DAEMON, DBusDaemon))
#define DBUS_DAEMON_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_DBUS_DAEMON, DBusDaemonClass))
#define DBUS_DAEMON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_DBUS_DAEMON, DBusDaemonClass))

/** DBus Daemon
 *
 */
struct dbus_daemon
{
    GObject parentInstance; ///< GObject instance

	guint ownerId; ///< Ownership ID for bus name
	GDBusConnection* pConnection; ///< DBus connection
	GDBusObjectManagerServer* pObjectManagerServer; ///< DBus Object manager

	gboolean constantPoll; ///< Constant poll parameter

	//Manager (at /)
	NeardObjectSkeleton* pManagerObjectSkeleton; ///< Manager (/): DBus Object Skeleton
	NeardManager* pNeardManager; ///< Manager (/): DBus manager interface

	//Agent Manager (at /org/neard/)
	NeardObjectSkeleton* pAgentManagerObjectSkeleton; ///< Agent manager (/org/neard/): DBUS Object Skeleton
	NeardAgentManager* pNeardAgentManager; ///< Agent manager (/org/neard/): DBus manager interface

	//Modules
	Adapter* pAdapter; ///< Reference Adapter
	GHashTable* pAgentTable; ///< Table of Handover agents

	//Pointer to hal
	hal_t* pHal; ///< Reference to HAL

	//Pointer to GMainLoop*
	GMainLoop* pMainLoop;
};
typedef struct dbus_daemon DBusDaemon; ///< DBus Daemon

/** DBus Daemon class
 *
 */
struct dbus_daemon_class
{
	GObjectClass parentClass; ///< Parent GObjectClass

	void (*dbus_daemon_start)(DBusDaemon* pDBusDaemon, hal_t* pHal); ///< Start method
};
typedef struct dbus_daemon_class DBusDaemonClass; ///< DBus Daemon class

/** Get DBus Daemon GType
 * \return type id
 */
GType dbus_daemon_get_type (void);

/** Create a new DBusDaemon instance
 * Try to get ownership of bus, set reference to HAL - if fails the instance will be destroyed
 * \param pHal hal_t instance
 * \param pMainLoop main loop on which this daemon is running
 * \return new DBusDaemon instance
 */
DBusDaemon* dbus_daemon_new(hal_t* pHal, GMainLoop* pMainLoop);

/** Set constant poll
 * Set whether polling should be restarted when a tag/device is lost
 * \param pDBusDaemon DBus Daemon instance
 * \param constantPoll TRUE to enable, FALSE to disable
 */
void dbus_daemon_set_constant_poll(DBusDaemon* pDBusDaemon, gboolean constantPoll);

/** Check NDEF record and pass it to agents if appropriate
 * \param pDBusDaemon DBus Daemon instance
 * \param pNdefRecord record to check
 * \return TRUE if the record was passed to (at least) one agent, FALSE otherwise
 */
gboolean dbus_daemon_check_ndef_record(DBusDaemon* pDBusDaemon, NdefRecord* pNdefRecord);

/** Callback from Handover Agent to advertise disconnection
 * \param pDBusDaemon DBus Daemon instance
 * \param pHandoverAgent Handover Agent which disconnected
 */
void dbus_daemon_handover_agent_disconnected(DBusDaemon* pDBusDaemon, HandoverAgent* pHandoverAgent);

#endif /* DBUS_DAEMON_H_ */

/**
 * @}
 * */
