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

#include "dbus-daemon.h"
#include "adapter.h"
#include "dbus-parameters.h"
#include "ndef.h"
#include "handover-agent.h"

#include <glib.h>
#include <gio/gio.h>
#include "string.h"

#define ADAPTER_ID 0 //For now, only one adapter

//Local functions prototypes
static void dbus_daemon_class_init (DBusDaemonClass* pDBusDaemonClass);
static void dbus_daemon_init(DBusDaemon* pDBusDaemon);
static void dbus_daemon_dispose(GObject* pGObject);
static void dbus_daemon_start(DBusDaemon* pDBusDaemon, hal_t* pHal);

static void on_bus_acquired (GDBusConnection* pConnection, const gchar* name, gpointer user_data);
static void on_name_acquired (GDBusConnection* pConnection, const gchar* name, gpointer user_data);
static void on_name_lost (GDBusConnection* pConnection, const gchar* name, gpointer user_data);

//GObject implementation
G_DEFINE_TYPE(DBusDaemon, dbus_daemon, G_TYPE_OBJECT)

void dbus_daemon_class_init (DBusDaemonClass* pDBusDaemonClass)
{
	pDBusDaemonClass->dbus_daemon_start = dbus_daemon_start;

	GObjectClass* pGObjectClass = G_OBJECT_CLASS (pDBusDaemonClass);

	pGObjectClass->dispose = dbus_daemon_dispose;

	//Do not overload finalize
	//pGObjectClass->finalize = dbus_daemon_finalize;
}

void dbus_daemon_init(DBusDaemon* pDBusDaemon)
{
	pDBusDaemon->pHal = NULL;
	pDBusDaemon->pObjectManagerServer = NULL;

	pDBusDaemon->constantPoll = FALSE;

	pDBusDaemon->pNeardManager = NULL;
	pDBusDaemon->pManagerObjectSkeleton = NULL;

	pDBusDaemon->pNeardAgentManager = NULL;
	pDBusDaemon->pAgentManagerObjectSkeleton = NULL;

	pDBusDaemon->ownerId = 0;
	pDBusDaemon->pAdapter = NULL;
	pDBusDaemon->pAgentTable = g_hash_table_new(g_direct_hash, g_direct_equal);
	pDBusDaemon->pConnection = NULL;

	pDBusDaemon->pMainLoop = NULL;
}

void dbus_daemon_dispose(GObject* pGObject)
{
	DBusDaemon* pDBusDaemon = DBUS_DAEMON(pGObject);

	if(pDBusDaemon->ownerId != 0) //own name never returns 0
	{
		g_bus_unown_name(pDBusDaemon->ownerId);
		pDBusDaemon->ownerId = 0;
	}
	g_clear_object(&pDBusDaemon->pObjectManagerServer);
	g_clear_object(&pDBusDaemon->pConnection);
	g_clear_object(&pDBusDaemon->pAdapter);
	g_hash_table_destroy(pDBusDaemon->pAgentTable);
	g_clear_object(&pDBusDaemon->pNeardManager);
	g_clear_object(&pDBusDaemon->pManagerObjectSkeleton);
	g_clear_object(&pDBusDaemon->pNeardAgentManager);
	g_clear_object(&pDBusDaemon->pAgentManagerObjectSkeleton);
	g_main_loop_unref(pDBusDaemon->pMainLoop);
	pDBusDaemon->pMainLoop = NULL;
	G_OBJECT_CLASS (dbus_daemon_parent_class)->dispose(pGObject);
}

void dbus_daemon_handover_agent_disconnected(DBusDaemon* pDBusDaemon, HandoverAgent* pHandoverAgent)
{
	handover_agent_type_t handoverAgentType = pHandoverAgent->type;

	HandoverAgent* pHandoverAgentInTable = g_hash_table_lookup(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType));
	if( (pHandoverAgentInTable == NULL) || (pHandoverAgentInTable != pHandoverAgent) )
	{
		g_debug("Agent already removed");
		return;
	}

	//Remove from table
	g_hash_table_remove(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType));

	//Loose the reference held by us
	g_object_unref(pHandoverAgent);
}

