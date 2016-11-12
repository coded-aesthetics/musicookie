/*
**         Copyright (c), NXP Semiconductors Gratkorn / Austria
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
/*
 * \file basic.c
 */

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>
#include <glib/gprintf.h>
#include <string.h>
#include <signal.h>

#include "neardal.h"

#include "wifi-connect-dbus-gen.h"
#include "wifi-connect-wsc-parser.h"

#define WPA_SUPPLICANT_NAMESPACE	"fi.w1.wpa_supplicant1"
#define WPA_SUPPLICANT_OBJECT_PATH  "/fi/w1/wpa_supplicant1"

struct params
{
	gboolean debug;
	gboolean keepPolling;
	gchar* adapterObjectPath;
	gchar* interface;
	gchar* interfaceObjectPath;
	gchar* networkObjectPath;
	GMainLoop* pMainLoop;
	GDBusConnection* pConnection;
	WPASupplicant* pWPASupplicant;
	WPASupplicantInterfaze* pWPASupplicantInterface;
	gint returnCode;
	gboolean agentRegistered;
	gboolean connected;
	GError* pError;
};
typedef struct params params_t;

static gint start_stop_polling(params_t* pParams, gboolean start)
{
	//List current adapters
	errorCode_t	err;

	int adaptersCount = 0;
	char** adapterNamesArray = NULL;
	err = neardal_get_adapters(&adapterNamesArray, &adaptersCount);
	if( err == NEARDAL_ERROR_NO_ADAPTER )
	{
		g_printf("No adapter found\r\n");
		return 1;
	}
	else if(err != NEARDAL_SUCCESS)
	{
		g_warning("Error %d when listing adapters (%s)\r\n", err, neardal_error_get_text(err));
		return 1;
	}

	gboolean adapterFound = FALSE;

	for(int i = 0; i < adaptersCount; i++)
	{
		if( pParams->adapterObjectPath != NULL )
		{
			if(strcmp(pParams->adapterObjectPath, adapterNamesArray[i]) != 0)
			{
				continue;
			}
		}

		//Reconfigure adapters if needed
		neardal_adapter* pAdapter;
		err = neardal_get_adapter_properties(adapterNamesArray[i], &pAdapter);
		if(err != NEARDAL_SUCCESS)
		{
			g_warning("Error %d when accessing adapter %s (%s)\r\n", err, adapterNamesArray[i], neardal_error_get_text(err));
			continue;
		}

		if( !pAdapter->powered )
		{
			g_info("Powering adapter %s\r\n", pAdapter->name);
			err = neardal_set_adapter_property(pAdapter->name, NEARD_ADP_PROP_POWERED, GUINT_TO_POINTER(1));
			if(err != NEARDAL_SUCCESS)
			{
				g_warning("Error %d when trying to power adapter %s (%s)\r\n", err, pAdapter->name, neardal_error_get_text(err));
			}
		}

		if( start && (pAdapter->polling == 0) )
		{
			//Start polling
			g_info("Starting polling for adapter %s\r\n", pAdapter->name);
			err = neardal_start_poll(pAdapter->name); //Mode NEARD_ADP_MODE_INITIATOR
			if(err != NEARDAL_SUCCESS)
			{
				g_warning("Error %d when trying to start polling on adapter %s (%s)\r\n", err, pAdapter->name, neardal_error_get_text(err));
			}
		}
		else if( !start && (pAdapter->polling != 0) )
		{
			//Stop polling
			g_info("Stopping polling for adapter %s\r\n", pAdapter->name);
			err = neardal_stop_poll(pAdapter->name);
			if(err != NEARDAL_SUCCESS)
			{
				g_warning("Error %d when trying to stop polling on adapter %s (%s)\r\n", err, pAdapter->name, neardal_error_get_text(err));
			}
		}

		neardal_free_adapter(pAdapter);

		adapterFound = TRUE;
		if( pParams->adapterObjectPath != NULL )
		{
			break;
		}
	}
	neardal_free_array(&adapterNamesArray);

	if(!adapterFound)
	{
		g_printf("Adapter not found\r\n");
		return 1;
	}

	return 0; //OK
}

