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

#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include "phOsal_Posix_Thread.h"

static gpointer hal_impl_snep_thread_fn(gpointer param);
static gpointer hal_impl_snep_server_thread_fn(gpointer param);
static gpointer hal_impl_snep_client_thread_fn(gpointer param);

static void hal_impl_snep_ndef_received_cb(hal_impl_t* pHal, guint8* buffer, gsize length);

int hal_impl_device_new(hal_impl_t* pHal, hal_impl_nfc_type_t nfcType, guint* pDeviceId)
{
	hal_impl_device_t* pDevice = g_malloc(sizeof(hal_impl_device_t));

	//Initialize everything
	pDevice->message.buffer = NULL;
	pDevice->message.length = 0;
	pDevice->message.size = 0;

	pDevice->type = nfcType;

	phStatus_t status;

	//Init other fields from device
	pDevice->connected = TRUE;
	pDevice->refs = 1; //1 reference
	g_rec_mutex_init(&pDevice->mutex);

	//Find a free ID in the hash table and add the device
	g_mutex_lock(&pHal->deviceTableMutex);

	pDevice->id = 0;

	while(g_hash_table_contains(pHal->pDeviceTable, GUINT_TO_POINTER(pDevice->id)))
	{
		pDevice->id++;
	}

	*pDeviceId = pDevice->id;

	g_debug("New device id %d", pDevice->id);

	g_hash_table_insert(pHal->pDeviceTable, GUINT_TO_POINTER(pDevice->id), (gpointer*)pDevice);

	g_mutex_unlock(&pHal->deviceTableMutex);

	return 0;
}

void hal_device_ref(hal_t* pHal, guint deviceId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHalImpl->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHalImpl->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_error("Did not find hal_impl_device_t instance of id %d", deviceId);
		return;
	}
	g_rec_mutex_lock(&pDevice->mutex);
	pDevice->refs++;
	g_rec_mutex_unlock(&pDevice->mutex);
}

void hal_device_unref(hal_t* pHal, guint deviceId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHalImpl->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHalImpl->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_error("Did not find hal_impl_device_t instance of id %d", deviceId);
		return;
	}
	g_rec_mutex_lock(&pDevice->mutex);
	guint refs = --pDevice->refs;
	g_rec_mutex_unlock(&pDevice->mutex);
	if(refs == 0)
	{
		//No one is using the device anymore, so free it
		g_mutex_lock(&pHalImpl->deviceTableMutex);
		g_hash_table_remove(pHalImpl->pDeviceTable, GUINT_TO_POINTER(deviceId));
		g_mutex_unlock(&pHalImpl->deviceTableMutex);

		if(pDevice->message.buffer != NULL)
		{
			g_free(pDevice->message.buffer);
		}

		g_free(pDevice);
	}
}

gboolean hal_device_is_connected(hal_t* pHal, guint deviceId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHalImpl->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHalImpl->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_error("Did not find hal_impl_device_t instance of id %d", deviceId);
		return FALSE;
	}

	g_rec_mutex_lock(&pDevice->mutex);
	gboolean connected = pDevice->connected;
	g_rec_mutex_unlock(&pDevice->mutex);

	return connected;
}

void hal_device_get_ndef(hal_t* pHal, guint deviceId, guint8** pBuffer, gsize* pBufferLength)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHalImpl->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHalImpl->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_error("Did not find hal_impl_device_t instance of id %d", deviceId);
		return;
	}

	g_rec_mutex_lock(&pDevice->mutex);
	*pBufferLength = pDevice->message.length;
	if(pDevice->message.buffer != NULL)
	{
		*pBuffer = g_memdup(pDevice->message.buffer, pDevice->message.length);
	}
	g_rec_mutex_unlock(&pDevice->mutex);
}

void hal_device_push_ndef(hal_t* pHal, guint deviceId, guint8* buffer, gsize bufferLength)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    hal_impl_device_ndef_push(pHalImpl, deviceId, buffer, bufferLength);
}

void hal_impl_device_disconnected(hal_impl_t* pHal, guint deviceId)
{
	g_mutex_lock(&pHal->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHal->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHal->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_error("Did not find hal_impl_device_t instance of id %d", deviceId);
	}
	g_rec_mutex_lock(&pDevice->mutex);
	pDevice->connected = FALSE;
	g_rec_mutex_unlock(&pDevice->mutex);
}