//DBUS commands handlers
static gboolean on_register_handover_agent (NeardManager *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
				const gchar *objectPath, const gchar* type, gpointer pUserData);
static gboolean on_unregister_handover_agent (NeardManager *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
				const gchar *objectPath, const gchar* type, gpointer pUserData);

//Constructor shortcut
DBusDaemon* dbus_daemon_new(hal_t* pHal, GMainLoop* pMainLoop)
{
	DBusDaemon* pDBusDaemon = g_object_new(TYPE_DBUS_DAEMON, NULL);
	pDBusDaemon->pMainLoop = pMainLoop;
	g_main_loop_ref(pDBusDaemon->pMainLoop);
	dbus_daemon_start(pDBusDaemon, pHal);
	return pDBusDaemon;
}

/* Kludge */

struct _GDBusObjectManagerServerPrivate
{
  GMutex lock;
  GDBusConnection *connection;
  gchar *object_path;
  gchar *object_path_ending_in_slash;
  GHashTable *map_object_path_to_data;
  guint manager_reg_id;
};

//Methods
void dbus_daemon_start(DBusDaemon* pDBusDaemon, hal_t* pHal)
{
	pDBusDaemon->pHal = pHal;

	pDBusDaemon->pObjectManagerServer = g_dbus_object_manager_server_new(DBUS_ROOT_OBJECT_PATH);

	g_mutex_lock (&pDBusDaemon->pObjectManagerServer->priv->lock);
	g_free(pDBusDaemon->pObjectManagerServer->priv->object_path_ending_in_slash);
	pDBusDaemon->pObjectManagerServer->priv->object_path_ending_in_slash = g_strdup_printf(DBUS_ROOT_OBJECT_PATH);
    g_mutex_unlock (&pDBusDaemon->pObjectManagerServer->priv->lock);

	pDBusDaemon->ownerId = g_bus_own_name(
			G_BUS_TYPE_SYSTEM,
			DBUS_BUS_NAME,
			G_BUS_NAME_OWNER_FLAGS_NONE,
			on_bus_acquired,
			on_name_acquired,
			on_name_lost,
			pDBusDaemon,
			NULL
			);
}

gboolean dbus_daemon_check_ndef_record(DBusDaemon* pDBusDaemon, NdefRecord* pNdefRecord)
{
	gboolean handled = FALSE;

	GHashTableIter iter;
	handover_agent_type_t* pHandoverAgentType;
	HandoverAgent* pHandoverAgent;

	g_hash_table_iter_init (&iter, pDBusDaemon->pAgentTable);
	while (g_hash_table_iter_next(&iter, (gpointer*)&pHandoverAgentType, (gpointer*)&pHandoverAgent))
	{
		handled |= handover_agent_check_ndef_record(pHandoverAgent, pNdefRecord);
	}

	return handled;
}

void dbus_daemon_set_constant_poll(DBusDaemon* pDBusDaemon, gboolean constantPoll)
{
	pDBusDaemon->constantPoll = constantPoll;
}