static void handover_req_wifi_agent_cb(unsigned char *blobWSC, unsigned int blobSize,
		unsigned char ** oobData, unsigned int * oobDataSize, freeFunc *freeF, void *pUserData)
{
	//Not implemented for now
	*oobData = NULL;
	*oobDataSize = 0;
	*freeF = NULL;
}

static void handover_push_wifi_agent_cb (unsigned char *blobWSC, unsigned int blobSize, void *pUserData)
{
	params_t* pParams = (params_t*) pUserData;

	g_debug("Handover push");

	//Reset connection status
	pParams->connected = FALSE;

	//Parse WSC blob
	wsc_data_t* pWscData = NULL;
	gint ret = wsc_blob_parse(blobWSC, blobSize, &pWscData);
	if(ret)
	{
		g_error("Could not parse WSC blob");
		return;
	}

	wsc_data_print(pWscData);

	//Try to create new interface
	GVariantDict dict;
	g_variant_dict_init(&dict, NULL);

	g_debug("wpa_supplicant configuration:");

	if( pWscData->ssid != NULL )
	{
		g_variant_dict_insert(&dict, "ssid", "s", pWscData->ssid);
		g_debug("ssid: %s", pWscData->ssid);
	}

	//No field in WPA supplicant
	/*
	const guint8 empty_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	if( (pWscData->mac != NULL) && ( memcmp(pWscData->mac, empty_mac, 6) != 0 ) )
	{
		gchar* mac = g_strdup_printf("%02X:%02X:%02X:%02X:%02X:%02X",
				pWscData->mac[0],
				pWscData->mac[1],
				pWscData->mac[2],
				pWscData->mac[3],
				pWscData->mac[4],
				pWscData->mac[5]
				);
		g_variant_dict_insert(&dict, "mac", "s", mac);
		g_debug("mac: %s", mac);
		g_free(mac);
	}
	*/

	const gchar* authAlg;
	const gchar* keyMgmt;
	const gchar* proto;
	switch( pWscData->auth )
	{
	case wsc_auth_open:
	default:
		proto = NULL;
		authAlg = "OPEN";
		keyMgmt = "NONE";
		break;
	case wsc_auth_wpa_psk:
		proto = "WPA";
		authAlg = "OPEN";
		keyMgmt = "WPA-PSK";
		break;
	case wsc_auth_shared:
		proto = NULL;
		authAlg = "SHARED";
		keyMgmt = "NONE";
		break;
	case wsc_auth_wpa_eap:
		proto = "WPA";
		authAlg = "OPEN";
		keyMgmt = "WPA-EAP";
		break;
	case wsc_auth_wpa2_eap:
		proto = "WPA2";
		authAlg = "OPEN";
		keyMgmt = "WPA-EAP";
		break;
	case wsc_auth_wpa2_psk:
		proto = "WPA2";
		authAlg = "OPEN";
		keyMgmt = "WPA-PSK";
		break;
	case wsc_auth_wpa_wpa2_psk:
		proto = "WPA WPA2";
		authAlg = "OPEN";
		keyMgmt = "WPA-PSK";
		break;
	case wsc_auth_wpa_none:
		proto = "WPA";
		authAlg = "OPEN";
		keyMgmt = "WPA-NONE";
		break;
	}

	g_variant_dict_insert(&dict, "auth_alg", "s", authAlg);
	g_debug("auth_alg: %s", authAlg);


	if(!strcmp(keyMgmt, "NONE"))
	{
		g_variant_dict_insert(&dict, "key_mgmt", "s", keyMgmt);
		g_debug("key_mgmt: %s", keyMgmt);
	}

	/*
	if(proto != NULL)
	{
		g_variant_dict_insert(&dict, "proto", "s", proto);
		g_debug("proto: %s", proto);
	}
	*/

	const gchar* enc;
	switch( pWscData->enc )
	{
	case wsc_enc_none:
	default:
	case wsc_enc_wep:
		enc = NULL;
		break;
	case wsc_enc_tkip:
		enc = "TKIP";
		break;
	case wsc_enc_aes:
		enc = "CCMP";
		break;
	case wsc_enc_tkip_aes:
		enc = "TKIP CCMP";
		break;
	}

	//g_variant_dict_insert(&dict, "group", "s", enc);
	if(enc != NULL)
	{
		g_variant_dict_insert(&dict, "pairwise", "s", enc);

		//g_debug("group: %s", enc);
		g_debug("pairwise: %s", enc);
	}

	if( pWscData->key != NULL )
	{
		//gchar* key = g_strdup_printf("\"%s\"", pWscData->key);
		if( pWscData->auth == wsc_auth_shared )
		{
			g_variant_dict_insert(&dict, "wep_key0", "s", pWscData->key);
			g_debug("wep_key0: %s", pWscData->key);
		}
		else
		{
			g_variant_dict_insert(&dict, "psk", "s", pWscData->key);
			g_debug("psk: %s", pWscData->key);
		}
		//g_free(key);
	}

	wsc_data_free(pWscData);

	GVariant* pVariant = g_variant_dict_end(&dict);

	//Remove previous network if existing
	if( pParams->networkObjectPath != NULL )
	{
		wpasupplicant_interfaze_call_remove_network_sync(pParams->pWPASupplicantInterface, pParams->networkObjectPath, NULL, NULL);
		g_free(pParams->networkObjectPath);
		pParams->networkObjectPath = NULL;
	}

	//Setup network in WPA-supplicant
	if( !wpasupplicant_interfaze_call_add_network_sync(pParams->pWPASupplicantInterface, pVariant, &pParams->networkObjectPath, NULL, &pParams->pError) )
	{
		g_warning("Cannot setup network");
		pParams->returnCode = 1;
		return;
	}

	if( !wpasupplicant_interfaze_call_select_network_sync(pParams->pWPASupplicantInterface, pParams->networkObjectPath, NULL, &pParams->pError) )
	{
		g_error("Cannot select network");
		pParams->returnCode = 1;
		return;
	}
}