void hal_impl_device_ndef_push(hal_impl_t* pHal, guint deviceId, guint8* buffer, gsize length)
{
	//Make sure tag won't get destroyed by other thread
	hal_device_ref((hal_t*)pHal, deviceId);

	g_mutex_lock(&pHal->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHal->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHal->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_warning("Did not find hal_impl_device_t instance of id %d", deviceId);
		return;
	}

	gboolean connected;
	g_rec_mutex_lock(&pDevice->mutex);
	connected = pDevice->connected;
	g_rec_mutex_unlock(&pDevice->mutex);

	if(connected)
	{
		rdlib_snep_client_send_message(pHal, deviceId, buffer, length); //TODO callback?
	}
	else
	{
		g_warning("Tag is disconnected\r\n");
	}

	hal_device_unref((hal_t*)pHal, deviceId);
}

static void rdlib_llcp(const rdlib_llcp_t* pLlcp);

/**
* \brief    Initialize the LLCP for communication
*/
phStatus_t rdlib_llcp_execute(hal_impl_t* pHal, uint8_t* pGeneralBytes, size_t generalBytesSz, uint8_t bDevType)
{
    phStatus_t status = PH_ERR_SUCCESS;

    rdlib_llcp_t* pLlcp = g_malloc(sizeof(rdlib_llcp_t));
    pLlcp->pGeneralBytes = g_memdup(pGeneralBytes, generalBytesSz);
    pLlcp->generalBytesSz = generalBytesSz;
    pLlcp->bDevType = bDevType;
    pLlcp->pslnLlcp = &pHal->rdlib.slnLlcp;

    if( bDevType == PHLN_LLCP_INITIATOR )
    {
		/* Assign the PAL 18092 initiator parameters once LLPC should be activated in initiator mode. */
		pHal->rdlib.slnLlcp.pPalI18092DataParams = &pHal->rdlib.palI18092mPI;
    }
    else
    {
        /* Assign the PAL 18092 target parameters once LLPC should be activated in listen mode. */
    	pHal->rdlib.slnLlcp.pPalI18092DataParams = &pHal->rdlib.palI18092mT;
    }

    pLlcp->pHal = pHal;

    //Run LLCP
    rdlib_llcp(pLlcp);

    // Perform LLCP DeInit procedure to release acquired resources.
    status = phlnLlcp_DeInit(&pHal->rdlib.slnLlcp);
    CHECK_STATUS(status);

    if (PH_ERR_SUCCESS != status)
    {
       g_warning("Target Connection Lost\n");
    }

    //rdlib_llcp_reset(pHal);

    return status;
}

//LLCP procedure
void rdlib_llcp(const rdlib_llcp_t* pLlcp)
{
    phStatus_t status = PH_ERR_SUCCESS;

    status = phlnLlcp_Activate(pLlcp->pslnLlcp,
    		pLlcp->pGeneralBytes,
			pLlcp->generalBytesSz,
			pLlcp->bDevType
        );

    phOsal_Event_Consume(E_PH_OSAL_EVT_LLCP_ACTIVATED, E_PH_OSAL_EVT_SRC_LIB);

    if (!(((status & PH_ERR_MASK) == PH_ERR_LLCP_DEACTIVATED) ||
        ((status & PH_ERR_MASK) == PH_ERR_PEER_DISCONNECTED)))
    {
        if ((status & PH_ERR_MASK) == PH_ERR_ABORTED)
        {
        	//Nothing
        }
        else if ((status & PH_ERR_MASK) == PH_ERR_EXT_RF_ERROR)
        {
            g_warning("LLCP exited because of external RF Off. \n");
        }
        else if ((status & PH_ERR_MASK) == PH_ERR_RF_ERROR)
        {
        	g_warning("LLCP exited because of active RF error. \n");
        }
        else
        {
        	g_warning("\n LLCP exited unexpectedly:");
            //PrintErrorInfo(status);
        }
    }

    //Unblock all sockets to disconnect SNEP sessions
    phlnLlcp_Sw_Transport_Socket_UnblockAll(pLlcp->pslnLlcp);
}


