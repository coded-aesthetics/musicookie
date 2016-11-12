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

#include "hal.h"
#include "hal_internal.h"

#include "dbus-parameters.h"
#include "tag.h"

#include <unistd.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

/* Parameters for L3 activation during Autocoll */
static const  uint8_t  sens_res[2]     = {0x04, 0x00};              /* ATQ bytes - needed for anti-collision */
static const  uint8_t  nfc_id1[3]      = {0xA1, 0xA2, 0xA3};        /* user defined bytes of the UID (one is hardcoded) - needed for anti-collision */
static const  uint8_t  sel_res         = 0x40;                      /* SAK (ISO18092mT) - needed for anti-collision */
static const  uint8_t  nfc_id3         = 0xFA;                      /* NFC3 byte - required for anti-collision */
static const  uint8_t  poll_res[18]    = {0x01, 0xFE, 0xB2, 0xB3, 0xB4, 0xB5,
                                   0xB6, 0xB7, 0xC0, 0xC1, 0xC2, 0xC3,
                                   0xC4, 0xC5, 0xC6, 0xC7, 0x23, 0x45 }; /* felica - needed for anti-collision */

/* DID is not used by default. */
static const uint8_t   bDid = 0;
static const uint8_t   bBst = 0x00;
static const uint8_t   bBrt = 0x00;
/* Set TO (time out) value in ATR_RES which will be used to set FDT/FWT on P2P initiator. */
static const uint8_t   bTo = 8;
/* Set LRT value to 3. AS LLCP mandates that LRT value to be 3. */
static const uint8_t   bLrt = 3;

//All these commands called from external (main) thread
hal_t* hal_impl_new()
{
	hal_impl_t* pHal = g_malloc(sizeof(hal_impl_t));

	pHal->init = FALSE;

	return (hal_t*)pHal;
}

int hal_impl_init(hal_t* pHal, GMainContext* pGMainContext)
{
    phStatus_t  status;

    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    if( pHalImpl->init == TRUE )
    {
    	return 0;
    }

    pHalImpl->pRemoteMainContext = pGMainContext;

    //Take a ref on the remote main context
    g_main_context_ref(pHalImpl->pRemoteMainContext);

    //Create queues
    pHalImpl->pHalQueue = g_async_queue_new();
    pHalImpl->pSnepQueue = g_async_queue_new();

    //Init callbacks
    pHalImpl->adapter.onModeChangedCb = NULL;
    pHalImpl->adapter.onPollingChangedCb = NULL;
    pHalImpl->adapter.onTagDetectedCb = NULL;
    pHalImpl->adapter.onTagLostCb = NULL;
    pHalImpl->adapter.onDeviceDetectedCb = NULL;
    pHalImpl->adapter.onDeviceLostCb = NULL;
    pHalImpl->adapter.pAdapterObject = NULL;
    g_rec_mutex_init(&pHalImpl->adapter.mutex);

    //Init parameters
    pHalImpl->parameters.currentMode = nfc_mode_idle;
    pHalImpl->parameters.polling = FALSE;
    g_rec_mutex_init(&pHalImpl->parameters.mutex);

    //Init session parameters
    pHalImpl->session.currentDeviceId = 0;
    pHalImpl->session.currentTagId = 0;
    pHalImpl->session.polling = FALSE;
    pHalImpl->session.tagOrDevicePresent = FALSE;

    pHalImpl->pTagTable = g_hash_table_new(g_direct_hash, g_direct_equal);
    g_mutex_init(&pHalImpl->tagTableMutex);

    pHalImpl->pDeviceTable = g_hash_table_new(g_direct_hash, g_direct_equal);
    g_mutex_init(&pHalImpl->deviceTableMutex);

    pHalImpl->init = TRUE;
    pHalImpl->joining = FALSE;

    //Spawn thread for loop
    pHalImpl->pThread = g_thread_new("RdLib", hal_impl_thread_fn, (gpointer) pHalImpl);

    return 0;
}


void hal_impl_free(hal_t* pHal)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

	if( pHalImpl->init == TRUE )
	{
		//Stop polling loop
	    hal_impl_cmd_info_t* pCmdInfo = g_malloc(sizeof(hal_impl_cmd_info_t));
	    pCmdInfo->type = HAL_CMD_JOIN;
		hal_impl_call_cmd(pHalImpl, pCmdInfo);

		//Join thread
		g_thread_join(pHalImpl->pThread); //This dereferences the thread as well

		//Delete queues
    	g_async_queue_unref(pHalImpl->pHalQueue);
    	g_async_queue_unref(pHalImpl->pSnepQueue);

		//Free tables
		g_hash_table_destroy(pHalImpl->pTagTable);
		g_hash_table_destroy(pHalImpl->pDeviceTable);
	}

	g_free(pHal);
}

void hal_adapter_register(hal_t* pHal, GObject* pAdapterObject,
		hal_adapter_on_mode_changed_cb_t onModeChangedCb,
		hal_adapter_on_polling_changed_cb_t onPollingChangedCb,
		hal_adapter_on_tag_detected_cb_t onTagDetectedCb,
		hal_adapter_on_tag_lost_cb_t onTagLostCb,
		hal_adapter_on_device_detected_cb_t onDeviceDetectedCb,
		hal_adapter_on_device_ndef_received_cb_t onDeviceNDEFReceivedCb,
		hal_adapter_on_device_lost_cb_t onDeviceLostCb
		)
{
	hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    //Get mutex
    g_rec_mutex_lock(&pHalImpl->adapter.mutex);

    pHalImpl->adapter.onModeChangedCb = onModeChangedCb;
    pHalImpl->adapter.onPollingChangedCb = onPollingChangedCb;
    pHalImpl->adapter.onTagDetectedCb = onTagDetectedCb;
    pHalImpl->adapter.onTagLostCb = onTagLostCb;
    pHalImpl->adapter.onDeviceDetectedCb = onDeviceDetectedCb;
    pHalImpl->adapter.onDeviceNDEFReceivedCb = onDeviceNDEFReceivedCb;
    pHalImpl->adapter.onDeviceLostCb = onDeviceLostCb;
    pHalImpl->adapter.pAdapterObject = pAdapterObject;

    //Get a reference on pAdapterObject
    g_object_ref(pAdapterObject);

	//Release mutex
    g_rec_mutex_unlock(&pHalImpl->adapter.mutex);
}

