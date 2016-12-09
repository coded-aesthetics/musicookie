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
//#include <fmod.h>

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
	neardal_record* pRcd;
	gchar* writeMessage;
	gchar* messageType;
	gchar* language;
	gboolean copy;
	gboolean isCopied;	
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
//        inst = libvlc_new (0, NULL);
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
void event_man(const struct libvlc_event_t  * event, void *data)
{
g_printf("what");
    g_printf( "type %s %s", event->type, libvlc_event_type_name (event->type));
}

static void play_song(char* filename) { //, FMOD_SYSTEM *system) {
     libvlc_media_t *m;
     //gboolean isPlaying;
          const char *const opts[] = {"--aout=alsa"};
     /* Load the VLC engine */
     if (inst == NULL) {
        inst = libvlc_new (0,  opts); 
         }
     if (mp != NULL) {
 	libvlc_media_player_stop (mp);
        g_printf("%s %s", filename, currentSong);  
        if(strcmp(filename, currentSong) == 0) {
          strcpy(currentSong, "");
          return;
        }
     	/* Free the media_player */
        libvlc_media_player_release (mp);
     }
     
     strcpy(currentSong, filename);      

     //FMOD_CHANNEL *channel = 0;

     //float* vol;

     //FMOD_Channel_GetVolume(channel, vol);

     //g_printf("test %f", vol);

     //FMOD_Channel_SetVolume(channel, 1.000);
     //FMOD_Channel_GetVolume(channel, vol);

     //g_printf("test %f", vol);

     //FMOD_System_Init(system, 1, FMOD_INIT_NORMAL, NULL);     
     //g_printf("\n%s\n", filename);
     char basedir[] = "file:///home/pi/songs/";
     /* Create a new item */
     //g_printf("\nTESTESTEST\n");
     strcat(basedir, filename);

     g_printf("\nNow playing: %s\n", basedir);

     m = libvlc_media_new_location (inst, basedir);
     //m = libvlc_media_new_path (inst, "/path/to/test.mov");
     
     //int pid = execl("/usr/bin/omxplayer", "/usr/bin/omxplayer",  "/home/pi/tungg.mp3", (char*) NULL);
     
     //g_printf("%i", pid);

     //FMOD_SOUND *dooropen;

     //FMOD_RESULT resultat7 = FMOD_System_CreateStream(system, "/home/pi/tungg.mp3",FMOD_CREATESTREAM | FMOD_LOOP_OFF | FMOD_2D, 0, &dooropen);

     //FMOD_System_PlaySound(system, dooropen, 0, false, &channel);

     /* Create a media player playing environement */
     mp = libvlc_media_player_new_from_media (m);
    //man = libvlc_media_player_event_manager( mp );
    //libvlc_event_attach(man,libvlc_MediaPlayerStopped    ,event_man,NULL);

     /* No need to keep the media now */
     libvlc_media_release (m);

     /* play the media_player */
     libvlc_media_player_play (mp);
    

//     params->isPlaying = TRUE;
    // sleep (100); /* Let it play a bit */
     	/* Stop playing */
    // 	 libvlc_media_player_stop (mp);

     	/* Free the media_player */
    // 	libvlc_media_player_release (mp);

    
     //libvlc_release (inst);
 
 }


static void dump_record(neardal_record* pRecord) {
	if( pRecord->name != NULL )
	{
		g_printf("Found record %s\r\n", pRecord->name);
	}
	else
	{
		g_printf("Found record\r\n");
	}

	if( pRecord->type != NULL )
	{
		g_printf("Record type: \t%s\r\n", pRecord->type);
	}
	else
	{
		g_printf("Unknown record type\r\n");
	}

	//Dump fields that are set
	if( pRecord->uri != NULL )
	{
		g_printf("URI: \t%s\r\n", pRecord->uri);
	}

	if( pRecord->representation != NULL )
	{
		g_printf("Title: \t%s\r\n", pRecord->representation);

                play_song(pRecord->representation);
	}

	if( pRecord->action != NULL )
	{
		g_printf("Action: \t%s\r\n", pRecord->action);
	}

	if( pRecord->language != NULL )
	{
		g_printf("Language: \t%s\r\n", pRecord->language);
	}

	if( pRecord->encoding != NULL )
	{
		g_printf("Encoding: \t%s\r\n", pRecord->encoding);
	}

	if( pRecord->mime != NULL )
	{
		g_printf("MIME type: \t%s\r\n", pRecord->mime);
	}

	if( pRecord->uriObjSize > 0 )
	{
		g_printf("URI object size: \t%u\r\n", pRecord->uriObjSize);
	}

	if( pRecord->carrier != NULL )
	{
		g_printf("Carrier: \t%s\r\n", pRecord->carrier);
	}

	if( pRecord->ssid != NULL )
	{
		g_printf("SSID: \t%s\r\n", pRecord->ssid);
	}

	if( pRecord->passphrase != NULL )
	{
		g_printf("Passphrase: \t%s\r\n", pRecord->passphrase);
	}

	if( pRecord->encryption != NULL )
	{
		g_printf("Encryption: \t%s\r\n", pRecord->encryption);
	}

	if( pRecord->authentication != NULL )
	{
		g_printf("Authentication: \t%s\r\n", pRecord->authentication);
	}
}