phStatus_t rdlib_llcp_reset(hal_impl_t* pHal)
{
    phStatus_t status = PH_ERR_SUCCESS;

    pHal->rdlib.slnLlcp.sLocalLMParams.wMiu = 0x00; /* 128 bytes only */
    pHal->rdlib.slnLlcp.sLocalLMParams.wWks = 0x11; /* SNEP & LLCP */
    pHal->rdlib.slnLlcp.sLocalLMParams.bLto = 100; /* Maximum LTO */
    pHal->rdlib.slnLlcp.sLocalLMParams.bOpt = 0x02;
    pHal->rdlib.slnLlcp.sLocalLMParams.bAvailableTlv = PHLN_LLCP_TLV_MIUX_MASK | PHLN_LLCP_TLV_WKS_MASK |
        PHLN_LLCP_TLV_LTO_MASK | PHLN_LLCP_TLV_OPT_MASK;

    /* Initialize LLCP component */
    pHal->rdlib.bLLCPGBLength = 0;
    status = phlnLlcp_Sw_Init(&pHal->rdlib.slnLlcp, sizeof(phlnLlcp_Sw_DataParams_t), pHal->rdlib.aLLCPGeneralBytes, &pHal->rdlib.bLLCPGBLength);
    CHECK_STATUS(status);

    /* Assign the GI for Type A */
    pHal->rdlib.discLoop.sTypeATargetInfo.sTypeA_P2P.pGi                        = (uint8_t *)pHal->rdlib.aLLCPGeneralBytes;
    pHal->rdlib.discLoop.sTypeATargetInfo.sTypeA_P2P.bGiLength                  = pHal->rdlib.bLLCPGBLength;

    /* Assign the GI for Type F */
    pHal->rdlib.discLoop.sTypeFTargetInfo.sTypeF_P2P.pGi                        = (uint8_t *)pHal->rdlib.aLLCPGeneralBytes;
    pHal->rdlib.discLoop.sTypeFTargetInfo.sTypeF_P2P.bGiLength                  = pHal->rdlib.bLLCPGBLength;

    return status;
}

phStatus_t rdlib_snep_init(hal_impl_t* pHal)
{
    phStatus_t status = PH_ERR_SUCCESS;

    //Create SNEP thread
	rdlib_snep_t* pSnep = g_malloc(sizeof(rdlib_snep_t));
	pSnep->pHal = pHal;
	pSnep->pslnLlcp = &pHal->rdlib.slnLlcp;

	//Spawn a thread
	status = phOsal_Posix_Thread_Create(E_PH_OSAL_EVT_DEST_APP, hal_impl_snep_thread_fn, (gpointer) pSnep);

    return status;
}