void hal_adapter_unregister(hal_t* pHal, GObject* pAdapterObject)
{
	hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    //Get mutex
    g_rec_mutex_lock(&pHalImpl->adapter.mutex);

    //Release reference on pAdapterObject
    g_object_unref(pHalImpl->adapter.pAdapterObject);

    pHalImpl->adapter.onModeChangedCb = NULL;
    pHalImpl->adapter.onPollingChangedCb = NULL;
    pHalImpl->adapter.onTagDetectedCb = NULL;
    pHalImpl->adapter.onTagLostCb = NULL;
    pHalImpl->adapter.onDeviceDetectedCb = NULL;
    pHalImpl->adapter.onDeviceLostCb = NULL;
    pHalImpl->adapter.pAdapterObject = NULL;

	//Release mutex
    g_rec_mutex_unlock(&pHalImpl->adapter.mutex);
}

void hal_adapter_polling_loop_start(hal_t* pHal, nfc_mode_t mode)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    hal_impl_cmd_info_t* pCmdInfo = g_malloc(sizeof(hal_impl_cmd_info_t));
    pCmdInfo->type = HAL_CMD_POLLING_LOOP_START;
    pCmdInfo->mode = mode;
	hal_impl_call_cmd(pHalImpl, pCmdInfo);
}

void hal_adapter_polling_loop_stop(hal_t* pHal)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    hal_impl_cmd_info_t* pCmdInfo = g_malloc(sizeof(hal_impl_cmd_info_t));
    pCmdInfo->type = HAL_CMD_POLLING_LOOP_STOP;
	hal_impl_call_cmd(pHalImpl, pCmdInfo);
}

nfc_mode_t hal_adapter_get_mode(hal_t* pHal)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

	nfc_mode_t mode;
	g_rec_mutex_lock(&pHalImpl->parameters.mutex);
	mode = pHalImpl->parameters.currentMode;
	g_rec_mutex_unlock(&pHalImpl->parameters.mutex);

	return mode;
}

gboolean hal_adapter_is_polling(hal_t* pHal)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

	gboolean polling;
	g_rec_mutex_lock(&pHalImpl->parameters.mutex);
	polling = pHalImpl->parameters.polling;
	g_rec_mutex_unlock(&pHalImpl->parameters.mutex);

	return polling;
}

//All these commands called from our own polling loop
void hal_impl_polling_loop_start(hal_impl_t* pHalImpl, nfc_mode_t mode)
{
	if( !pHalImpl->session.polling && !pHalImpl->session.tagOrDevicePresent )
	{
		//Configure polling loop
		phStatus_t status = rdlib_loop_setup(pHalImpl, mode);
		if( status == PH_ERR_SUCCESS )
		{
			//Start polling loop
			pHalImpl->session.polling = TRUE;

			//Say we are polling
			hal_impl_update_polling(pHalImpl, TRUE);
		}
		else
		{
			g_error("Could not start polling loop, error %02x\r\n", status);
		}
	}
}

void hal_impl_polling_loop_stop(hal_impl_t* pHalImpl)
{
	if( pHalImpl->session.polling )
	{
		pHalImpl->session.polling = FALSE;

		//Say we are not polling anymore
		hal_impl_update_polling(pHalImpl, FALSE);
	}
}

gboolean hal_impl_polling_loop_iteration_fn(hal_impl_t* pHalImpl)
{
	hal_impl_nfc_type_t nfcType;

    //Poll
	phStatus_t status = rdlib_loop_iteration(pHalImpl, &nfcType);

	if(status == PH_ERR_SUCCESS)
	{
		if( HAL_IMPL_NFC_TYPE_IS_TAG(nfcType) )
		{
			//Create tag
			status = hal_impl_tag_new(pHalImpl, nfcType, &pHalImpl->session.currentTagId);

			//Read NDEF
			if(status == PH_ERR_SUCCESS)
			{
				//Try to read tag
				rdlib_tag_ndef_read(pHalImpl, pHalImpl->session.currentTagId);
			}

			if(status == PH_ERR_SUCCESS)
			{
				//Advertise NFC tag to adapter
				hal_impl_call_adapter_on_tag_detected(pHalImpl, pHalImpl->session.currentTagId);
				pHalImpl->session.tagOrDevicePresent = TRUE;
			}

		}
		else //Device
		{
			//Try to init LLCP + SNEP server
			status = hal_impl_device_new(pHalImpl, nfcType, &pHalImpl->session.currentDeviceId);

			if(status == PH_ERR_SUCCESS)
			{
				//Advertise NFC tag to adapter
				hal_impl_call_adapter_on_device_detected(pHalImpl, pHalImpl->session.currentDeviceId);
				pHalImpl->session.tagOrDevicePresent = TRUE;

				//g_timeout_add(HAL_DEVICE_PRESENCE_CHECK_INTERVAL, hal_impl_device_present_fn, pHalImpl);
			}

		}

		if( pHalImpl->session.tagOrDevicePresent )
		{
			//Say we are not polling anymore
			hal_impl_update_polling(pHalImpl, FALSE);
		    pHalImpl->session.polling = FALSE;

			//Update radio mode
			if( HAL_IMPL_NFC_TYPE_IS_TAG(nfcType) || HAL_IMPL_NFC_DEVICE_TYPE_IS_TARGET(nfcType) )
			{
				hal_impl_update_mode(pHalImpl, nfc_mode_initiator);
			}
			else
			{
				hal_impl_update_mode(pHalImpl, nfc_mode_target);
			}

			if( HAL_IMPL_NFC_TYPE_IS_TAG(nfcType) )
			{
				//Check presence every 200ms and wait for command
				do
				{
					hal_impl_process_queue(pHalImpl, HAL_TAG_PRESENCE_CHECK_INTERVAL);
				} while( hal_impl_tag_present_fn(pHalImpl) );
			}
			else
			{
				if( HAL_IMPL_NFC_DEVICE_TYPE_IS_INITIATOR(nfcType) )
				{
		            uint16_t      wGeneralBytesLength;
		            uint8_t     * pGeneralBytes;
		            status = phpalI18092mT_Activate(&pHalImpl->rdlib.palI18092mT,
		            	pHalImpl->rdlib.discLoop.sTargetParams.pRxBuffer,
		            	pHalImpl->rdlib.discLoop.sTargetParams.wRxBufferLen,
		            	pHalImpl->rdlib.aAtrRes,
		            	pHalImpl->rdlib.wAtrResLength,
		                &pGeneralBytes,
		                &wGeneralBytesLength
		                );
		            CHECK_STATUS(status);

					/* Activate LLCP with the received ATR_RES in target mode. */
					status = rdlib_llcp_execute(pHalImpl, pGeneralBytes, wGeneralBytesLength, PHLN_LLCP_TARGET);
					CHECK_STATUS(status);
				}
				else
				{
					/* Get the ATR_RES length. */
					uint16_t wGtLength = 0;
					status = phacDiscLoop_GetConfig(&pHalImpl->rdlib.discLoop, PHAC_DISCLOOP_CONFIG_TYPEF_P2P_ATR_RES_LEN, &wGtLength);
					CHECK_STATUS(status);

					/* Activate LLCP with the received ATR_RES in initiator mode. */
					status = rdlib_llcp_execute(pHalImpl, &pHalImpl->rdlib.aAtrRes[PHLN_LLCP_ATR_RES_MIN_LEN], (wGtLength - PHLN_LLCP_ATR_RES_MIN_LEN), PHLN_LLCP_INITIATOR);
					CHECK_STATUS(status);
				}

				hal_impl_device_lost(pHalImpl);
			}

			return FALSE;
		}
	}

	return TRUE;
}

