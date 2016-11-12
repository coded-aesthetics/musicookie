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
 * \file wifi-connect-swc-parser.h
 */

#ifndef WIFI_CONNECT_WSC_PARSER_H_
#define WIFI_CONNECT_WSC_PARSER_H_

#include <glib.h>

enum wsc_auth
{
	wsc_auth_open,
	wsc_auth_wpa_psk,
	wsc_auth_shared,
	wsc_auth_wpa_eap,
	wsc_auth_wpa2_eap,
	wsc_auth_wpa2_psk,
	wsc_auth_wpa_wpa2_psk,
	wsc_auth_wpa_none
};
typedef enum wsc_auth wsc_auth_t;

enum wsc_enc
{
	wsc_enc_none,
	wsc_enc_wep,
	wsc_enc_tkip,
	wsc_enc_aes,
	wsc_enc_tkip_aes
};
typedef enum wsc_enc wsc_enc_t;

//https://code.google.com/r/wsxcde290-gafgaf/source/browse/trunk/linux-3.0.x/drivers/net/wireless/rt3090/include/wsc_tlv.h
//http://linux.die.net/man/5/wpa_supplicant.conf
//https://github.com/kapt/nfcpy/blob/master/nfc/ndef/wifi_record.py

struct wsc_data
{
	gint version;
	gchar* ssid;
	guint8* mac;
	gchar* key;

	wsc_auth_t auth;
	wsc_enc_t enc;
};
typedef struct wsc_data wsc_data_t;

gint wsc_blob_parse(guint8* data, gsize length, wsc_data_t** ppWscData);

void wsc_data_free(wsc_data_t* pWscData);

void wsc_data_print(wsc_data_t* pWscData);

#endif /* WIFI_CONNECT_WSC_PARSER_H_ */
