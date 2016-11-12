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

#ifndef HAL_INTERNAL_H_
#define HAL_INTERNAL_H_

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include "hal.h"

/* Configuration of the hardware platform */
//#include <phhwConfig.h>

#define HAL_IMPL_NFC_NDEF_MESSAGE_MAX_SIZE (16*1024)
#define HAL_POLLING_LOOP_INTERVAL 200 //Milliseconds
#define HAL_TAG_PRESENCE_CHECK_INTERVAL 500
#define HAL_DEVICE_PRESENCE_CHECK_INTERVAL 200

#define HAL_THREAD_NAME "NFC HAL"
#define HAL_CMD_POLLING_LOOP_START			0
#define HAL_CMD_POLLING_LOOP_STOP  			1
#define HAL_CMD_TAG_NDEF_WRITE	  			2
//#define HAL_CMD_DEVICE_NDEF_PUSH			3
#define HAL_CMD_JOIN						3
//#define HAL_CMD_INTL_DEVICE_LOST			4

#define HAL_CB_MODE_CHANGED					0
#define HAL_CB_POLLING_CHANGED				1
#define HAL_CB_TAG_DETECTED					2
#define HAL_CB_TAG_LOST						3
#define HAL_CB_DEVICE_DETECTED				4
#define HAL_CB_DEVICE_NDEF_RECEIVED			5
#define HAL_CB_DEVICE_LOST					6

/*
 * Reader Library Headers
 */
/* Status code definitions */
#include <ph_Status.h>
/* Generic HAL Component of
 * Reader Library Framework.*/
#include <phhalHw.h>
/* Generic ISO14443-3A Component of
 * Reader Library Framework */
#include <phpalI14443p3a.h>
/* Generic ISO14443-3B Component of
 * Reader Library Framework */
#include <phpalI14443p3b.h>
/* Generic ISO14443-4 Component of
 * Reader Library Framework */
#include <phpalI14443p4.h>
#include <phpalI14443p4a.h>
/* Generic Felica Component of
 * Reader Library Framework */
#include <phpalFelica.h>
/* Generic ISO18092 passive initiator
 * mode Component of Reader Library Framework. */
#include <phpalI18092mPI.h>
/* Generic ISO18092 passive target
 * mode Component of Reader Library Framework. */
#include <phpalI18092mT.h>
/* Generic Discovery Loop Activities
 * Component of Reader Library Framework */
#include <phacDiscLoop.h>
/* Generic BAL Component of
 * Reader Library Framework */
#include <phbalReg.h>
/* Generic OSAL Component of
 * Reader Library Framework */
#include <phOsal.h>
/* Generic MIFARE(R) Component of
 * Reader Library Framework. */
#include <phpalMifare.h>
/* Generic MIFARE(R) Ultralight Application
 * Component of Reader Library Framework. */
#include <phalMful.h>
/* Generic MIFARE DESFire(R) EV1 Application
 * Component of Reader Library Framework.*/
#include <phalMfdf.h>
/* Generic Felica Application
 * Component of Reader Library Framework.*/
#include <phalFelica.h>
/* Generic Type 1 Application
 * Component of Reader Library Framework.*/
#include <phalT1T.h>
/* Generic MIFARE(R) Application
 * Component of Reader Library Framework.*/
#include <phalMfc.h>
/* Generic NDEF Component of
 * Reader Library Framework.*/
#include <phalTop.h>
/* Generic KeyStore Component of
 * Reader Library Framework.*/
#include <phKeyStore.h>
/* SNEP and LLCP headers */
#include <phlnLlcp.h>
#include <phnpSnep.h>

#include <phhwConfig.h>


/**
 * Macros and Definitions
 */
#define PRETTY_PRINTING              /**< Enable pretty printing */

//#define DEBUG

#ifdef  DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#if 0
/* prints if error is detected */
#define CHECK_STATUS(x)                                      \
        if ((x) != PH_ERR_SUCCESS)                               \
        {                                                            \
           g_warning("Line: %d   Error - (0x%04X) has occurred : 0xCCEE CC-Component ID, EE-Error code. Refer-ph_Status.h\n", __LINE__, (x));    \
        }

/* Returns if error is detected */
#define CHECK_SUCCESS(x)              \
        if ((x) != PH_ERR_SUCCESS)        \
        {                                     \
            g_warning("Line: %d   Error - (0x%04X) has occurred : 0xCCEE CC-Component ID, EE-Error code. Refer-ph_Status.h\n ", __LINE__, (x)); \
            return x;                         \
        }
#endif
struct hal_impl;
typedef struct hal_impl hal_impl_t;

struct hal_impl_cmd_info
{
	gint type;
	union {
		//Polling start
		nfc_mode_t mode;