static gchar* bytes_to_str(GBytes* bytes)
{
	gchar* str = g_malloc0( 2*g_bytes_get_size(bytes) + 1 );
	const guint8* data = g_bytes_get_data(bytes, NULL);
	for(int i = 0 ; i < g_bytes_get_size(bytes); i++)
	{
		sprintf(&str[2*i], "%02X", data[i]);
	}
	return str;
}



static void dump_tag(neardal_tag* pTag)
{
	if( pTag->iso14443aAtqa != NULL )
	{
		gchar *str = bytes_to_str(pTag->iso14443aAtqa);
		g_printf("ISO14443A ATQA: \t%s\r\n", str);
		g_free(str);
	}

	if( pTag->iso14443aSak != NULL )
	{
		gchar *str = bytes_to_str(pTag->iso14443aSak);
		g_printf("ISO14443A SAK: \t%s\r\n", str);
		g_free(str);
	}

	if( pTag->iso14443aUid != NULL )
	{
		gchar *str = bytes_to_str(pTag->iso14443aUid);
		g_printf("ISO14443A UID: \t%s\r\n", str);
		g_free(str);
	}

	if( pTag->felicaManufacturer != NULL )
	{
		gchar *str = bytes_to_str(pTag->felicaManufacturer);
		g_printf("Felica Manufacturer: \t%s\r\n", str);
		g_free(str);
	}

	if( pTag->felicaCid != NULL )
	{
		gchar *str = bytes_to_str(pTag->felicaCid);
		g_printf("Felica CID: \t%s\r\n", str);
		g_free(str);
	}

	if( pTag->felicaIc != NULL )
	{
		gchar *str = bytes_to_str(pTag->felicaIc);
		g_printf("Felica IC: \t%s\r\n", str);
		g_free(str);
	}

	if( pTag->felicaMaxRespTimes != NULL )
	{
		gchar *str = bytes_to_str(pTag->felicaMaxRespTimes);
		g_printf("Felica Maximum response times: \t%s\r\n", str);
		g_free(str);
	}
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
	dump_record(pRecord);
	if(pParams->copy && !(pParams->isCopied))
	{
		if(strcmp(pRecord->type,"Text") != 0 &&
		   strcmp(pRecord->type,"URI") != 0 &&
//		   strcmp(pRecord->type,"HandoverCarrier") != 0 &&
//		   strcmp(pRecord->type,"HandoverRequest") != 0 &&
//		   strcmp(pRecord->type,"HandoverSelect") != 0 &&
		   strcmp(pRecord->type,"SmartPoster") != 0)
		{
			g_printf("Not a valid record type for write (currently only 'Text', 'URI' or 'SmartPoster' are supported)\r\n");
			exit(1);
		}
		if( pParams->pRcd != NULL )
		{
			neardal_free_record(pParams->pRcd);
		}

		pParams->pRcd = pRecord;
		pParams->isCopied = TRUE;
		g_printf("Record is stored\r\n");
	}
	else
	{
		neardal_free_record(pRecord);
	}
}



static void tag_found(const char *tagName, void *pUserData)
{
	g_printf("Tag found\r\n");
        //FMOD_SYSTEM *system;
        //FMOD_System_Create(&system);
        
	errorCode_t	err;
	neardal_tag* pTag;
	params_t* pParams = (params_t*) pUserData;

	err = neardal_get_tag_properties(tagName, &pTag);
	if(err != NEARDAL_SUCCESS)
	{
		g_warning("Error %d when reading tag %s (%s)\r\n", err, tagName, neardal_error_get_text(err));
		return;
	}

	if(pParams->writeMessage != NULL || (pParams->copy && pParams->isCopied))
	{
		if(pTag->readOnly)
		{
			g_printf("Tag is set to read only\r\n");
			return;
		}

		if(pParams->pRcd->name != NULL)
		{
			g_free(pParams->pRcd->name);
		}
		pParams->pRcd->name = g_strdup(tagName);
		err = neardal_tag_write(pParams->pRcd);
		if(err != NEARDAL_SUCCESS)
		{
			g_warning("Error %d when writing tag %s (%s)\r\n", err, tagName, neardal_error_get_text(err));
			return;
		}
		g_printf("Record was written successfully\r\n");
	}
	else
	{
		//Dump record's content
		dump_tag(pTag);
	}
	neardal_free_tag(pTag);
}