//Local functions
void on_bus_acquired (GDBusConnection* pConnection, const gchar* name, gpointer user_data)
{
	DBusDaemon* pDBusDaemon = (DBusDaemon*)user_data;

	//Save connection details
	g_object_ref(pConnection);
	pDBusDaemon->pConnection = pConnection;

	//Manager
	pDBusDaemon->pManagerObjectSkeleton = neard_object_skeleton_new(DBUS_ROOT_OBJECT_PATH);
	pDBusDaemon->pNeardManager = neard_manager_skeleton_new();
	neard_object_skeleton_set_manager(pDBusDaemon->pManagerObjectSkeleton, pDBusDaemon->pNeardManager);

	//Agent Manager
	pDBusDaemon->pAgentManagerObjectSkeleton = neard_object_skeleton_new(DBUS_AGENT_MANAGER_OBJECT_PATH);
	pDBusDaemon->pNeardAgentManager = neard_agent_manager_skeleton_new();
	neard_object_skeleton_set_agent_manager(pDBusDaemon->pAgentManagerObjectSkeleton, pDBusDaemon->pNeardAgentManager);

	//Register objects
	pDBusDaemon->pAdapter = adapter_new();
	adapter_register(pDBusDaemon->pAdapter, pDBusDaemon, ADAPTER_ID);

	//Connect signals
	g_signal_connect(pDBusDaemon->pNeardManager, "handle-register-handover-agent",
			G_CALLBACK (on_register_handover_agent), pDBusDaemon);
	g_signal_connect(pDBusDaemon->pNeardManager, "handle-unregister-handover-agent",
				G_CALLBACK (on_unregister_handover_agent), pDBusDaemon);

	g_signal_connect(pDBusDaemon->pNeardAgentManager, "handle-register-handover-agent",
			G_CALLBACK (on_register_handover_agent), pDBusDaemon);
	g_signal_connect(pDBusDaemon->pNeardAgentManager, "handle-unregister-handover-agent",
				G_CALLBACK (on_unregister_handover_agent), pDBusDaemon);

	//Set properties
	const char* adapters[] = { g_strdup(pDBusDaemon->pAdapter->objectPath), NULL };
	neard_manager_set_adapters(pDBusDaemon->pNeardManager, adapters);

	//Export objects
	g_dbus_object_manager_server_export( pDBusDaemon->pObjectManagerServer, G_DBUS_OBJECT_SKELETON(pDBusDaemon->pManagerObjectSkeleton) );
	g_dbus_object_manager_server_export( pDBusDaemon->pObjectManagerServer, G_DBUS_OBJECT_SKELETON(pDBusDaemon->pAgentManagerObjectSkeleton) );
	g_dbus_object_manager_server_set_connection(pDBusDaemon->pObjectManagerServer, pConnection);
}

void on_name_acquired (GDBusConnection* pConnection, const gchar* name, gpointer user_data)
{
	g_debug("Bus %s ready", name);
}

void on_name_lost (GDBusConnection* pConnection, const gchar* name, gpointer user_data)
{
	DBusDaemon* pDBusDaemon = (DBusDaemon*)user_data;

	//If pConnection == NULL it means we could not obtain the name, otherwise we lost an existing connection
	if(pConnection != NULL)
	{
		//Unregister objects
		adapter_unregister(pDBusDaemon->pAdapter);

		//Delete adapter
		g_clear_object(&pDBusDaemon->pAdapter);

		//Delete handover agents (if present)
		GHashTableIter iter;
		handover_agent_type_t* pHandoverAgentType;
		HandoverAgent* pHandoverAgent;

		g_hash_table_iter_init (&iter, pDBusDaemon->pAgentTable);
		while (g_hash_table_iter_next(&iter, (gpointer*)&pHandoverAgentType, (gpointer*)&pHandoverAgent))
		{
			//Remove from table
			g_hash_table_iter_remove(&iter);

			//Free tag
			g_object_unref(pHandoverAgent);
		}

		//Stop exporting objects
		g_dbus_object_manager_server_unexport( pDBusDaemon->pObjectManagerServer, DBUS_ROOT_OBJECT_PATH );
		g_dbus_object_manager_server_unexport( pDBusDaemon->pObjectManagerServer, DBUS_AGENT_MANAGER_OBJECT_PATH );
		g_dbus_object_manager_server_set_connection(pDBusDaemon->pObjectManagerServer, NULL);

		g_clear_object(&pDBusDaemon->pNeardManager);
		g_clear_object(&pDBusDaemon->pManagerObjectSkeleton);

		g_clear_object(&pDBusDaemon->pNeardAgentManager);
		g_clear_object(&pDBusDaemon->pAgentManagerObjectSkeleton);

		//Lose connection details
		g_clear_object(&pDBusDaemon->pConnection);
	}
	else
	{
		//Exit program?
	}
	g_debug("Could not obtain bus %s\r\n", name);

	//Clear ourselves
	g_main_loop_quit(pDBusDaemon->pMainLoop);
}