gboolean hal_impl_tag_present_fn(hal_impl_t* pHalImpl)
{
    //Check tag presence
	phStatus_t status = rdlib_tag_presence_check(pHalImpl, pHalImpl->session.currentTagId);
	if(status != PH_ERR_SUCCESS)
	{
		//If tag lost
		//Set tag as disconnected and unref it
		hal_impl_tag_disconnected(pHalImpl, pHalImpl->session.currentTagId);
		hal_tag_unref((hal_t*)pHalImpl, pHalImpl->session.currentTagId);

		hal_impl_update_mode(pHalImpl, nfc_mode_idle);

		pHalImpl->session.tagOrDevicePresent = FALSE;

		//Callback to adapter
		hal_impl_call_adapter_on_tag_lost(pHalImpl, pHalImpl->session.currentTagId);

		return FALSE;
	}

	return TRUE;
}


void hal_impl_device_lost(hal_impl_t* pHal)
{
	//If device lost
	//Set device as disconnected and unref it


	hal_impl_device_disconnected(pHal, pHal->session.currentDeviceId);
	hal_device_unref((hal_t*)pHal, pHal->session.currentDeviceId);

	hal_impl_update_mode(pHal, nfc_mode_idle);

	pHal->session.tagOrDevicePresent = FALSE;

	//Callback to adapter
	hal_impl_call_adapter_on_device_lost(pHal, pHal->session.currentDeviceId);
}