static void handover_release_wifi_agent_cb(void *pUserData)
{
	//Exit program
	g_debug("Agent released");
	params_t* pParams = (params_t*) pUserData;
	pParams->returnCode = 0;
	g_main_loop_quit(pParams->pMainLoop);
}

static void tag_found(const char *name, void *pUserData)
{
	g_printf("Tag found\r\n");
}

static void device_found(const char *name, void *pUserData)
{
	g_printf("Device found\r\n");
}

static void tag_device_lost(const char *name, void *pUserData)
{
	//Go through adapters and restart polling
	params_t* pParams = (params_t*) pUserData;
	pParams->returnCode = start_stop_polling(pParams, TRUE);
	if(pParams->returnCode != 0)
	{
		g_main_loop_quit(pParams->pMainLoop);
	}
}

static void record_found(const char *recordName, void *pUserData)
{
}

static void dhcp_client_exit(GPid pid, gint status, gpointer pUserData)
{
	params_t* pParams = (params_t*) pUserData;

	//Cleanup
	g_spawn_close_pid(pid);

	if( status == 0 )
	{
		g_printf("Connected!\r\n");

		pParams->connected = TRUE;

		//Exit program
		if(!pParams->keepPolling)
		{
			g_main_loop_quit(pParams->pMainLoop);
		}
	}
	else
	{
		g_printf("Could not obtain DHCP lease\r\n");
	}
}