gboolean on_register_handover_agent (NeardManager *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
				const gchar *objectPath, const gchar* type, gpointer pUserData)
{
	DBusDaemon* pDBusDaemon = DBUS_DAEMON(pUserData);

	g_info("Agent (%s) at %s", type, objectPath);

	const gchar* sender = g_dbus_method_invocation_get_sender(pInvocation);

	handover_agent_type_t handoverAgentType;
	if(!strcmp(type, "wifi"))
	{
		handoverAgentType = handover_agent_type_wifi;
	}
	else if(!strcmp(type, "bluetooth"))
	{
		handoverAgentType = handover_agent_type_bluetooth;
	}
	else
	{
		g_warning("Unknown agent type: %s", type);
		g_dbus_method_invocation_return_dbus_error(pInvocation, "org.neard.Error.InvalidArguments", "Unknown agent type");
		return TRUE;
	}

	//See if an agent of this type is already registered
	if(g_hash_table_contains(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType)))
	{
		//Check that is still around
		HandoverAgent* pFormerHandoverAgent = g_hash_table_lookup(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType));
		if( !handover_agent_check_connection(pFormerHandoverAgent) )
		{
			g_hash_table_remove(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType));
			g_object_unref(pFormerHandoverAgent);
		}
		else
		{
			g_warning("Agent already registered");
			g_dbus_method_invocation_return_dbus_error(pInvocation, "org.neard.Error.AlreadyExists", "Agent already registered");
			return TRUE;
		}
	}

	HandoverAgent* pHandoverAgent = handover_agent_new();
	handover_agent_connect(pHandoverAgent, pDBusDaemon, sender, objectPath, handoverAgentType);

	//Insert in table
	g_hash_table_insert(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType), pHandoverAgent);

	neard_manager_complete_register_handover_agent(pInterfaceSkeleton, pInvocation);

	return TRUE;
}

gboolean on_unregister_handover_agent (NeardManager *pInterfaceSkeleton, GDBusMethodInvocation *pInvocation,
				const gchar *objectPath, const gchar* type, gpointer pUserData)
{
	DBusDaemon* pDBusDaemon = DBUS_DAEMON(pUserData);

	g_info("Agent lost at %s", objectPath);

	const gchar* sender = g_dbus_method_invocation_get_sender(pInvocation);

	handover_agent_type_t handoverAgentType;
	if(!strcmp(type, "wifi"))
	{
		handoverAgentType = handover_agent_type_wifi;
	}
	else if(!strcmp(type, "bluetooth"))
	{
		handoverAgentType = handover_agent_type_bluetooth;
	}
	else
	{
		g_warning("Unknown agent type: %s", type);
		g_dbus_method_invocation_return_dbus_error(pInvocation, "org.neard.Error.InvalidArguments", "Unknown agent type");
		return TRUE;
	}

	HandoverAgent* pHandoverAgent = g_hash_table_lookup(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType));
	if( (pHandoverAgent == NULL) || !handover_agent_match_params(pHandoverAgent, sender, objectPath) )
	{
		g_warning("Invalid agent");
		g_dbus_method_invocation_return_dbus_error(pInvocation, "org.neard.Error.InvalidArguments", "Invalid agent");
		return TRUE;
	}

	g_hash_table_remove(pDBusDaemon->pAgentTable, GUINT_TO_POINTER(handoverAgentType));
	g_object_unref(pHandoverAgent);
	neard_manager_complete_unregister_handover_agent(pInterfaceSkeleton, pInvocation);

	return TRUE;
}
