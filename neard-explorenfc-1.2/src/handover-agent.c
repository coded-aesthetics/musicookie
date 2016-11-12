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
#include "handover-agent.h"
#include "dbus-daemon.h"
#include "dbus-parameters.h"
#include "record-container.h"
#include "ndef.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include "string.h"

#include "hal.h"

#define RECORD_MIME_WIFI "application/vnd.wfa.wsc"

//Local functions
static void handover_agent_class_init (HandoverAgentClass* pAdapterClass);
static void handover_agent_init(HandoverAgent* pHandoverAgent);
static void handover_agent_dispose(GObject* pGObject);
static void handover_agent_disconnect(HandoverAgent* pHandoverAgent);
static void handover_agent_connection_result(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void handover_agent_push_oob_result(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_dbus_connection_closed(GDBusConnection *connection, gboolean remote_peer_vanished, GError *error, gpointer user_data);
static void on_name_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data);

//GObject implementation
G_DEFINE_TYPE (HandoverAgent, handover_agent, G_TYPE_OBJECT)

void handover_agent_class_init (HandoverAgentClass* pHandoverAgentClass)
{
	GObjectClass* pGObjectClass = G_OBJECT_CLASS (pHandoverAgentClass);

	pGObjectClass->dispose = handover_agent_dispose;

	//Do not overload finalize
	//pGObjectClass->finalize = handover_agent_finalize;
}

void handover_agent_init(HandoverAgent* pHandoverAgent)
{
	pHandoverAgent->pDaemon = NULL;

	pHandoverAgent->type = handover_agent_type_bluetooth;
	pHandoverAgent->pNeardHandoverAgent = NULL;
	pHandoverAgent->sender = NULL;
	pHandoverAgent->objectPath = NULL;
	pHandoverAgent->nameWatcherHandle = 0;

	pHandoverAgent->status = handover_agent_idle;
}

void handover_agent_dispose(GObject* pGObject)
{
	HandoverAgent* pHandoverAgent = HANDOVER_AGENT(pGObject);

	if( pHandoverAgent->sender != NULL )
	{
		handover_agent_disconnect(pHandoverAgent);
	}

	G_OBJECT_CLASS (handover_agent_parent_class)->dispose(pGObject);
}

HandoverAgent* handover_agent_new()
{
	HandoverAgent* pHandoverAgent = g_object_new(TYPE_HANDOVER_AGENT, NULL);
	return pHandoverAgent;
}

void handover_agent_connect(HandoverAgent* pHandoverAgent, DBusDaemon* pDBusDaemon, const gchar* sender, const gchar* objectPath, handover_agent_type_t type)
{
	pHandoverAgent->pDaemon = pDBusDaemon;
	g_object_ref(pDBusDaemon);

	pHandoverAgent->type = type;

	pHandoverAgent->sender = g_strdup(sender);
	pHandoverAgent->objectPath = g_strdup(objectPath);
	g_info("Handover agent for %s at %s\r\n", (pHandoverAgent->type==handover_agent_type_bluetooth)?"Bluetooth":"WiFi", pHandoverAgent->objectPath);

	pHandoverAgent->status = handover_agent_connecting;

	pHandoverAgent->nameWatcherHandle = g_bus_watch_name(G_BUS_TYPE_SYSTEM, sender, G_BUS_NAME_WATCHER_FLAGS_NONE, NULL, on_name_vanished, pHandoverAgent, NULL );

	neard_handover_agent_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, sender, objectPath, NULL, handover_agent_connection_result, pHandoverAgent);
}

gboolean handover_agent_check_connection(HandoverAgent* pHandoverAgent)
{
	gchar* owner = NULL;

	g_object_get (pHandoverAgent->pNeardHandoverAgent,
					  "g-name-owner", &owner,
					  NULL);

	if( owner == NULL )
	{
		return FALSE;
	}

	g_debug("Name owned %s", owner);

	g_free(owner);

	GDBusConnection* pConnection = NULL;


	g_object_get (pHandoverAgent->pNeardHandoverAgent,
				  "g-connection", &pConnection,
				  NULL);

	gboolean closed = FALSE;
	g_object_get(pConnection, "closed",
				&closed, NULL);

	g_object_unref(pConnection);


	return !closed;
}

