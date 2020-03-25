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
/*
 * \file basic.c
 */

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <vlc/vlc.h>
#include <unistd.h>

#include "neardal.h"

libvlc_instance_t * inst = NULL;
libvlc_media_player_t *mp = NULL;
char currentSong[64] = "";
libvlc_event_manager_t *man;

struct params
{
	gboolean debug;
	gboolean keepPolling;
	gchar* adapterObjectPath;
 	GMainLoop* pMainLoop;
	gint returnCode;
};
typedef struct params params_t;

static gint start_polling(params_t* pParams)
{
	//List current adapters
	errorCode_t	err;

	int adaptersCount = 0;
	char** adapterNamesArray = NULL;
	err = neardal_get_adapters(&adapterNamesArray, &adaptersCount);
	if ( err == NEARDAL_ERROR_NO_ADAPTER )
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

		if( pAdapter->polling == 0 )
		{
			//Start polling
			g_info("Starting polling for adapter %s\r\n", pAdapter->name);
			err = neardal_start_poll(pAdapter->name); //Mode NEARD_ADP_MODE_INITIATOR
			if(err != NEARDAL_SUCCESS)
			{
				g_warning("Error %d when trying to start polling on adapter %s (%s)\r\n", err, pAdapter->name, neardal_error_get_text(err));
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

/* event handler callback function */
void event_man(const struct libvlc_event_t  * event, void *data)
{
   g_printf("caught event");
   // g_printf( "type %s %s", event->type, libvlc_event_type_name (event->type));
}

static void play_song(char* filename) { 
     libvlc_media_t *m;
     if (inst == NULL) {
        /* Load the VLC engine if not previously initialized */
        const char *const opts[] = {"--aout=alsa"};
        inst = libvlc_new (1,  opts); 
     }
     
     if (mp != NULL) {
 	libvlc_media_player_pause (mp);

        if(strcmp(filename, currentSong) == 0) {
          /* if requested song is that same that is currently playing toggle pause */
          //strcpy(currentSong, ""); // this needs to be done if sound is to be stopped rather than paused
          g_printf("Toggling pause on %s\n", currentSong);
          return;
        }
        /* Free the media_player */
        libvlc_media_player_release (mp);
     }

     /* register new song that is playing */
     strcpy(currentSong, filename);      

     char basedir[128] = "file:///home/pi/songs/";
     strcat(basedir, filename);

     g_printf("Now playing: %s\n", basedir);

     m = libvlc_media_new_location (inst, basedir);

     /* Create a media player playing environment */
     mp = libvlc_media_player_new_from_media (m);
     /* been unsuccesfully trying to listen to end of playback event with the following lines */
     //man = libvlc_media_player_event_manager( mp );
     //libvlc_event_attach(man,libvlc_MediaPlayerEndReached ,event_man,NULL);
     libvlc_audio_set_volume(mp, 100); 

     /* No need to keep the media now */
     libvlc_media_release (m);

     /* play the media_player */
     libvlc_media_player_play (mp);
 }

static void record_found(const char *recordName, void *pUserData)
{
	errorCode_t	err;
	neardal_record* pRecord;

	params_t* pParams = (params_t*) pUserData;

	err = neardal_get_record_properties(recordName, &pRecord);
	if(err != NEARDAL_SUCCESS)
	{
		g_warning("Error %d when reading record %s (%s)\r\n", err, recordName, neardal_error_get_text(err));
		return;
	}

	//Dump record's content
        if( pRecord->representation != NULL ) {
            play_song(pRecord->representation);
	}
	neardal_free_record(pRecord);
}

static void tag_device_lost(const char *name, void *pUserData)
{
	params_t* pParams = (params_t*) pUserData;

	//Exit program
	if(!pParams->keepPolling)
	{
		g_main_loop_quit(pParams->pMainLoop);
		return;
	}

	//Go through adapters and restart polling
	pParams->returnCode = start_polling(pParams);
	if(pParams->returnCode != 0)
	{
		g_main_loop_quit(pParams->pMainLoop);
	}
}

int main(int argc, char** argv)
{
	params_t params;
	params.debug = FALSE;
	params.keepPolling = FALSE;
	params.adapterObjectPath = NULL;
	params.returnCode = 0;

	//Parse options
	const GOptionEntry entries[] =
	{
	  { "debug", 'd', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &params.debug, "Enable debugging mode", NULL },
	  { "keep-polling", 'k', 0, G_OPTION_ARG_NONE, &params.keepPolling, "Keep polling", NULL },
	  { "adapter", 'a', 0, G_OPTION_ARG_STRING, &params.adapterObjectPath, "Use a specific adapter", NULL},
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
		return 1;
	}
	g_option_context_free(pContext);

	if(params.debug)
	{
		g_setenv("G_MESSAGES_DEBUG", "all", 1);
	}

	//Initialize GLib main loop
	params.pMainLoop = g_main_loop_new(NULL, FALSE);

	//Add record found callback
	neardal_set_cb_record_found(record_found, (gpointer)&params);

	//Add tag/device lost callbacks
	neardal_set_cb_tag_lost(tag_device_lost, (gpointer)&params);
	neardal_set_cb_dev_lost(tag_device_lost, (gpointer)&params);

	params.returnCode = start_polling(&params);
	if(params.returnCode != 0)
	{
		return params.returnCode;
	}

	play_song("lets-go.mp3");

	g_printf("Waiting for tag or device...\r\n");

	//Will run till signal or record found
	g_main_loop_run(params.pMainLoop);

	if( params.adapterObjectPath != NULL )
	{
		g_free(params.adapterObjectPath);
	}
	
	return params.returnCode;
}
