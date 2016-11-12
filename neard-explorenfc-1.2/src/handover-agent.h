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
 * \file handover-agent.h
 */
/** \defgroup HandoverAgentGp Handover Agent
 * And handover agent can be registered to handle WiFi and Bluetooth handover data
 *  @{
 */

#ifndef HANDOVER_AGENT_H_
#define HANDOVER_AGENT_H_

//DBUS-accessible methods
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "generated-code.h"

struct dbus_daemon;
typedef struct dbus_daemon DBusDaemon;

struct ndef_record;
typedef struct ndef_record NdefRecord;

#define TYPE_HANDOVER_AGENT   (handover_agent_get_type               ())
#define HANDOVER_AGENT(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_HANDOVER_AGENT, HandoverAgent))
#define HANDOVER_AGENT_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_HANDOVER_AGENT, HandoverAgentClass))
#define HANDOVER_AGENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_HANDOVER_AGENT, HandoverAgentClass))

/** Handover Agent type
 *
 */
enum handover_agent_type
{
	handover_agent_type_bluetooth, ///< Bluetooth
	handover_agent_type_wifi ///< WiFi
};
typedef enum handover_agent_type handover_agent_type_t; ///< Handover agent type

/** Handover Agent
 *
 */
struct handover_agent
{
	GObject parentInstance; ///< GObject instance
	DBusDaemon* pDaemon; ///< Reference to containing DBusDaemon

	handover_agent_type_t type; ///< Agent type
	NeardHandoverAgent* pNeardHandoverAgent; ///< DBUS handover agent interface
	gchar* sender; ///< Sender's DBUS namespace
	gchar* objectPath; ///< Sender's DBUS object path
	guint nameWatcherHandle; ///< Name watcher handle
	enum
	{
		handover_agent_idle, ///< Idle
		handover_agent_connecting, ///< Connecting
		handover_agent_connected, ///< Connected
		handover_agent_disconnected ///< Disconnected
	} status; ///< status
};
typedef struct handover_agent HandoverAgent; ///< Handover Agent

/** Handover Agent class
 *
 */
struct handover_agent_class
{
	GObjectClass parentClass; ///< Parent GObjectClass
};
typedef struct adapter_class HandoverAgentClass; ///< Handover Agent class

/** Create a new Handover Agent instance
 * \return new Handover Agent instance
 */
HandoverAgent* handover_agent_new();

/** Connect Handover Agent
 * Connect to the specified DBUS server
 * \param pHandoverAgent Handover Agent instance
 * \param pDBusDaemon pDBusDaemon owning this agent
 * \param sender Sender's DBUS namespace
 * \param objectPath Sender's DBUS object path
 * \param type Handover agent type
 */
void handover_agent_connect(HandoverAgent* pHandoverAgent, DBusDaemon* pDBusDaemon, const gchar* sender, const gchar* objectPath, handover_agent_type_t type);

/** Check Handover Agent connection
 * Check if the Handover Agent is still there
 * \param pHandoverAgent Handover Agent instance
 * \return whether the agent is still connected
 */
gboolean handover_agent_check_connection(HandoverAgent* pHandoverAgent);

/** Check NDEF record
 * Pass the record to the remote agent if its type match
 * \param pHandoverAgent Handover Agent instance
 * \param pNdefRecord the NDEF record
 * \return TRUE if the record was sent to the remote agent, FALSE otherwise
 */
gboolean handover_agent_check_ndef_record(HandoverAgent* pHandoverAgent, NdefRecord* pNdefRecord);

/** Check parameters
 * \param pHandoverAgent Handover Agent instance
 * \param sender Sender's DBUS namespace
 * \param objectPath Sender's DBUS object path
 * \return TRUE if sender and objectPath parameters match with this agent's, FALSE otherwise
 */
gboolean handover_agent_match_params(HandoverAgent* pHandoverAgent, const gchar* sender, const gchar* objectPath);

#endif /* HANDOVER_AGENT_H_ */

/**
 * @}
 * */
