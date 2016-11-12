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
 * \file wifi-connect-swc-parser.c
 */

#include <glib.h>
#include <glib/gprintf.h>
#include "string.h"
#include "wifi-connect-wsc-parser.h"

static wsc_data_t* wsc_data_new();

static gint wsc_blob_parse_internal(guint8* data, gsize size, wsc_data_t* pWscData);

gint wsc_blob_parse(guint8* data, gsize length, wsc_data_t** ppWscData)
{
	gint ret = 0;
	*ppWscData = wsc_data_new();

	ret = wsc_blob_parse_internal(data, length, *ppWscData);

	if(ret != 0)
	{
		wsc_data_free(*ppWscData);
		*ppWscData = NULL;
	}
	return ret;
}

wsc_data_t* wsc_data_new()
{
	wsc_data_t* pWscData = g_malloc(sizeof(wsc_data_t));

	pWscData->version = 0;
	pWscData->ssid = NULL;
	pWscData->mac = NULL;
	pWscData->key = NULL;
	pWscData->auth = wsc_auth_open;
	pWscData->enc = wsc_enc_none;

	return pWscData;
}

void wsc_data_free(wsc_data_t* pWscData)
{
	if(pWscData->ssid != NULL)
	{
		g_free(pWscData->ssid);
	}

	if(pWscData->mac != NULL)
	{
		g_free(pWscData->mac);
	}

	if(pWscData->key != NULL)
	{
		g_free(pWscData->key);
	}

	g_free(pWscData);
}


void wsc_data_print(wsc_data_t* pWscData)
{
	if(pWscData->ssid != NULL)
	{
		g_printf("SSID: %s\r\n", pWscData->ssid);
	}

	if(pWscData->mac != NULL)
	{
		g_printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
				pWscData->mac[0],
				pWscData->mac[1],
				pWscData->mac[2],
				pWscData->mac[3],
				pWscData->mac[4],
				pWscData->mac[5]
				);
	}

	const gchar* auth;
	switch( pWscData->auth )
	{
	case wsc_auth_open:
	default:
		auth = "Open";
		break;
	case wsc_auth_wpa_psk:
		auth = "WPA-PSK";
		break;
	case wsc_auth_shared:
		auth = "WEP";
		break;
	case wsc_auth_wpa_eap:
		auth = "WPA-EAP";
		break;
	case wsc_auth_wpa2_eap:
		auth = "WPA2-EAP";
		break;
	case wsc_auth_wpa2_psk:
		auth = "WPA2-PSK";
		break;
	case wsc_auth_wpa_wpa2_psk:
		auth = "WPA-PSK WPA2-PSK";
		break;
	case wsc_auth_wpa_none:
		auth = "WPA-NONE";
		break;
	}

	g_printf("Authentication: %s\r\n", auth);

	const gchar* enc;
	switch( pWscData->enc )
	{
	case wsc_enc_none:
	default:
		enc = "None";
		break;
	case wsc_enc_wep:
		enc = "WEP";
		break;
	case wsc_enc_tkip:
		enc = "TKIP";
		break;
	case wsc_enc_aes:
		enc = "AES";
		break;
	case wsc_enc_tkip_aes:
		enc = "TKIP+AES";
		break;
	}

	g_printf("Encryption: %s\r\n", enc);


	if(pWscData->key != NULL)
	{
		g_printf("Key: %s\r\n", pWscData->key);
	}
}

#define WSC_TLC_AUTH_TYPE	0x1003
#define WSC_TLV_CREDENTIAL 	0x100E
#define WSC_TLV_ENCR_TYPE	0x100F
#define WSC_TLV_MAC_ADDR	0x1020
#define WSC_TLV_KEY			0x1027
#define WSC_TLV_SSID		0x1045
#define WSC_TLV_VENDOR_EXT	0x1049
#define WSC_TLV_VERSION		0x104A

#define WSC_AUTH_OPEN		0x0001
#define WSC_AUTH_WPA_PSK	0x0002
#define WSC_AUTH_SHARED		0x0004
#define WSC_AUTH_WPA_EAP	0x0008
#define WSC_AUTH_WPA2_EAP	0x0010
#define WSC_AUTH_WPA2_PSK	0x0020
#define WSC_AUTH_WPA_WPA2_PSK	0x0022
#define WSC_AUTH_WPA_NONE	0x0080

#define WSC_ENCR_NONE		0x0001
#define WSC_ENCR_WEP		0x0002
#define WSC_ENCR_TKIP		0x0004
#define WSC_ENCR_AES		0x0008
#define WSC_ENCR_TKIP_AES	0x000C