phStatus_t rdlib_init(hal_impl_t* pHal)
{
    phStatus_t  status;

	/* Set the interface link for the internal chip communication */
	Set_Interface_Link();

    /* Perform a hardware reset */
    Reset_reader_device();

    /* Initialize the Reader BAL (Bus Abstraction Layer) component */
    phbalReg_Stub_Init( &pHal->rdlib.balReader, sizeof(phbalReg_Stub_DataParams_t));

    /* Initialize the OSAL Events. */
    status = phOsal_Event_Init();
    CHECK_STATUS(status);

    //Start interrupt thread
    Set_Interrupt();

    /* Initialize the Reader HAL (Hardware Abstraction Layer) component */
    status = phbalReg_SetConfig(
        &pHal->rdlib.balReader,
        PHBAL_REG_CONFIG_HAL_HW_TYPE,
        PHBAL_REG_HAL_HW_RC523);

    /* Open BAL */
    status = phbalReg_OpenPort(&pHal->rdlib.balReader);
    CHECK_STATUS(status);

    /* Initialize the Reader HAL (Hardware Abstraction Layer) component */
    status = phhalHw_Nfc_IC_Init(
        &pHal->rdlib.hal,
        sizeof(phhalHw_Nfc_Ic_DataParams_t),
        &pHal->rdlib.balReader,
        0,
		pHal->rdlib.bHalBufferTx,
        sizeof(pHal->rdlib.bHalBufferTx),
		pHal->rdlib.bHalBufferRx,
        sizeof(pHal->rdlib.bHalBufferRx));

    /* Set the parameter to use the SPI interface */
    pHal->rdlib.hal.sHal.bBalConnectionType = PHHAL_HW_BAL_CONNECTION_SPI;

    Configure_Device(&pHal->rdlib.hal);

    /* Initialize the I14443-A PAL layer */
    status = phpalI14443p3a_Sw_Init(&pHal->rdlib.palI14443p3a, sizeof(phpalI14443p3a_Sw_DataParams_t), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Initialize the I14443-A PAL component */
    status = phpalI14443p4a_Sw_Init(&pHal->rdlib.palI14443p4a, sizeof(phpalI14443p4a_Sw_DataParams_t), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Initialize the I14443-4 PAL component */
    status = phpalI14443p4_Sw_Init(&pHal->rdlib.palI14443p4, sizeof(phpalI14443p4_Sw_DataParams_t), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Initialize the I14443-B PAL  component */
    status = phpalI14443p3b_Sw_Init(&pHal->rdlib.palI14443p3b, sizeof(pHal->rdlib.palI14443p3b), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Initialize PAL Felica PAL component */
    status = phpalFelica_Sw_Init(&pHal->rdlib.palFelica, sizeof(phpalFelica_Sw_DataParams_t), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Init 18092I PAL component */
    status = phpalI18092mPI_Sw_Init(&pHal->rdlib.palI18092mPI, sizeof(phpalI18092mPI_Sw_DataParams_t), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Init 18092T PAL component */
    status = phpalI18092mT_Sw_Init(&pHal->rdlib.palI18092mT, sizeof(phpalI18092mT_Sw_DataParams_t), &pHal->rdlib.hal, NULL);
    CHECK_SUCCESS(status);

    /* Initialize the Mifare PAL component */
    status = phpalMifare_Sw_Init(&pHal->rdlib.palMifare, sizeof(phpalMifare_Sw_DataParams_t), &pHal->rdlib.hal, &pHal->rdlib.palI14443p4);
    CHECK_SUCCESS(status);

    /* Initialize the Felica AL component */
    status = phalFelica_Sw_Init(&pHal->rdlib.alFelica, sizeof(phalFelica_Sw_DataParams_t), &pHal->rdlib.palFelica);
    CHECK_SUCCESS(status);

    /* Initialize the T1T AL component */
    status = phalT1T_Sw_Init(&pHal->rdlib.alT1T, sizeof(phalT1T_Sw_DataParams_t), &pHal->rdlib.palI14443p3a);
    CHECK_SUCCESS(status);

    /* Initialize the Mful AL component */
    status = phalMful_Sw_Init(&pHal->rdlib.alMful, sizeof(phalMful_Sw_DataParams_t), &pHal->rdlib.palMifare, NULL, NULL, NULL);
    CHECK_SUCCESS(status);

    /* Initialize the MF DesFire EV1 component */
    status = phalMfdf_Sw_Init(&pHal->rdlib.alMfdf, sizeof(phalMfdf_Sw_DataParams_t), &pHal->rdlib.palMifare, NULL, NULL, NULL, &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Initialize NDEF component */
    status = phalTop_Sw_Init(&pHal->rdlib.tagop, sizeof(phalTop_Sw_DataParams_t), &pHal->rdlib.t1tparam, &pHal->rdlib.t2tparam, &pHal->rdlib.t3tparam, &pHal->rdlib.t4tparam, NULL);
    CHECK_SUCCESS(status);

    ((phalTop_T1T_t *)(pHal->rdlib.tagop.pT1T))->pAlT1TDataParams = &pHal->rdlib.alT1T;
    ((phalTop_T2T_t *)(pHal->rdlib.tagop.pT2T))->pAlT2TDataParams = &pHal->rdlib.alMful;
    ((phalTop_T3T_t *)(pHal->rdlib.tagop.pT3T))->pAlT3TDataParams = &pHal->rdlib.alFelica;
    ((phalTop_T4T_t *)(pHal->rdlib.tagop.pT4T))->pAlT4TDataParams = &pHal->rdlib.alMfdf;

    /* Initialize the discover component */
    status = phacDiscLoop_Sw_Init(&pHal->rdlib.discLoop, sizeof(phacDiscLoop_Sw_DataParams_t), &pHal->rdlib.hal);
    CHECK_SUCCESS(status);

    /* Set listen parameters in HAL buffer used during Autocoll */
    status = phhalHw_Rc523_SetListenParameters(&pHal->rdlib.hal.sHal, (uint8_t*)&sens_res[0], (uint8_t*)&nfc_id1[0], sel_res, (uint8_t*)&poll_res[0], nfc_id3);
    CHECK_SUCCESS(status);

    pHal->rdlib.discLoop.pPal1443p3aDataParams  = &pHal->rdlib.palI14443p3a;
    pHal->rdlib.discLoop.pPal1443p3bDataParams  = &pHal->rdlib.palI14443p3b;
    pHal->rdlib.discLoop.pPal1443p4aDataParams  = &pHal->rdlib.palI14443p4a;
    pHal->rdlib.discLoop.pPal14443p4DataParams  = &pHal->rdlib.palI14443p4;
    pHal->rdlib.discLoop.pAlT1TDataParams		= &pHal->rdlib.alT1T;
    pHal->rdlib.discLoop.pPal18092mPIDataParams = &pHal->rdlib.palI18092mPI;
    pHal->rdlib.discLoop.pPalFelicaDataParams   = &pHal->rdlib.palFelica;
    pHal->rdlib.discLoop.pHalDataParams         = &pHal->rdlib.hal;

    //Init LLCP
    rdlib_llcp_reset(pHal);

    /* Assign ATR response for Type A */
    pHal->rdlib.discLoop.sTypeATargetInfo.sTypeA_P2P.pAtrRes                    = pHal->rdlib.aAtrRes;

    /* Assign ATR response for Type F */
    pHal->rdlib.discLoop.sTypeFTargetInfo.sTypeF_P2P.pAtrRes                    = pHal->rdlib.aAtrRes;

    /* Assign ATS buffer for Type A */
    pHal->rdlib.discLoop.sTypeATargetInfo.sTypeA_I3P4.pAts                      = pHal->rdlib.aAtrRes;

    /* Set max retry count of 1 in PAL 18092 Initiator to allow only one MAC recovery cycle. */
    status = phpalI18092mPI_SetConfig(&pHal->rdlib.palI18092mPI, PHPAL_I18092MPI_CONFIG_MAXRETRYCOUNT, 0x01);
    CHECK_STATUS(status);

    //Init SNEP
    status = rdlib_snep_init(pHal);
    CHECK_SUCCESS(status);


    return PH_ERR_SUCCESS;
}

void rdlib_close(hal_impl_t* pHal)
{
	rdlib_snep_close(pHal);

	Cleanup_Interrupt();
	Cleanup_Interface_Link();
}


phStatus_t rdlib_loop_setup(hal_impl_t* pHal, nfc_mode_t pollingMode)
{
	phStatus_t status;

	/* I14443p4 and p4A related variables */
	uint8_t   bCidEnabled;      /*Card Identifier Enabler; Unequal '0' if enabled. */
	uint8_t   bCid;             /* Card Identifier; Ignored if *pCidSupported is equal '0'. */
	uint8_t   bNadSupported;      /* Node Address Enabler; Unequal '0' if enabled. */
	uint8_t   bFwi;             /* Frame Waiting Integer. */
	uint8_t   bFsdi;            /* PCD Frame Size Integer; 0-8; */
	uint8_t   bFsci;            /* PICC Frame Size Integer; 0-8; */

#if 0
    const static uint8_t aGi[] = { 0x46,0x66,0x6D, /* Magic NFC */
            0x01,0x01,0x10,        /*VERSION*/
            0x03,0x02,0x00,0x01,   /*WKS*/
            0x04,0x01,0xF1         /*LTO*/
         };
#endif
	phacDiscLoop_Sw_DataParams_t* psDiscLoop = &pHal->rdlib.discLoop;

	if( (pollingMode == nfc_mode_initiator) || (pollingMode == nfc_mode_dual) )
	{
	    /* Passive poll bitmap configuration. */
	    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_POLL_TECH_CFG, (PHAC_DISCLOOP_POS_BIT_MASK_A |
	        PHAC_DISCLOOP_POS_BIT_MASK_B | PHAC_DISCLOOP_POS_BIT_MASK_F212 | PHAC_DISCLOOP_POS_BIT_MASK_F424));
	    CHECK_SUCCESS(status);

	    /* Passive CON_DEVICE limit for Type A. */
	    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEA_DEVICE_LIMIT, 1);
	    CHECK_SUCCESS(status);

	    /* Passive CON_DEVICE limit for Type F. */
	    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEF_DEVICE_LIMIT, 1);
	    CHECK_SUCCESS(status);
	}
	else
	{
	    /* Passive poll bitmap configuration. */
	    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_POLL_TECH_CFG, 0x00);
	    CHECK_SUCCESS(status);
	}

	if( (pollingMode == nfc_mode_target) || (pollingMode == nfc_mode_dual) )
	{
	    /* Passive listen bitmap configuration. */
	    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_LIS_TECH_CFG,
	        (PHAC_DISCLOOP_POS_BIT_MASK_A | PHAC_DISCLOOP_POS_BIT_MASK_F212 | PHAC_DISCLOOP_POS_BIT_MASK_F424));
	    CHECK_SUCCESS(status);
	}
	else
	{
	    /* Passive listen bitmap configuration. */
	    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_LIS_TECH_CFG, 0x00);
	    CHECK_SUCCESS(status);
	}

    /* Passive Bailout bitmap configuration. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_BAIL_OUT, 0x00);
    CHECK_STATUS(status);

    /* Set LRI value for Type-A polling to 3. LLCP mandates that LRI value to be 3. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEA_P2P_LRI, 3);
    CHECK_STATUS(status);

    /* Set LRI value for Type-F polling to 3. LLCP mandates that LRI value to be 3. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEF_P2P_LRI, 3);
    CHECK_STATUS(status);

    /* Active listen bitmap configuration. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_ACT_LIS_TECH_CFG, 0x00);
    CHECK_SUCCESS(status);

    /* Disable LPCD feature. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_ENABLE_LPCD, PH_OFF);
    CHECK_STATUS(status);

    /* Reset collision pending */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_COLLISION_PENDING, PH_OFF);
    CHECK_STATUS(status);

    /* Set anti-collision is supported. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_ANTI_COLL, PH_ON);
    CHECK_STATUS(status);

    /* Set Discovery loop mode to NFC mode. */
    status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_OPE_MODE, RD_LIB_MODE_NFC);
    CHECK_STATUS(status);

    /* Reset state of layers. */
    (void)phalTop_Reset(&pHal->rdlib.tagop);
    (void)phpalI14443p4_ResetProtocol(&pHal->rdlib.palI14443p4);
    (void)phpalI18092mPI_ResetProtocol(&pHal->rdlib.palI18092mPI);
    (void)phpalI18092mT_ResetProtocol(&pHal->rdlib.palI18092mT);

	/* FSDI means length of Info frame that is reader able to read. The Reader library keeps three values of the FSDI parameter.
	   It is initiated by the Discovery loop to 8 (maximal valid value) but needs to be synchronized with other components using FSDI parameter.
	   Synchronization with the I14443p4A is performed within the DiscLoop, but sync with the I14443p4 needs to be done manually here. */
	pHal->rdlib.palI14443p4a.bFsdi = psDiscLoop->sTypeATargetInfo.sTypeA_I3P4.bFsdi;

	/* Retrieve 14443-4A protocol parameter */
	status = phpalI14443p4a_GetProtocolParams(&pHal->rdlib.palI14443p4a,
	                &bCidEnabled, &bCid, &bNadSupported,
	                &bFwi, &bFsdi, &bFsci);
	CHECK_STATUS(status);

	/* Set 14443-4 protocol parameter */
	status = phpalI14443p4_SetProtocol(&pHal->rdlib.palI14443p4,
	                bCidEnabled, bCid, bNadSupported, 0,
	                bFwi, bFsdi, bFsci);
	CHECK_STATUS(status);

	return PH_ERR_SUCCESS;
}