		//Tag / device
		struct
		{
			union
			{
				guint tagId;
				guint deviceId;
			};
			guint8* buffer;
			gsize bufferLength;
		};
	};
};
typedef struct hal_impl_cmd_info hal_impl_cmd_info_t;


struct hal_impl_cb_info
{
	gint type;
	union {
		//Tag related
		guint tagId;

		//Device related
		guint deviceId;

		//Mode
		nfc_mode_t mode;

		//Polling
		gboolean polling;
	};
	struct hal_impl* pHal;
};
typedef struct hal_impl_cb_info hal_impl_cb_info_t;


enum hal_impl_nfc_type
{
	//Tags
	hal_impl_nfc_tag_type_1,
	hal_impl_nfc_tag_type_2,
	hal_impl_nfc_tag_type_3,
	hal_impl_nfc_tag_type_4a,

	//NFC-DEP devices
	hal_impl_nfc_device_nfc_dep_a_target,
	hal_impl_nfc_device_nfc_dep_a_initiator,
	hal_impl_nfc_device_nfc_dep_f_target,
	hal_impl_nfc_device_nfc_dep_f_initiator,
};
typedef enum hal_impl_nfc_type hal_impl_nfc_type_t;

#define HAL_IMPL_NFC_TYPE_IS_TAG(type) (((type)<= hal_impl_nfc_tag_type_4a )?TRUE:FALSE)
#define HAL_IMPL_NFC_TYPE_IS_TAG_ISO14443A(type) ((((type) == hal_impl_nfc_tag_type_1) || ((type) == hal_impl_nfc_tag_type_2) || ((type) == hal_impl_nfc_tag_type_4a) )?TRUE:FALSE)
#define HAL_IMPL_NFC_TYPE_IS_TAG_FELICA(type) (((type) == hal_impl_nfc_tag_type_3)?TRUE:FALSE)

#define HAL_IMPL_NFC_DEVICE_TYPE_IS_INITIATOR(type) (((type)==hal_impl_nfc_device_nfc_dep_a_initiator)||((type)==hal_impl_nfc_device_nfc_dep_f_initiator))
#define HAL_IMPL_NFC_DEVICE_TYPE_IS_TARGET(type) (((type)==hal_impl_nfc_device_nfc_dep_a_target)||((type)==hal_impl_nfc_device_nfc_dep_f_target))

enum hal_impl_nfc_ndef_status
{
	hal_impl_nfc_ndef_status_readwrite,
	hal_impl_nfc_ndef_status_readonly,
	hal_impl_nfc_ndef_status_formattable,
	hal_impl_nfc_ndef_status_invalid,
};
typedef enum hal_impl_nfc_ndef_status hal_impl_nfc_ndef_status_t;

struct hal_impl_nfc_ndef_message
{
	guint8* buffer;
	gsize size;
	gsize length;
};
typedef struct hal_impl_nfc_ndef_message hal_impl_nfc_ndef_message_t;

struct hal_impl_nfc_iso14443a_params
{
	guint8 atqa[2];
	guint8 sak;
	guint8 uid[10];
	gsize uidLength;
};
typedef struct hal_impl_nfc_iso14443a_params hal_impl_nfc_iso14443a_params_t;

struct hal_impl_nfc_felica_params
{
	guint8 manufacturer[2];
	guint8 cid[6];
	guint8 ic[2];
	guint8 maxRespTimes[6];
};
typedef struct hal_impl_nfc_felica_params hal_impl_nfc_felica_params_t;

struct hal_impl_tag
{
	guint id;
	gboolean connected;
	hal_impl_nfc_type_t type;
	hal_impl_nfc_ndef_status_t status;
	hal_impl_nfc_ndef_message_t message;
	hal_impl_nfc_iso14443a_params_t iso14443a;
	hal_impl_nfc_felica_params_t felica;
//	hal_impl_nfc_ndef_message_t outMessage;
	gint refs;
	GRecMutex mutex;
};
typedef struct hal_impl_tag hal_impl_tag_t;

struct hal_impl_device
{
	guint id;
	gboolean connected;
	hal_impl_nfc_type_t type;
	hal_impl_nfc_ndef_message_t message;
	gint refs;
	GRecMutex mutex;
};
typedef struct hal_impl_device hal_impl_device_t;

#define HAL_BUFFER_TX_SIZE 256
#define HAL_BUFFER_RX_SIZE 256

#define HAL_BUFFER_SERVICE_NAME_SIZE 	32
#define HAL_BUFFER_WORKING_SIZE  		384
#define HAL_BUFFER_CHUNKING_SIZE		256
#define HAL_BUFFER_SNEP_HEADER_SIZE 	10
#define HAL_BUFFER_APP_RECEIVE_SIZE 	1050
#define HAL_BUFFER_APP_TEMP_SIZE 		1050