void rdlib_snep_server(rdlib_snep_t* pSnep)
{
    phStatus_t status = PH_ERR_SUCCESS;
    phnpSnep_Sw_DataParams_t           snpSnepServer;

    /*
     * SNEP Server application buffer to store received PUT Message.
     * Max SNEP PUT message length that can be accepted will be defined by below array length.
     *  */
    uint8_t                     baSnepAppBuffer[1033];
    uint32_t                    baSnepRxLen = 0;

    /* SNEP Server socket and buffers. */
    phlnLlcp_Transport_Socket_t ServerSocket;
    uint8_t                     bServerRxBuffer[260];

    uint32_t                    dwServerRxBuffLength = sizeof(bServerRxBuffer);
    uint32_t                    baSnepAppBufSize = sizeof(baSnepAppBuffer) - 1;
    uint8_t                     bClientReq;

	baSnepRxLen = 0;

	/* Initialize SNEP server component. */
	status = phnpSnep_Sw_Init(&snpSnepServer, sizeof(phnpSnep_Sw_DataParams_t), &pSnep->pslnLlcp, &ServerSocket);
	CHECK_STATUS(status);

	/* Perform server initialization to handle remote SNEP client connection. */
	status = phnpSnep_ServerInit(&snpSnepServer, phnpSnep_Default_Server, NULL, bServerRxBuffer, dwServerRxBuffLength);
	if (status == PH_ERR_SUCCESS)
	{
		do
		{
			/* Handle client PUT request. */
			status = phnpSnep_ServerListen(&snpSnepServer, 0, NULL, NULL, &bClientReq);

			if (status == PH_ERR_SUCCESS)
			{
				status = phnpSnep_ServerSendResponse(&snpSnepServer, bClientReq, NULL, 0, baSnepAppBufSize, baSnepAppBuffer, &baSnepRxLen);

				if (baSnepRxLen > 0)
				{
					/* Process only if server received PUT message of length greater than 0 bytes. */

					//Call callback
					hal_impl_snep_ndef_received_cb(pSnep->pHal, baSnepAppBuffer, baSnepRxLen);
				}
			}
		}while(!status);

		if (!(((status & PH_ERR_MASK) == PH_ERR_PEER_DISCONNECTED) ||
			((status & PH_ERR_MASK) == PH_ERR_LLCP_DEACTIVATED)) ||
			((status & PH_ERR_MASK) == PH_ERR_SUCCESS))
		{
			CHECK_STATUS(status);
		}
		else
		{

		}
	}
	else
	{
		/* Server initialization is un-successful as peer did not send CONNECT PDU. */
	}

	/* Perform server de-init. */
	status = phnpSnep_ServerDeInit(&snpSnepServer);
	CHECK_STATUS(status);
}

void rdlib_snep_client(rdlib_snep_t* pSnep, rdlib_snep_client_msg_t* pMsg)
{
	/* SNEP Client socket and buffers. */
	phlnLlcp_Transport_Socket_t ClientSocket;
	uint8_t                     bClientRxBuffer[260];
	uint32_t                    dwClientRxBuffLength = sizeof(bClientRxBuffer);

    phStatus_t status    = 0;
    phnpSnep_Sw_DataParams_t           snpSnepClient;              /* SNEP component holder */

	/* Initialize SNEP client component */
	status = phnpSnep_Sw_Init(&snpSnepClient, sizeof(snpSnepClient), &pSnep->pslnLlcp, &ClientSocket);
	CHECK_STATUS(status);

	/* Perform SNEP client Initialization and connect to remote SNEP server. */
	status = phnpSnep_ClientInit(&snpSnepClient, phnpSnep_Default_Server, NULL, bClientRxBuffer, dwClientRxBuffLength);
	if (status == PH_ERR_SUCCESS)
	{
		status = phnpSnep_Put(&snpSnepClient, pMsg->msg, pMsg->msgSz);
		ClientSocket.fReady = true;

		if ((status & PH_ERR_MASK) == PH_ERR_SUCCESS)
		{
			status = phnpSnep_ClientDeInit(&snpSnepClient);
			CHECK_STATUS(status);
		}
		else
		{
			status = phlnLlcp_Transport_Socket_Unregister(snpSnepClient.plnLlcpDataParams, snpSnepClient.psSocket);
			CHECK_STATUS(status);
		}
	}
	else
	{
		/* Client initialization is un-successful as failed to connect to remote server.
		 * Release RTOS memory by performing socket unregister. */
		status = phlnLlcp_Transport_Socket_Unregister(snpSnepClient.plnLlcpDataParams, snpSnepClient.psSocket);
		CHECK_STATUS(status);
	}

}

phStatus_t rdlib_snep_close(hal_impl_t* pHal)
{
    phStatus_t status = PH_ERR_SUCCESS;

    /* Join  thread */
    phOsal_Posix_Thread_Join(E_PH_OSAL_EVT_DEST_APP, NULL);

    return status;
}

phStatus_t rdlib_snep_client_send_message(hal_impl_t* pHal, guint deviceId, guint8* buffer, gsize bufferLength)
{
	g_mutex_lock(&pHal->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHal->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHal->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_warning("Did not find hal_impl_device_t instance of id %d", deviceId);
		return PH_ERR_FAILED;
	}

	rdlib_snep_client_msg_t* pMsg = g_malloc(sizeof(rdlib_snep_client_msg_t));
	pMsg->msg = g_memdup(buffer, bufferLength);
	pMsg->msgSz = bufferLength;

	//Psuh to queue
	g_async_queue_push(pHal->pSnepQueue, pMsg);

    return PH_ERR_SUCCESS;
}