static phStatus_t rdlib_loop_helper(hal_impl_t* pHal, hal_impl_nfc_type_t* pNFCType)
{
    uint8_t bIndex;
    uint8_t bTagType;
    phStatus_t status = 0;
	phacDiscLoop_Sw_DataParams_t* psDiscLoop = &pHal->rdlib.discLoop;

    /* Get detected technology type */
	uint16_t wTagsDetected = 0;
	status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TECH_DETECTED, &wTagsDetected);
	CHECK_STATUS(status);

    /* Get number of tags detected */
	uint16_t wNumberOfTags = 0;
    status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_NR_TAGS_FOUND, &wNumberOfTags);
    CHECK_STATUS(status);

    /* Required if DETECT_ERROR is not set. DETECT_ERROR is required for debugging purpose only */
    PH_UNUSED_VARIABLE(status);

    /* Check for Type A tag */
    if (PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_A))
    {
        /* Check for T1T */
        if(psDiscLoop->sTypeATargetInfo.bT1TFlag)
        {
            g_debug("Type A : T1T detected \n");

            *pNFCType = hal_impl_nfc_tag_type_1;
            return PH_ERR_SUCCESS;
        }
        else
        {
            g_debug("Technology : Type A");
            /* Loop through all the detected tags (if multiple tags are
            * detected) */
            for(bIndex = 0; bIndex < wNumberOfTags; bIndex++)
            {
                g_debug("\t\tCard : %d",bIndex + 1);

                if ((psDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aSak & (uint8_t) ~0xFB) == 0)
                {
                    /* Bit b3 is set to zero, [Digital] 4.8.2 */
                    /* Mask out all other bits except for b7 and b6 */
                    bTagType = (psDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aSak & 0x60);
                    bTagType = bTagType >> 5;

                    /* Switch to tag type */
                    switch(bTagType)
                    {
                    case PHAC_DISCLOOP_TYPEA_TYPE2_TAG_CONFIG_MASK:
                        g_debug("\t\tType : Type 2 tag\n");

                        //TODO
                       /* if (!DetectClassic(
                        	psDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aAtqa,
                            &psDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aSak))*/
                        {
                        	*pNFCType = hal_impl_nfc_tag_type_2;
                        	return PH_ERR_SUCCESS;
                        }
                        break;

                    case PHAC_DISCLOOP_TYPEA_TYPE4A_TAG_CONFIG_MASK:
                        g_debug("\t\tType : Type 4A tag\n");

                        *pNFCType = hal_impl_nfc_tag_type_4a;
                        return PH_ERR_SUCCESS;

                    case PHAC_DISCLOOP_TYPEA_TYPE_NFC_DEP_TAG_CONFIG_MASK:
                        g_debug("\t\tType : P2P\n");
                        //
                        return PH_ERR_FAILED;

                    case PHAC_DISCLOOP_TYPEA_TYPE_NFC_DEP_TYPE4A_TAG_CONFIG_MASK:
                        g_debug("\t\tType : Type NFC_DEP and 4A tag\n");
                        return PH_ERR_FAILED;

                    default:
                        break;
                    }
                }
            }
        }
    }

    /* Check for Type F tag */
    if( PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_F212) ||
        PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_F424))
    {
        g_debug("Technology: Type F");

        /* Loop through all the type F tags and print the IDm */
        for (bIndex = 0; bIndex < wNumberOfTags; bIndex++)
        {
            g_debug("\t\tCard : %d",bIndex + 1);
            g_debug("\t\tUID  :");

            /* Check data rate  */
            if(psDiscLoop->sTypeFTargetInfo.aTypeFTag[bIndex].bBaud != PHAC_DISCLOOP_CON_BITR_212)
            {
                g_debug("\t\tBit Rate: 424 kbps");
            }
            else
            {
                g_debug("\t\tBit Rate: 212 kbps");
            }
            if ((psDiscLoop->sTypeFTargetInfo.aTypeFTag[bIndex].aIDmPMm[0] == 0x01) &&
                (psDiscLoop->sTypeFTargetInfo.aTypeFTag[bIndex].aIDmPMm[1] == 0xFE))
            {
                /* This is type F tag with P2P capabilities */
                g_debug("\t\tType : P2P\n");
            }
            else
            {
                /* This is Type F T3T tag */
                g_debug("\t\tType : Type 3 tag\n");

                *pNFCType = hal_impl_nfc_tag_type_3;
                return PH_ERR_SUCCESS;
            }
        }
    }
    return PH_ERR_FAILED;
}