#define PH_ERR_FAILED PH_ERR_ABORTED


struct rdlib
{
	phbalReg_Stub_DataParams_t		   balReader;                 /* BAL component holder */
	uint8_t                            bHalBufferTx[256];         /* HAL TX buffer. Size 256 - Based on maximum FSL */
	uint8_t                            bHalBufferRx[256];         /* HAL RX buffer. Size 256 - Based on maximum FSL */
	uint8_t  						   aAtrRes[64];				  /* ATR response holder */
	uint16_t                           wAtrResLength;            /* ATR response length */
	phhalHw_Nfc_Ic_DataParams_t        hal;                       /* HAL component holder */
	phpalI14443p3a_Sw_DataParams_t     palI14443p3a;              /* PAL I14443-A component */
	phpalI14443p4a_Sw_DataParams_t     palI14443p4a;              /* PAL I14443-4A component */
	phpalI14443p3b_Sw_DataParams_t     palI14443p3b;              /* PAL I14443-B component */
	phpalI14443p4_Sw_DataParams_t      palI14443p4;               /* PAL I14443-4 component */
	phpalFelica_Sw_DataParams_t        palFelica;                 /* PAL Felica component */
	phpalI18092mPI_Sw_DataParams_t     palI18092mPI;              /* PAL MPI component */
	phpalI18092mT_Sw_DataParams_t      palI18092mT;               /* PAL MT component */
	phpalMifare_Sw_DataParams_t        palMifare;                 /* PAL Mifare component */
	phalMful_Sw_DataParams_t           alMful;                    /* AL Mifare UltraLite component */
	phalMfdf_Sw_DataParams_t           alMfdf;                    /* AL Mifare Desfire component */
	phalFelica_Sw_DataParams_t         alFelica;                  /* AL Felica component */
	phalT1T_Sw_DataParams_t            alT1T;                      /* AL T1T component */
	phalTop_Sw_DataParams_t            tagop;                      /* AL Top component */
	phalTop_T1T_t                      t1tparam;                   /* T1T Param for Tag operations */
	phalTop_T2T_t                      t2tparam;                   /* T2T Param for Tag operations */
	phalTop_T3T_t                      t3tparam;                   /* T3T Param for Tag operations */
	phalTop_T4T_t                      t4tparam;                   /* T4T Param for Tag operations */
	phacDiscLoop_Sw_DataParams_t       discLoop;                  /* Discovery loop component */
	void                              *pHal;                      /* HAL pointer */

	//LLCP & SNEP variables
	phlnLlcp_Sw_DataParams_t           slnLlcp;                    /* LLCP component holder */

	/* Array allocated to store LLCP General Bytes. */
	uint8_t   aLLCPGeneralBytes[36];
	uint8_t    bLLCPGBLength;
};
typedef struct rdlib rdlib_t;

struct rdlib_llcp
{
	phlnLlcp_Sw_DataParams_t* pslnLlcp;                    /* LLCP component holder */
	uint8_t* pGeneralBytes;
	size_t generalBytesSz;
	uint8_t bDevType;  /* device type PHLN_LLCP_INITIATOR or PHLN_LLCP_TARGET. */
	hal_impl_t* pHal; //For callback
};
typedef struct rdlib_llcp rdlib_llcp_t;

struct rdlib_snep
{
	phlnLlcp_Sw_DataParams_t* pslnLlcp;                    /* LLCP component holder */
	hal_impl_t* pHal; //For callback
};
typedef struct rdlib_snep rdlib_snep_t;

struct rdlib_snep_client_msg
{
	uint8_t* msg;
	size_t msgSz;
};
typedef struct rdlib_snep_client_msg rdlib_snep_client_msg_t;


struct hal_impl
{
	gboolean init;
	struct
	{
		//HAL --> Adapter
		nfc_mode_t currentMode;
		gboolean polling;

		//Adapter --> HAL
		//(None)

		GRecMutex mutex;
	} parameters;

	struct
	{
		gboolean tagOrDevicePresent;

		guint currentTagId;
		guint currentDeviceId;

		gboolean polling;
	} session;

	//These can be accessed from multiple threads
	GMutex tagTableMutex;
	GHashTable* pTagTable;

	GMutex deviceTableMutex;
	GHashTable* pDeviceTable;
	//End

	//Main thread
	GThread* pThread;
	gboolean joining;

	//Queues
	GAsyncQueue* pHalQueue;
	GAsyncQueue* pSnepQueue;

	//LLCP thread
	//pthread_t llcpThread;

	//SNEP server thread
	pthread_t snepServerThread;

	//SNEP client thread
	pthread_t snepClientThread;

	//Remote context
	GMainContext* pRemoteMainContext;