gboolean handover_agent_check_ndef_record(HandoverAgent* pHandoverAgent, NdefRecord* pNdefRecord)
{
	if( pHandoverAgent->status != handover_agent_connected )
	{
		return FALSE;
	}
	switch(pHandoverAgent->type)
	{
	case handover_agent_type_wifi:
		if( (pNdefRecord->type == ndef_record_type_mime) && !strcmp(pNdefRecord->mimeType, RECORD_MIME_WIFI) )
		{
			g_info("WiFi record detected\r\n");

			GVariantDict dict;
			g_variant_dict_init(&dict, NULL);

			GVariant* pWSCVariant = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, pNdefRecord->mimePayload, FALSE);
			g_variant_dict_insert_value(&dict, "WSC", pWSCVariant);

			GVariant* pVariant = g_variant_dict_end(&dict);

			//Call agent
			g_object_ref(pHandoverAgent); //Get temp ref
			neard_handover_agent_call_push_oob(pHandoverAgent->pNeardHandoverAgent, pVariant, NULL, handover_agent_push_oob_result, pHandoverAgent);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

gboolean handover_agent_match_params(HandoverAgent* pHandoverAgent, const gchar* sender, const gchar* objectPath)
{
	if(pHandoverAgent->sender == NULL)
	{
		//Agent is disconnected
		return FALSE;
	}
	return !strcmp(pHandoverAgent->sender, sender) && !strcmp(pHandoverAgent->objectPath, objectPath);
}

void handover_agent_disconnect(HandoverAgent* pHandoverAgent)
{
	if(pHandoverAgent->sender == NULL)
	{
		return; //Already cleaned-up
	}

	g_free(pHandoverAgent->sender);
	pHandoverAgent->sender = NULL;

	g_free(pHandoverAgent->objectPath);
	pHandoverAgent->objectPath = NULL;

	if(pHandoverAgent->pNeardHandoverAgent != NULL)
	{
		//g_clear_object(&pHandoverAgent->pNeardHandoverAgent);
		g_object_unref(pHandoverAgent->pNeardHandoverAgent); //Do not clear, there might be a call pending
	}

	pHandoverAgent->status = handover_agent_disconnected;

	//Advertise our disconnected status
	dbus_daemon_handover_agent_disconnected(pHandoverAgent->pDaemon, pHandoverAgent);

	//Clear ref to DBusDaemon
	g_clear_object(&pHandoverAgent->pDaemon);

	//Stop watching name
	g_bus_unwatch_name( pHandoverAgent->nameWatcherHandle );
	pHandoverAgent->nameWatcherHandle = 0;
}

void handover_agent_connection_result(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	HandoverAgent* pHandoverAgent = (HandoverAgent*) user_data;

	//Get the connection result
	GError* pError = NULL;
	pHandoverAgent->pNeardHandoverAgent = neard_handover_agent_proxy_new_for_bus_finish(res, &pError);
	if(pHandoverAgent->pNeardHandoverAgent != NULL)
	{
		g_debug("Connection to handover agent successful\r\n");
		pHandoverAgent->status = handover_agent_connected;

		//Register connection lost signal
		GDBusConnection* pConnection = NULL;
		g_object_get (pHandoverAgent->pNeardHandoverAgent,
		              "g-connection", &pConnection,
		              NULL);

		g_signal_connect(pConnection, "closed",
					G_CALLBACK (on_dbus_connection_closed), pHandoverAgent);

		g_object_unref(pConnection);
	}
	else
	{
		g_warning("Connection to handover agent failed (error %d): %s\r\n", pError->code, pError->message);
		handover_agent_disconnect(pHandoverAgent); //Cleanup
		g_error_free(pError);
	}
}

void handover_agent_push_oob_result(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	HandoverAgent* pHandoverAgent = (HandoverAgent*) user_data;

	//Get the connection result
	GError* pError = NULL;
	gboolean success = neard_handover_agent_call_push_oob_finish(pHandoverAgent->pNeardHandoverAgent, res, &pError);
	if(success)
	{
		g_debug("Push OOB successful\r\n");
	}
	else
	{
		g_warning("Push OOB failed (error %d): %s\r\n", pError->code, pError->message);
		g_error_free(pError);
	}
	g_object_unref(pHandoverAgent); //Loose temp ref
}

void on_dbus_connection_closed(GDBusConnection *connection, gboolean remote_peer_vanished, GError *error, gpointer user_data)
{
	HandoverAgent* pHandoverAgent = (HandoverAgent*) user_data;

	g_debug("Agent's DBUS connection closed");
	//Advertise disconnection if unexpected
	if(remote_peer_vanished)
	{
		g_warning("Agent vanished");
		handover_agent_disconnect(pHandoverAgent);
	}
}

void on_name_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	HandoverAgent* pHandoverAgent = (HandoverAgent*) user_data;

	g_debug("Agent is gone");

	//Advertise disconnection
	handover_agent_disconnect(pHandoverAgent);
}