phStatus_t rdlib_loop_iteration(hal_impl_t* pHal, hal_impl_nfc_type_t* pNFCType)
{
    phStatus_t    status;

    uint16_t wEntryPoint;
    uint16_t bSavePollTechCfg  = 0;
	phacDiscLoop_Sw_DataParams_t* psDiscLoop = &pHal->rdlib.discLoop;


	uint16_t value;
	status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_POLL_TECH_CFG, &value);
	CHECK_SUCCESS(status);

	if( value != 0 )
	{
		//Start with initiator mode
		wEntryPoint = PHAC_DISCLOOP_ENTRY_POINT_POLL;
	}
	else
	{
		wEntryPoint = PHAC_DISCLOOP_ENTRY_POINT_LISTEN;
	}

	while(true)
	{
        /* Set Discovery poll state to detection */
        status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_NEXT_POLL_STATE, PHAC_DISCLOOP_POLL_STATE_DETECTION);
        CHECK_STATUS(status);

        /* Switch off RF field */
        status = phhalHw_FieldOff(&pHal->rdlib.hal);
        CHECK_STATUS(status);

        /* Start discovery loop */
        status = phacDiscLoop_Run(psDiscLoop, wEntryPoint);
        switch(status & PH_ERR_MASK)
        {
        case PHAC_DISCLOOP_MULTI_TECH_DETECTED:
        case PHAC_DISCLOOP_MULTI_DEVICES_RESOLVED:
        {
        	if( (status & PH_ERR_MASK) == PHAC_DISCLOOP_MULTI_TECH_DETECTED )
        	{
				g_debug("Multiple technologies detected: \n");

				uint16_t wTagsDetected = 0;
				status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TECH_DETECTED, &wTagsDetected);
				CHECK_STATUS(status);

				if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_A))
				{
					g_debug("Type A detected... \n");
				}
				if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_B))
				{
					g_debug("Type B detected... \n");
				}
				if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_F212))
				{
					g_debug("Type F detected with baud rate 212... \n");
				}
				if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_F424))
				{
					g_debug("Type F detected with baud rate 424... \n");
				}

				/* Store user configured poll configuration. */
				status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_POLL_TECH_CFG, &bSavePollTechCfg);
				CHECK_STATUS(status);

				/* Select last detected technology to resolve. */
				for(int idx = 0; idx < PHAC_DISCLOOP_PASS_POLL_MAX_TECHS_SUPPORTED; idx++)
				{
					if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, (1 << idx)))
					{
						/* Configure for one of the detected technology. */
						status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_POLL_TECH_CFG, (1 << idx));
						CHECK_STATUS(status);
					}
				}

				/* Set Discovery loop poll state to collision resolution. */
				status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_NEXT_POLL_STATE, PHAC_DISCLOOP_POLL_STATE_COLLISION_RESOLUTION);
				CHECK_STATUS(status);

				/* Start Discovery loop in pool mode from collision resolution phase */
				status = phacDiscLoop_Run(psDiscLoop, wEntryPoint);
        	}
            switch(status & PH_ERR_MASK)
            {
            case PHAC_DISCLOOP_MULTI_DEVICES_RESOLVED:
            {
            	/* Get detected technology type */
            	uint16_t wTagsDetected = 0;
				status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TECH_DETECTED, &wTagsDetected);
				CHECK_STATUS(status);

				/* Get number of tags detected */
				uint16_t wNumberOfTags = 0;
				status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_NR_TAGS_FOUND, &wNumberOfTags);
				CHECK_STATUS(status);

				g_debug("Multiple cards resolved: %u cards\n",wNumberOfTags);

				if(wNumberOfTags > 1)
				{
					/* Get 1st Detected Technology and Activate device at index 0*/
					for(int idx = 0; idx < PHAC_DISCLOOP_PASS_POLL_MAX_TECHS_SUPPORTED; idx++)
					{
						if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, (1 << idx)))
						{
							g_debug("Activating one card...\n");
							status = phacDiscLoop_ActivateCard(psDiscLoop, idx, 0);
							break;
						}
					}

					if(((status & PH_ERR_MASK) == PHAC_DISCLOOP_DEVICE_ACTIVATED) ||
						((status & PH_ERR_MASK) == PHAC_DISCLOOP_PASSIVE_TARGET_ACTIVATED))
					{
						/* Get Activated Technology Type */
						status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TECH_DETECTED, &wTagsDetected);
						CHECK_STATUS(status);

						/* Print card details */
						g_debug("Activation successful\n");
						return rdlib_loop_helper(pHal, pNFCType);
						//return GetTagInfo(psDiscLoop, 0x01, wTagsDetected);
					}
					else
					{
						g_debug("Card activation failed\n");
						return PH_ERR_FAILED;
					}
				}
				else
				{
					//Continue
				}
            }
            break;

            case PHAC_DISCLOOP_DEVICE_ACTIVATED:
            {
            	g_debug("Card detected and activated successfully. \n");

				return rdlib_loop_helper(pHal, pNFCType);

            }
            break;

            case PHAC_DISCLOOP_PASSIVE_TARGET_ACTIVATED:
            {
            	DEBUG_PRINTF (" \nPassive P2P target detected and activated successfully after collision resolution: \n");

				/* Get the ATR_RES length. */
				uint16_t wGtLength = 0;
				status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEF_P2P_ATR_RES_LEN, &wGtLength);
				CHECK_STATUS(status);

				uint8_t bGBLen = (uint8_t)wGtLength;

				if (bGBLen >= PHLN_LLCP_ATR_RES_MIN_LEN + 3)
				{
					*pNFCType = hal_impl_nfc_device_nfc_dep_f_target;
					return PH_ERR_SUCCESS;
				}
				else
				{
					g_warning("Received ATR_RES length is wrong.\n");
					return PH_ERR_FAILED;
				}
            }
            break;
            default:
                g_warning("Failed to resolve selected technology.\n");
            }

            /* Re-Store user configured poll configuration. */
            status = phacDiscLoop_SetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_PAS_POLL_TECH_CFG, bSavePollTechCfg);
            CHECK_SUCCESS(status);
        }
        break;

        case PHAC_DISCLOOP_NO_TECH_DETECTED:
        case PHAC_DISCLOOP_NO_DEVICE_RESOLVED:
        {
			return PH_ERR_FAILED; //Wait till next iteration
        }

        case PHAC_DISCLOOP_EXTERNAL_RFON:
        {
            wEntryPoint = PHAC_DISCLOOP_ENTRY_POINT_LISTEN;
            continue;
        }
        break;

        case PHAC_DISCLOOP_EXTERNAL_RFOFF:
        {
        	return PH_ERR_FAILED;
        }
        break;

        case PHAC_DISCLOOP_ACTIVATED_BY_PEER:
        {
            memcpy(pHal->rdlib.aAtrRes, poll_res, 10);
            pHal->rdlib.aAtrRes[10] = bBst;
            pHal->rdlib.aAtrRes[11] = bBrt;
            pHal->rdlib.aAtrRes[12] = bTo;
            pHal->rdlib.aAtrRes[13] = bLrt << 4 | 0x02; /* Frame Size is 254 and ATR_RES contains General Bytes. */
            memcpy(&pHal->rdlib.aAtrRes[14], pHal->rdlib.aLLCPGeneralBytes, pHal->rdlib.bLLCPGBLength);
            pHal->rdlib.wAtrResLength = 14 + pHal->rdlib.bLLCPGBLength;

			return PH_ERR_SUCCESS;
        }

        case PHAC_DISCLOOP_PASSIVE_TARGET_ACTIVATED:
        case PHAC_DISCLOOP_MERGED_SEL_RES_FOUND:
        {
            /* Get detected technology type */
        	uint16_t wTagsDetected = 0;
        	status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TECH_DETECTED, &wTagsDetected);
        	CHECK_STATUS(status);

        	uint8_t bGBLen = 0;
        	if ((status & PH_ERR_MASK) == PHAC_DISCLOOP_MERGED_SEL_RES_FOUND)
        	{
				g_debug("Merged SAK: Device having T4T and NFC-DEP support detected.\n");

				/* Send ATR_REQ to activate device in P2P mode. */
				status = phpalI18092mPI_Atr(&pHal->rdlib.palI18092mPI,
					psDiscLoop->sTypeATargetInfo.aTypeA_I3P3[0].aUid,
					0x00,
					0x03,
					0x00,
					0x00,
					pHal->rdlib.aLLCPGeneralBytes,
					psDiscLoop->sTypeATargetInfo.sTypeA_P2P.bGiLength,
					pHal->rdlib.aAtrRes,
					&bGBLen
					);
				CHECK_STATUS(status);

				*pNFCType = hal_impl_nfc_device_nfc_dep_a_target;
        	}
        	else
        	{
        		uint16_t wGtLength = 0;
                if(PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_A))
                {
                    g_debug("Passive P2P target detected and activated successfully at 106kbps. \n");

					*pNFCType = hal_impl_nfc_device_nfc_dep_a_target;

                    /* Get the ATR_RES Length. */
                    status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEA_P2P_ATR_RES_LEN, &wGtLength);
                    CHECK_STATUS(status);
                }
                else if((PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_F212)) ||
                    (PHAC_DISCLOOP_CHECK_ANDMASK(wTagsDetected, PHAC_DISCLOOP_POS_BIT_MASK_F424)))
                {
                	g_debug("Passive P2P target detected and activated successfully at 212/424kbps. \n");

					*pNFCType = hal_impl_nfc_device_nfc_dep_f_target;

                    /* Get the ATR_RES Length. */
                    status = phacDiscLoop_GetConfig(psDiscLoop, PHAC_DISCLOOP_CONFIG_TYPEF_P2P_ATR_RES_LEN, &wGtLength);
                    CHECK_STATUS(status);
                }
                else
                {
                	g_debug("Unknown passive P2P target detected. \n");
                	return PH_ERR_FAILED;
                }

                /* Save the ATR_RES length. */
                bGBLen = (uint8_t)wGtLength;
        	}

			if (bGBLen >= PHLN_LLCP_ATR_RES_MIN_LEN + 3)
			{
				return PH_ERR_SUCCESS;
			}
			else
			{
				g_warning("Received ATR_RES length is wrong.\n");
				return PH_ERR_FAILED;
			}
        }
        break;

        case PHAC_DISCLOOP_DEVICE_ACTIVATED:
        {
            /*Print card info*/
            return rdlib_loop_helper(pHal, pNFCType);
        }
        break;

        default:
        	break;
        }


	}

	return PH_ERR_FAILED;
}