static void wpa_supplicant_interface_properties_changed(WPASupplicantInterfaze *object, GVariant *arg_properties, gpointer pUserData)
{
	params_t* pParams = (params_t*) pUserData;
	GVariantDict dict;
	g_variant_dict_init(&dict, arg_properties);
	gchar* state = NULL;
	if(g_variant_dict_lookup (&dict, "State", "s", &state))
	{
		g_printf("%s: %s\r\n", pParams->interface, state);

		if(!strcmp(state, "completed"))
		{
			//Start DHCP client

			g_printf("Killing existing DHCP clients...\r\n");
			gchar* args[] = {"dhclient", "-x", pParams->interface, NULL};
			g_spawn_sync(NULL, args, NULL,
									 G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
						             NULL,
						             NULL,
						             NULL,
						             NULL,
						             NULL,
						             NULL);

			GPid dhcpPid;
			g_printf("Running DHCP client...\r\n");
			gchar* args2[] = {"dhclient", "-v", pParams->interface, NULL};
			if( g_spawn_async_with_pipes(NULL, args2, NULL,
						 G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
			             NULL,
			             NULL,
			             &dhcpPid,
			             NULL,
			             NULL,
			             NULL,
			             NULL) )
			{
				//Set callback
				g_child_watch_add(dhcpPid, dhcp_client_exit, pParams);
			}
			else
			{
				pParams->returnCode = 1;
				g_error("Could not start DHCP client");
				g_main_loop_quit(pParams->pMainLoop);
			}
		}
		g_free(state);
	}
}

static gboolean terminate(gpointer pUserData)
{
	params_t* pParams = (params_t*) pUserData;
	g_idle_add ((GSourceFunc) g_main_loop_quit, pParams->pMainLoop);

	return FALSE;
}

int main(int argc, char** argv)
{
	params_t params;
	params.debug = FALSE;
	params.keepPolling = FALSE;
	params.adapterObjectPath = NULL;
	params.interface = NULL;
	params.returnCode = 0;
	params.pConnection = NULL;
	params.pWPASupplicant = NULL;
	params.pWPASupplicantInterface = NULL;
	params.pMainLoop = NULL;
	params.returnCode = 0;
	params.agentRegistered = FALSE;
	params.interfaceObjectPath = NULL;
	params.networkObjectPath = NULL;
	params.connected = FALSE;
	params.pError = NULL;

	//Parse options
	const GOptionEntry entries[] =
	{
	  { "debug", 'd', 0, G_OPTION_ARG_NONE, &params.debug, "Enable debugging mode", NULL },
	  { "keep-polling", 'k', 0, G_OPTION_ARG_NONE, &params.keepPolling, "Keep polling", NULL },
	  { "adapter", 'a', 0, G_OPTION_ARG_STRING, &params.adapterObjectPath, "Use a specific adapter", NULL},
	  { "interface", 'i', 0, G_OPTION_ARG_STRING, &params.interface, "Use a specific network interface (defaults to wlan0)", NULL},
	  { NULL }
	};

	GOptionContext* pContext = g_option_context_new(NULL);
	g_option_context_add_main_entries(pContext, entries, NULL);

	if(!g_option_context_parse(pContext, &argc, &argv, &params.pError))
	{
		params.returnCode = 1;
		g_option_context_free(pContext);
		goto exit;
	}
	g_option_context_free(pContext);

	if(params.debug)
	{
		g_setenv("G_MESSAGES_DEBUG", "all", 1);
	}

	params.pConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &params.pError);
	if (params.pConnection == NULL) {
		g_error("Cannot access system bus");
		params.returnCode = 1;
		goto exit;
	}

	//Try to find the network interface
	params.pWPASupplicant = wpasupplicant_proxy_new_sync(params.pConnection, G_DBUS_PROXY_FLAGS_NONE, WPA_SUPPLICANT_NAMESPACE, WPA_SUPPLICANT_OBJECT_PATH, NULL, &params.pError);
	if (params.pWPASupplicant == NULL) {
		params.returnCode = 1;
		goto exit;
	}

	if(params.interface == NULL)
	{
		params.interface = g_strdup("wlan0"); //Default interface name
		g_debug("Using default interface %s", params.interface);
	}

	if( !wpasupplicant_call_get_interface_sync(params.pWPASupplicant, params.interface, &params.interfaceObjectPath, NULL, NULL) )
	{
		g_debug("Trying to add interface to WPA supplicant");

		//Try to create new interface
		GVariantDict dict;
		g_variant_dict_init(&dict, NULL);

		g_variant_dict_insert(&dict, "Ifname", "s", params.interface);

		GVariant* pVariant = g_variant_dict_end(&dict);

		if( !wpasupplicant_call_create_interface_sync(params.pWPASupplicant, pVariant, &params.interfaceObjectPath, NULL, &params.pError) )
		{
			g_debug("Cannot use or access interface");
			params.returnCode = 1;
			goto exit;
		}
	}

	//Now connect to this interface
	params.pWPASupplicantInterface = wpasupplicant_interfaze_proxy_new_sync(params.pConnection, G_DBUS_PROXY_FLAGS_NONE, WPA_SUPPLICANT_NAMESPACE, params.interfaceObjectPath, NULL, &params.pError);
	if (params.pWPASupplicantInterface == NULL) {
		params.returnCode = 1;
		goto exit;
	}

	//Connect signal to check interface's status
	g_signal_connect(params.pWPASupplicantInterface, "properties-changed",
						G_CALLBACK(wpa_supplicant_interface_properties_changed), &params);

	//Initialize GLib main loop
    params.pMainLoop = g_main_loop_new(NULL, FALSE);

    //Add signals handling
    g_unix_signal_add(SIGTERM, terminate, (gpointer)&params);
    g_unix_signal_add(SIGINT, terminate, (gpointer)&params);

    //Add tag found callback
    neardal_set_cb_tag_found(tag_found, (gpointer)&params);
    neardal_set_cb_dev_found(device_found, (gpointer)&params);

    //Set dummy record found callback
	neardal_set_cb_record_found(record_found, (gpointer)&params);

	//Add tag/device lost callbacks
	neardal_set_cb_tag_lost(tag_device_lost, (gpointer)&params);
	neardal_set_cb_dev_lost(tag_device_lost, (gpointer)&params);

    //Add handover request callback
    errorCode_t errc = neardal_agent_set_handover_cb("wifi", handover_push_wifi_agent_cb, handover_req_wifi_agent_cb, handover_release_wifi_agent_cb, (gpointer)&params);
    if(errc != 1)
    {
    	params.returnCode = 1;
    	goto exit;
    }

    params.agentRegistered = 1;

	params.returnCode = start_stop_polling(&params, TRUE);
	if(params.returnCode != 0)
	{
		goto exit;
	}

	g_printf("Waiting for tag or device...\r\n");

	//Will run till signal or record found
    g_main_loop_run(params.pMainLoop);

