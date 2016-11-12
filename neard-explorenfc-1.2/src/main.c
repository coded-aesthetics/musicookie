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
 * \file main.c
 * \author Donatien Garnier
 */

/*! \mainpage Neard-ExploreNFC
 *
 */

/**
 * Operation System Headers
 */
#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>

#include "dbus-daemon.h"
#include "hal.h"

#define CONFIG_FILE CONFIGDIR "/main.conf"

int main(int argc, char** argv)
{
	gboolean debug = FALSE;
	gboolean daemonize = TRUE;
	gboolean version = FALSE;

	//Parse options
	const GOptionEntry entries[] =
	{
	  { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "Enable debugging mode", NULL },
	  { "nodaemon", 'n', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &daemonize, "Do not fork daemon to background", NULL },
	  { "version", 'v', 0, G_OPTION_ARG_NONE, &version, "Show version information and exit", NULL },
	  { NULL }
	};

	GOptionContext* pContext = g_option_context_new(NULL);
	g_option_context_add_main_entries(pContext, entries, NULL);

	GError* pError = NULL;
	if(!g_option_context_parse(pContext, &argc, &argv, &pError))
	{
		if(pError != NULL)
		{
			g_printerr("%s\r\n", pError->message);
			g_error_free(pError);
		}
		else
		{
			g_printerr("An unknown error occurred\r\n");
		}
		exit(1);
	}
	g_option_context_free(pContext);

	if (version)
	{
		printf("%s\n", PROJECT_VERSION);
		exit(0);
	}

    if(debug)
    {
    	g_setenv("G_MESSAGES_DEBUG", "all", 1);
    }

	//Parse config file
    gboolean constantPoll = FALSE;
	GKeyFile* pKeyFile = g_key_file_new();

	pError = NULL;
	if(g_key_file_load_from_file(pKeyFile, CONFIG_FILE, 0, &pError))
	{
		pError = NULL;
		constantPoll = g_key_file_get_boolean(pKeyFile, "General", "ConstantPoll", &pError);
		if(pError != NULL)
		{
			g_warning("Could not read ConstantPoll parameter, defaulting to TRUE: %s\r\n", pError->message);
			g_error_free(pError);
			constantPoll = TRUE;
		}
	}
	else
	{
		if( (pError != NULL) && (pError->code == G_FILE_ERROR_NOENT) )
		{
			g_warning("Could not find config file %s\r\n", CONFIG_FILE);
		}
		else
		{
			if(pError != NULL)
			{
				g_printerr("%s\r\n", pError->message);
				g_error_free(pError);
			}
			else
			{
				g_printerr("An unknown error occurred\r\n");
			}
			exit(1);
		}
	}

    if(daemonize)
    {
		if (daemon(0, 0))
		{
			g_printerr("Could not daemonize\r\n");
			exit(1);
		}
    }

    hal_t* pHal = hal_impl_new();
    hal_impl_init(pHal, g_main_context_default());

    g_info("Constant polling is %s", constantPoll?"enabled":"disabled");

    GMainLoop* pGMainLoop = g_main_loop_new(NULL, FALSE);

    DBusDaemon* pDBusDaemon = dbus_daemon_new(pHal, pGMainLoop);
    dbus_daemon_set_constant_poll(pDBusDaemon, constantPoll);

    g_info("Starting main loop");

    g_main_loop_run(pGMainLoop);

    g_info("Exiting main loop");

    g_object_unref(pDBusDaemon);

    hal_impl_free(pHal);

    g_info("End\r\n");

    return 0;
}