//These functions will call calbacks to be executed in the GMainContext passed in init()

void hal_impl_update_polling(hal_impl_t* pHal, gboolean polling)
{
	g_rec_mutex_lock(&pHal->parameters.mutex);
	if(polling != pHal->parameters.polling)
	{
		pHal->parameters.polling = polling;
		hal_impl_call_adapter_on_polling_changed(pHal, polling);
	}
	g_rec_mutex_unlock(&pHal->parameters.mutex);
}

void hal_impl_update_mode(hal_impl_t* pHal, nfc_mode_t mode)
{
	g_rec_mutex_lock(&pHal->parameters.mutex);
	if(mode != pHal->parameters.currentMode)
	{
		pHal->parameters.currentMode = mode;
		hal_impl_call_adapter_on_mode_changed(pHal, mode);
	}
	g_rec_mutex_unlock(&pHal->parameters.mutex);
}


void hal_impl_call_adapter_on_mode_changed(hal_impl_t* pHal, nfc_mode_t mode)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_MODE_CHANGED;
	pCbInfo->mode = mode;
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_adapter_on_polling_changed(hal_impl_t* pHal, gboolean polling)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_POLLING_CHANGED;
	pCbInfo->polling = polling;
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_adapter_on_tag_detected(hal_impl_t* pHal, guint tagId)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_TAG_DETECTED;
	pCbInfo->tagId = tagId;

	hal_tag_ref((hal_t*)pHal, tagId); //Make sure tag is not deleted before callback is called - see hal_impl_call_main_context
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_adapter_on_tag_lost(hal_impl_t* pHal, guint tagId)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_TAG_LOST;
	pCbInfo->tagId = tagId;
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_adapter_on_device_detected(hal_impl_t* pHal, guint deviceId)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_DEVICE_DETECTED;
	pCbInfo->deviceId = deviceId;

	hal_device_ref((hal_t*)pHal, deviceId); //Make sure device is not deleted before callback is called - see hal_impl_call_main_context
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_adapter_on_device_ndef_received(hal_impl_t* pHal, guint deviceId)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_DEVICE_NDEF_RECEIVED;
	pCbInfo->deviceId = deviceId;

	hal_device_ref((hal_t*)pHal, deviceId); //Make sure device is not deleted before callback is called - see hal_impl_call_main_context
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_adapter_on_device_lost(hal_impl_t* pHal, guint deviceId)
{
	hal_impl_cb_info_t* pCbInfo = g_malloc(sizeof(hal_impl_cb_info_t));
	pCbInfo->pHal = pHal;
	pCbInfo->type = HAL_CB_DEVICE_LOST;
	pCbInfo->deviceId = deviceId;
	hal_impl_call_cb(pHal, pCbInfo);
}