static void device_found(const char *devName, void *pUserData)
{
	g_printf("Device found\r\n");
	errorCode_t err;
	neardal_dev* pDev;
	params_t* pParams = (params_t*) pUserData;

	err = neardal_get_dev_properties(devName, &pDev);
	if(err != NEARDAL_SUCCESS)
	{
		g_warning("Error %d when reading device %s (%s)\r\n", err, devName, neardal_error_get_text(err));
		return;
	}

	neardal_free_device(pDev);

	if(pParams->writeMessage != NULL || (pParams->copy && pParams->isCopied))
	{
		if(pParams->pRcd->name != NULL)
		{
			g_free(pParams->pRcd->name);
		}
		pParams->pRcd->name = g_strdup(devName);
		err = neardal_dev_push(pParams->pRcd);
		if(err != NEARDAL_SUCCESS)
		{
			g_warning("Error %d when pushing to device %s (%s)\r\n", err, devName, neardal_error_get_text(err));
			return;
		}
		g_printf("Push to device was successful\r\n");
	}

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

	params.pRcd = NULL;
	params.writeMessage = NULL;
	params.messageType = "";
	params.language = "en";
	params.copy = FALSE;
	params.isCopied = FALSE;
	
               //isPlaying = FALSE;
	//params.isPlaying = FALSE;
       

	//Parse options
	const GOptionEntry entries[] =
	{
	 { "debug", 'd', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &params.debug, "Enable debugging mode", NULL },
	 { "keep-polling", 'k', 0, G_OPTION_ARG_NONE, &params.keepPolling, "Keep polling", NULL },
	 { "adapter", 'a', 0, G_OPTION_ARG_STRING, &params.adapterObjectPath, "Use a specific adapter", NULL},
	 { "write", 'w', 0, G_OPTION_ARG_STRING, &params.writeMessage, "Message to write", NULL},
	 { "type", 't', 0, G_OPTION_ARG_STRING, &params.messageType, "Text, URI,...", "Text"},
	 { "copy", 'c', 0, G_OPTION_ARG_NONE, &params.copy, "Copy record (use with -k)", NULL},
	 { "language", 'l', 0, G_OPTION_ARG_STRING, &params.language, "Language of record", "en"},
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

	if(params.writeMessage != NULL)	//references: https://github.com/connectivity/neardal/blob/master/lib/neardal.h
	{
		params.pRcd = g_malloc(sizeof(neardal_record));
		if(params.copy)
		{
			g_printf("Combination of copy and write not possible\r\n");
			return 1;
		}
		if(strcmp(params.messageType,"") == 0)
		{
			g_printf("Please specify record type using '-t' 'Text' or 'URI'\r\n");
			return 1;			
		}
		if(strcmp(params.messageType,"Text") != 0 &&
		   strcmp(params.messageType,"URI") != 0 &&
//		   strcmp(params.messageType,"HandoverCarrier") != 0 &&
//		   strcmp(params.messageType,"HandoverRequest") != 0 &&
//		   strcmp(params.messageType,"HandoverSelect") != 0 &&
		   strcmp(params.messageType,"SmartPoster") != 0)
		{
			g_printf("Not a valid record type for write (currently only 'Text', 'URI' or 'SmartPoster' are supported)\r\n");
			return 1;
		}
		params.pRcd->name = NULL;
		params.pRcd->action = g_strdup("Save"); //Save, Edit, Download
		params.pRcd->carrier = g_strdup("Bluetooth"); //Bluetooth
		params.pRcd->encoding = g_strdup("UTF-8");
		params.pRcd->language = g_strdup(params.language); //ISO/IANA: en, jp, etc.
		params.pRcd->mime = g_strdup("application/vnd.wfa.wsc"); //audio/mp3 //text/plain
		params.pRcd->representation = g_strdup(params.writeMessage);
		params.pRcd->type = g_strdup(params.messageType);
		params.pRcd->ssid = g_strdup("vt"); //params.writeMessage;
		params.pRcd->passphrase = g_strdup(" ");
		params.pRcd->encryption = g_strdup("NONE"); //NONE,WEP,TKIP,AES
		params.pRcd->authentication = g_strdup("OPEN"); //OPEN,WPA-Personal,Shared,WPA-Enterprise,WPA2-Enterprise,WPA2-Personal
		params.pRcd->uri = params.writeMessage;//"http://www.nxp.com";
		params.pRcd->uriObjSize = 0;//The size of the object pointed by the URI, not the URI's length
	}

	//Initialize GLib main loop
	params.pMainLoop = g_main_loop_new(NULL, FALSE);

	//Add tag found callback
	neardal_set_cb_tag_found(tag_found, (gpointer)&params);
	neardal_set_cb_dev_found(device_found, (gpointer)&params);

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

	g_printf("Waiting for tag or device...\r\n");

	//Will run till signal or record found
	g_main_loop_run(params.pMainLoop);

	if( params.adapterObjectPath != NULL )
	{
		g_free(params.adapterObjectPath);
	}
	if(params.pRcd != NULL)
	{
		neardal_free_record(params.pRcd);
	}

	return params.returnCode;
}