gpointer hal_impl_snep_client_thread_fn(gpointer param)
{
	rdlib_snep_t* pSnep = (rdlib_snep_t*) param;
	hal_impl_t* pHal = pSnep->pHal;

	gpointer pData = g_async_queue_pop(pHal->pSnepQueue);
	if( pData == NULL )
	{
		return NULL;
	}

	rdlib_snep_client_msg_t* pMsg = (rdlib_snep_client_msg_t*) pData;

	if(	pMsg->msg != NULL )
	{
		rdlib_snep_client(pSnep, pMsg);
		g_free(pMsg->msg);
	}

	g_free(pMsg);

	//Clear queue
	while( true )
	{
		gpointer pData = g_async_queue_try_pop(pHal->pSnepQueue);
		if(pData == NULL)
		{
			break;
		}

		pMsg = (rdlib_snep_client_msg_t*) pData;
		if(	pMsg->msg != NULL )
		{
			g_free(pMsg->msg);
		}
		g_free(pMsg);
	}

	return NULL;
}

gpointer hal_impl_snep_server_thread_fn(gpointer param)
{
	rdlib_snep_t* pSnep = (rdlib_snep_t*) param;
	hal_impl_t* pHal = pSnep->pHal;

	rdlib_snep_server(pSnep);

	return NULL;
}

gpointer hal_impl_snep_thread_fn(gpointer param)
{
	rdlib_snep_t* pSnep = (rdlib_snep_t*) param;

	phStatus_t status;
	pthread_t snepClientThread;
	pthread_t snepServerThread;

	while (!pSnep->pHal->joining)
	{
		/* Wait until LLCP activation is complete. */
		status = phlnLlcp_WaitForActivation(&pSnep->pHal->rdlib.slnLlcp);
		if( status )
		{
			continue;
		}

		/* Cretae the SNEP Server and SNEP Client tasks*/
		status = phOsal_Posix_Thread_Create_Extra(&snepServerThread, hal_impl_snep_server_thread_fn, pSnep);
		if(status)
		{
			return NULL;
		}

		status = phOsal_Posix_Thread_Create_Extra(&snepClientThread, hal_impl_snep_client_thread_fn, pSnep);
		if(status)
		{
			return NULL;
		}

		phOsal_Posix_Thread_Join_Extra(&snepServerThread, NULL);

		//Force Client to join when server has joined
		rdlib_snep_client_msg_t* pMsg = g_malloc(sizeof(rdlib_snep_client_msg_t));
		pMsg->msg = NULL;
		pMsg->msgSz = 0;

		//Psuh to queue
		g_async_queue_push(pSnep->pHal->pSnepQueue, (gpointer)pMsg);

		phOsal_Posix_Thread_Join_Extra(&snepClientThread, NULL);
		phOsal_Event_Consume(E_PH_OSAL_EVT_LLCP_ACTIVATED, E_PH_OSAL_EVT_SRC_LIB);
	}

	//Clear queue


	g_free(pSnep);

	return NULL;
}

void hal_impl_snep_ndef_received_cb(hal_impl_t* pHal, guint8* buffer, gsize length)
{
	guint deviceId = pHal->session.currentDeviceId;

	g_mutex_lock(&pHal->deviceTableMutex);
	hal_impl_device_t* pDevice = g_hash_table_lookup(pHal->pDeviceTable, GUINT_TO_POINTER(deviceId));
	g_mutex_unlock(&pHal->deviceTableMutex);

	if(pDevice == NULL)
	{
		g_error("Did not find hal_impl_device_t instance of id %d", deviceId);
		return;
	}

	g_rec_mutex_lock(&pDevice->mutex);
	if(pDevice->message.buffer != NULL)
	{
		g_free(pDevice->message.buffer);
	}
	pDevice->message.buffer = g_memdup(buffer, length);
	pDevice->message.length = length;
	pDevice->message.size = length;
	g_rec_mutex_unlock(&pDevice->mutex);

	//Advertise it
	hal_impl_call_adapter_on_device_ndef_received(pHal, deviceId);
}