void hal_impl_call_cb(hal_impl_t* pHal, hal_impl_cb_info_t* pCbInfo)
{
	g_main_context_invoke(pHal->pRemoteMainContext, hal_impl_call_remote_context, (gpointer)pCbInfo);
}

//Executed in remote context
gboolean hal_impl_call_remote_context(gpointer pData)
{
	hal_impl_cb_info_t* pCbInfo = (hal_impl_cb_info_t*) pData;
	hal_impl_t* pHal = pCbInfo->pHal;

	g_rec_mutex_lock(&pHal->adapter.mutex); //FIXME useless, impl dependent
	switch(pCbInfo->type)
	{
	case HAL_CB_MODE_CHANGED:
		if( pHal->adapter.onModeChangedCb != NULL )
		{
			pHal->adapter.onModeChangedCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->mode );
		}
		break;
	case HAL_CB_POLLING_CHANGED:
		if( pHal->adapter.onPollingChangedCb != NULL )
		{
			pHal->adapter.onPollingChangedCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->polling );
		}
		break;
	case HAL_CB_TAG_DETECTED:
		if( pHal->adapter.onTagDetectedCb != NULL )
		{
			pHal->adapter.onTagDetectedCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->tagId );
		}
		//Lose temporary ref
		hal_tag_unref((hal_t*)pCbInfo->pHal, pCbInfo->tagId);
		break;
	case HAL_CB_TAG_LOST:
		if( pHal->adapter.onTagLostCb != NULL )
		{
			pHal->adapter.onTagLostCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->tagId );
		}
		break;
	case HAL_CB_DEVICE_DETECTED:
		if( pHal->adapter.onDeviceDetectedCb != NULL )
		{
			pHal->adapter.onDeviceDetectedCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->deviceId );
		}
		//Lose temporary ref
		hal_device_unref((hal_t*)pCbInfo->pHal, pCbInfo->deviceId);
		break;
	case HAL_CB_DEVICE_NDEF_RECEIVED:
		if( pHal->adapter.onDeviceNDEFReceivedCb != NULL )
		{
			pHal->adapter.onDeviceNDEFReceivedCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->deviceId );
		}
		//Lose temporary ref
		hal_device_unref((hal_t*)pCbInfo->pHal, pCbInfo->deviceId);
		break;
	case HAL_CB_DEVICE_LOST:
		if( pHal->adapter.onDeviceLostCb != NULL )
		{
			pHal->adapter.onDeviceLostCb( (hal_t*)pHal, pHal->adapter.pAdapterObject,
					pCbInfo->deviceId );
		}
		break;
	}
	g_rec_mutex_unlock(&pHal->adapter.mutex);

	g_free(pCbInfo);
	return FALSE; //Do not want to be called again
}

void hal_impl_call_cmd(hal_impl_t* pHal, hal_impl_cmd_info_t* pCmdInfo)
{
	g_async_queue_push(pHal->pHalQueue, (gpointer)pCmdInfo);
}

gboolean hal_impl_process_queue(hal_impl_t* pHal, guint32 timeout)
{
	gpointer pData = g_async_queue_timeout_pop(pHal->pHalQueue, timeout * 1000);
	if( pData == NULL )
	{
		return FALSE;
	}

	hal_impl_cmd_info_t* pCmdInfo = (hal_impl_cmd_info_t*) pData;

	switch(pCmdInfo->type)
	{
	case HAL_CMD_POLLING_LOOP_START:
		hal_impl_polling_loop_start(pHal, pCmdInfo->mode);
		break;

	case HAL_CMD_POLLING_LOOP_STOP:
		hal_impl_polling_loop_stop(pHal);
		break;

	case HAL_CMD_TAG_NDEF_WRITE:
		hal_impl_tag_ndef_write(pHal, pCmdInfo->tagId, pCmdInfo->buffer, pCmdInfo->bufferLength);
		g_free(pCmdInfo->buffer);
		break;

	case HAL_CMD_JOIN:
		pHal->joining = TRUE;
		break;
	}

	g_free(pCmdInfo);
	return FALSE; //Do not want to be called again
}

//HAL thread function
gpointer hal_impl_thread_fn(gpointer param)
{
	hal_impl_t* pHal = (hal_impl_t*) param;
    //Init NXP-RDLIB
    if( rdlib_init(pHal) != PH_ERR_SUCCESS )
    {
    	g_error("Could not init NXPRdLib\n");
    	return NULL;
    }

	while(!pHal->joining)
	{
		hal_impl_process_queue(pHal, 10000);
		while(pHal->session.polling)
		{
			hal_impl_polling_loop_iteration_fn(pHal);
			hal_impl_process_queue(pHal, 0);
		}
	}

	//Cleanup rdlib
	rdlib_close(pHal);

	return NULL;
}