exit:
	//Stop polling
	start_stop_polling(&params, FALSE);

	//Remove handover request callback
	if( params.agentRegistered )
	{
		g_debug("Removing handover callback\r\n");
		neardal_agent_set_handover_cb("wifi", NULL, NULL, NULL, NULL);
	}

	//Remove network if not connected
	if(!params.connected && (params.pWPASupplicantInterface != NULL) && (params.networkObjectPath != NULL))
	{
		wpasupplicant_interfaze_call_remove_network_sync(params.pWPASupplicantInterface, params.networkObjectPath, NULL, NULL);
	}

	if( params.pWPASupplicantInterface != NULL )
	{
		g_object_unref(params.pWPASupplicantInterface);
	}

	//De-register if not connected
	if(!params.connected && (params.pWPASupplicant != NULL) && (params.interfaceObjectPath != NULL))
	{
		wpasupplicant_call_remove_interface_sync(params.pWPASupplicant, params.interfaceObjectPath, NULL, NULL);
	}

	if( params.pWPASupplicant != NULL )
	{
		g_object_unref(params.pWPASupplicant);
	}

	if( params.pConnection != NULL )
	{
		g_dbus_connection_close_sync(params.pConnection, NULL, NULL);
		g_object_unref(params.pConnection);
	}

	if( params.networkObjectPath != NULL )
	{
		g_free(params.networkObjectPath);
	}

	if( params.adapterObjectPath != NULL )
	{
		g_free(params.adapterObjectPath);
	}

	if( params.interface != NULL )
	{
		g_free(params.interface);
	}

	if( params.interfaceObjectPath != NULL )
	{
		g_free(params.interfaceObjectPath);
	}

	if( params.returnCode != 0 )
	{
		if(params.pError != NULL)
		{
			g_printerr("%s\r\n", params.pError->message);
			g_error_free(params.pError);
		}
	}

	return params.returnCode;
}