	struct
	{
		hal_adapter_on_mode_changed_cb_t onModeChangedCb;
		hal_adapter_on_polling_changed_cb_t onPollingChangedCb;
		hal_adapter_on_tag_detected_cb_t onTagDetectedCb;
		hal_adapter_on_tag_lost_cb_t onTagLostCb;
		hal_adapter_on_device_detected_cb_t onDeviceDetectedCb;
		hal_adapter_on_device_ndef_received_cb_t onDeviceNDEFReceivedCb;
		hal_adapter_on_device_lost_cb_t onDeviceLostCb;
		GObject* pAdapterObject;
		GRecMutex mutex;
	} adapter;


	//NXP Rd Lib variables
	rdlib_t rdlib;
};
typedef struct hal_impl hal_impl_t;

int hal_impl_tag_new(hal_impl_t* pHal, hal_impl_nfc_type_t nfcType, guint* pTagId);
void hal_impl_tag_disconnected(hal_impl_t* pHal, guint tagId);
void hal_impl_tag_ndef_write(hal_impl_t* pHal, guint tagId, guint8* buffer, gsize length);

int hal_impl_device_new(hal_impl_t* pHal, hal_impl_nfc_type_t nfcType, guint* pDeviceId);
void hal_impl_device_disconnected(hal_impl_t* pHal, guint deviceId);
void hal_impl_device_ndef_push(hal_impl_t* pHal, guint deviceId, guint8* buffer, gsize length);

void rdlib_set_interrupt_cb(uint8_t en);

phStatus_t rdlib_init(hal_impl_t* pHal);
phStatus_t rdlib_llcp_reset(hal_impl_t* pHal);
void rdlib_close(hal_impl_t* pHal);
phStatus_t rdlib_loop_setup(hal_impl_t* pHal, nfc_mode_t pollingMode);
phStatus_t rdlib_loop_iteration(hal_impl_t* pHal, hal_impl_nfc_type_t* pNFCType);

phStatus_t rdlib_tag_ndef_read(hal_impl_t* pHal, guint tagId);
phStatus_t rdlib_tag_ndef_write(hal_impl_t* pHal, guint tagId, guint8* buffer, gsize length);
phStatus_t rdlib_tag_presence_check(hal_impl_t* pHal, guint tagId);

phStatus_t rdlib_llcp_execute(hal_impl_t* pHal, uint8_t* pGeneralBytes, size_t generalBytesSz, uint8_t bDevType);
//phStatus_t rdlib_llcp_close(hal_impl_t* pHal);
//phStatus_t rdlib_llcp_reset(hal_impl_t* pHal);
phStatus_t rdlib_snep_init(hal_impl_t* pHal);
phStatus_t rdlib_snep_close(hal_impl_t* pHal);
phStatus_t rdlib_snep_client_send_message(hal_impl_t* pHal, guint deviceId, guint8* buffer, gsize bufferLength);

void hal_impl_polling_loop_start(hal_impl_t* pHalImpl, nfc_mode_t mode);
void hal_impl_polling_loop_stop(hal_impl_t* pHalImpl);
gboolean hal_impl_polling_loop_iteration_fn(hal_impl_t* pHalImpl);
gboolean hal_impl_tag_present_fn(hal_impl_t* pHalImpl);
//gboolean hal_impl_device_present_fn(gpointer pData);
void hal_impl_device_lost(hal_impl_t* pHal);

void hal_impl_update_polling(hal_impl_t* pHal, gboolean polling);
void hal_impl_update_mode(hal_impl_t* pHal, nfc_mode_t mode);

void hal_impl_call_adapter_on_mode_changed(hal_impl_t* pHal, nfc_mode_t mode);
void hal_impl_call_adapter_on_polling_changed(hal_impl_t* pHal, gboolean polling);
void hal_impl_call_adapter_on_tag_detected(hal_impl_t* pHal, guint tagId);
void hal_impl_call_adapter_on_tag_lost(hal_impl_t* pHal, guint tagId);
void hal_impl_call_adapter_on_device_detected(hal_impl_t* pHal, guint deviceId);
void hal_impl_call_adapter_on_device_ndef_received(hal_impl_t* pHal, guint deviceId);
void hal_impl_call_adapter_on_device_lost(hal_impl_t* pHal, guint deviceId);

void hal_impl_call_cb(hal_impl_t* pHal, hal_impl_cb_info_t* pCbInfo);
gboolean hal_impl_call_remote_context(gpointer pData);

void hal_impl_call_cmd(hal_impl_t* pHal, hal_impl_cmd_info_t* pCmdInfo);
gboolean hal_impl_process_queue(hal_impl_t* pHal, guint32 timeout);

gpointer hal_impl_thread_fn(gpointer param);

#endif /* HAL_INTERNAL_H_ */