const static guint8 wfa_id[] = {0x00, 0x37, 0x2A};

gint wsc_blob_parse_internal(guint8* data, gsize size, wsc_data_t* pWscData)
{
	while(size > 0)
	{
		//Read TLV type
		if(size < 2)
		{
			g_error("Could not read type");
			return 1;
		}

		guint16 type = (data[0] << 8) | data[1];
		data += 2;
		size -= 2;

		//Read TLV length
		if(size < 2)
		{
			g_error("Could not read length");
			return 1;
		}

		guint16 length = (data[0] << 8) | data[1];
		data += 2;
		size -= 2;

		//Check payload length
		if( size < length )
		{
			g_error("Could not read payload");
			return 1;
		}

		gint ret = 0;
		switch(type)
		{
			case WSC_TLV_CREDENTIAL:
				ret = wsc_blob_parse_internal(data, length, pWscData);
				if(ret)
				{
					return ret;
				}
				break;
			case WSC_TLV_VERSION:
				if(length != 1)
				{
					g_error("Wrong length");
					return 1;
				}
				pWscData->version = data[0];
				break;
			case WSC_TLV_VENDOR_EXT:
				if(length != 6)
				{
					g_error("Wrong length");
					return 1;
				}
				if( !memcmp(data, wfa_id, 3) )
				{
					if( (data[3] == 0x00) && (data[4] == 0x01) )
					{
						pWscData->version = data[5];
					}
				}
				break;
			case WSC_TLV_SSID:
				if( pWscData->ssid == NULL )
				{
					pWscData->ssid = g_malloc(length + 1);
					memcpy(pWscData->ssid, data, length);
					pWscData->ssid[length] = '\x0';
				}
				break;
			case WSC_TLV_MAC_ADDR:
				if( length != 6 )
				{
					g_error("Wrong MAC size");
					return 1;
				}
				if( pWscData->mac == NULL )
				{
					pWscData->mac = g_malloc(length);
					memcpy(pWscData->mac, data, length);
				}
				break;
			case WSC_TLV_KEY:
				if( pWscData->key == NULL )
				{
					if(length > 0)
					{
						pWscData->key = g_malloc(length + 1);
						memcpy(pWscData->key, data, length);
						pWscData->key[length] = '\x0';
					}
				}
				break;
			case WSC_TLC_AUTH_TYPE:
				if( length != 2)
				{
					g_error("Wrong length");
					return 1;
				}
				switch( (data[0] << 8) | data[1] )
				{
				case WSC_AUTH_OPEN:
					pWscData->auth = wsc_auth_open;
					break;
				case WSC_AUTH_WPA_PSK:
					pWscData->auth = wsc_auth_wpa_psk;
					break;
				case WSC_AUTH_SHARED:
					pWscData->auth = wsc_auth_shared;
					break;
				case WSC_AUTH_WPA_EAP:
					pWscData->auth = wsc_auth_wpa_eap;
					break;
				case WSC_AUTH_WPA2_EAP:
					pWscData->auth = wsc_auth_wpa2_eap;
					break;
				case WSC_AUTH_WPA2_PSK:
					pWscData->auth = wsc_auth_wpa2_psk;
					break;
				case WSC_AUTH_WPA_WPA2_PSK:
					pWscData->auth = wsc_auth_wpa_wpa2_psk;
					break;
				case WSC_AUTH_WPA_NONE:
					pWscData->auth = wsc_auth_wpa_none;
					break;
				default:
					g_warning("Unknown authentication method %04X", (data[0] << 8) | data[1] );
					break;
				}
				break;
				case WSC_TLV_ENCR_TYPE:
					if( length != 2)
					{
						g_error("Wrong length");
						return 1;
					}
					switch( (data[0] << 8) | data[1] )
					{
					case WSC_ENCR_NONE:
						pWscData->enc = wsc_enc_none;
						break;
					case WSC_ENCR_WEP:
						pWscData->enc = wsc_enc_wep;
						break;
					case WSC_ENCR_TKIP:
						pWscData->enc = wsc_enc_tkip;
						break;
					case WSC_ENCR_AES:
						pWscData->enc = wsc_enc_aes;
						break;
					case WSC_ENCR_TKIP_AES:
						pWscData->enc = wsc_enc_tkip_aes;
						break;
					default:
						g_warning("Unknown encryption type %04X", (data[0] << 8) | data[1] );
						break;
					}
					break;
			default:
				//Just skip bytes
				break;
		}

		data += length;
		size -= length;
	}
	return 0;
}
