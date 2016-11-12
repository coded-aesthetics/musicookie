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

int hal_impl_tag_new(hal_impl_t* pHal, hal_impl_nfc_type_t nfcType, guint* pTagId)
{
	hal_impl_tag_t* pTag = g_malloc(sizeof(hal_impl_tag_t));

	//Reset Top parameters
	phalTop_Reset(&pHal->rdlib.tagop);

	//Initialize everything
	pTag->status = hal_impl_nfc_ndef_status_invalid;

	pTag->message.buffer = NULL;
	pTag->message.length = 0;
	pTag->message.size = 0;

	pTag->type = nfcType;

	uint16_t tagType;

	switch(	nfcType )
	{
	case hal_impl_nfc_tag_type_1:
		tagType = PHAL_TOP_TAG_TYPE_T1T_TAG;
		break;
	case hal_impl_nfc_tag_type_2:
		tagType = PHAL_TOP_TAG_TYPE_T2T_TAG;
		break;
	case hal_impl_nfc_tag_type_3:
		tagType = PHAL_TOP_TAG_TYPE_T3T_TAG;
		break;
	case hal_impl_nfc_tag_type_4a:
		tagType = PHAL_TOP_TAG_TYPE_T4T_TAG;
		break;
	default:
		g_free(pTag);
		return PH_ERR_INVALID_PARAMETER;
	}

	//If tag is ISO14443A-compliant
	if( HAL_IMPL_NFC_TYPE_IS_TAG_ISO14443A(nfcType) )
	{
		memcpy(pTag->iso14443a.atqa, pHal->rdlib.discLoop.sTypeATargetInfo.aTypeA_I3P3[0].aAtqa, 2);
		pTag->iso14443a.sak = pHal->rdlib.discLoop.sTypeATargetInfo.aTypeA_I3P3[0].aSak;
		memcpy(pTag->iso14443a.uid, pHal->rdlib.discLoop.sTypeATargetInfo.aTypeA_I3P3[0].aUid, 10);
		pTag->iso14443a.uidLength = pHal->rdlib.discLoop.sTypeATargetInfo.aTypeA_I3P3[0].bUidSize;
	}
	else
	{
		memset(&pTag->iso14443a, 0, sizeof(hal_impl_nfc_iso14443a_params_t));
	}

	//If tag is ISO14443A-compliant
	if( HAL_IMPL_NFC_TYPE_IS_TAG_FELICA(nfcType) )
	{
		memcpy(pTag->felica.manufacturer, &pHal->rdlib.discLoop.sTypeFTargetInfo.aTypeFTag[0].aIDmPMm[0], 2);
		memcpy(pTag->felica.cid, &pHal->rdlib.discLoop.sTypeFTargetInfo.aTypeFTag[0].aIDmPMm[2], 6);
		memcpy(pTag->felica.ic, &pHal->rdlib.discLoop.sTypeFTargetInfo.aTypeFTag[0].aIDmPMm[2+6], 2);
		memcpy(pTag->felica.maxRespTimes, &pHal->rdlib.discLoop.sTypeFTargetInfo.aTypeFTag[0].aIDmPMm[2+6+2], 6);
	}
	else
	{
		memset(&pTag->felica, 0, sizeof(hal_impl_nfc_felica_params_t));
	}

	//Set tag parameters
	phalTop_SetConfig(&pHal->rdlib.tagop, PHAL_TOP_CONFIG_TAG_TYPE, tagType);

	//Check if a NDEF message is there
	uint8_t value = 0;
	phStatus_t status = phalTop_CheckNdef(&pHal->rdlib.tagop, &value);
	if((status != PH_ERR_SUCCESS) && 
	(status & PH_ERR_MASK) != PHAL_TOP_ERR_NON_NDEF_TAG && 
	(status & PH_ERR_MASK) != PHAL_TOP_ERR_MISCONFIGURED_TAG)
	{
		g_warning("phalTop_CheckNdef() returned %04X\n", status);
		pTag->status = hal_impl_nfc_ndef_status_invalid;
	}
	else
	{
		//Is Tag in R/W or RO mode?
		if( value == PHAL_TOP_STATE_READWRITE  )
		{
			pTag->status = hal_impl_nfc_ndef_status_readwrite;
		}
		else if( value ==  PHAL_TOP_STATE_READONLY )
		{
			pTag->status = hal_impl_nfc_ndef_status_readonly;
		}
		else
		{
			pTag->status = hal_impl_nfc_ndef_status_formattable;
		}
	}


	if( pTag->status != hal_impl_nfc_ndef_status_invalid )
	{
		uint16_t value;

		phalTop_GetConfig(&pHal->rdlib.tagop, PHAL_TOP_CONFIG_MAX_NDEF_LENGTH, &value);

		pTag->message.size = (gsize)value;
		//pTag->message.buffer = g_malloc(pTag->message.size);
	}

	switch(pTag->status)
	{
	case hal_impl_nfc_ndef_status_readwrite:
		g_info("Tag is in read/write mode");
		break;
	case hal_impl_nfc_ndef_status_formattable:
		g_info("Tag is formattable");
		break;
	case hal_impl_nfc_ndef_status_readonly:
		g_info("Tag is in read only mode");
		break;
	case hal_impl_nfc_ndef_status_invalid:
	default:
		g_info("Tag is invalid");
		break;
	}

	//Init other fields from tag
	pTag->connected = TRUE;
	pTag->refs = 1; //1 reference
	g_rec_mutex_init(&pTag->mutex);

	//Find a free ID in the hash table and add the tag
	g_mutex_lock(&pHal->tagTableMutex);

	pTag->id = 0;

	while(g_hash_table_contains(pHal->pTagTable, GUINT_TO_POINTER(pTag->id)))
	{
		pTag->id++;
	}

	*pTagId = pTag->id;

	g_debug("New tag id %d", pTag->id);

	g_hash_table_insert(pHal->pTagTable, GUINT_TO_POINTER(pTag->id), (gpointer*)pTag);

	g_mutex_unlock(&pHal->tagTableMutex);

	return 0;
}

void hal_tag_ref(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return;
	}
	g_rec_mutex_lock(&pTag->mutex);
	pTag->refs++;
	g_rec_mutex_unlock(&pTag->mutex);
}

void hal_tag_unref(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return;
	}
	g_rec_mutex_lock(&pTag->mutex);
	guint refs = --pTag->refs;
	g_rec_mutex_unlock(&pTag->mutex);
	if(refs == 0)
	{
		//No one is using the tag anymore, so free it
		g_mutex_lock(&pHalImpl->tagTableMutex);
		g_hash_table_remove(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
		g_mutex_unlock(&pHalImpl->tagTableMutex);

		if(pTag->message.buffer != NULL)
		{
			g_free(pTag->message.buffer);
		}

		g_free(pTag);
	}
}

nfc_tag_type_t hal_tag_get_type(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return nfc_tag_type_1;
	}

	g_rec_mutex_lock(&pTag->mutex);
	hal_impl_nfc_type_t type = pTag->type;
	g_rec_mutex_unlock(&pTag->mutex);

	switch(type)
	{
	case hal_impl_nfc_tag_type_1:
		return nfc_tag_type_1;
	case hal_impl_nfc_tag_type_2:
		return nfc_tag_type_2;
	case hal_impl_nfc_tag_type_3:
		return nfc_tag_type_3;
	case hal_impl_nfc_tag_type_4a:
	default:
		return nfc_tag_type_4;
	}
}

gboolean hal_tag_is_connected(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return FALSE;
	}

	g_rec_mutex_lock(&pTag->mutex);
	gboolean connected = pTag->connected;
	g_rec_mutex_unlock(&pTag->mutex);

	return connected;
}

void hal_tag_get_ndef(hal_t* pHal, guint tagId, guint8** pBuffer, gsize* pBufferLength)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return;
	}

	g_rec_mutex_lock(&pTag->mutex);
	*pBufferLength = pTag->message.length;
	if(pTag->message.buffer != NULL)
	{
		*pBuffer = g_memdup(pTag->message.buffer, pTag->message.length);
	}
	g_rec_mutex_unlock(&pTag->mutex);
}

void hal_tag_write_ndef(hal_t* pHal, guint tagId, guint8* buffer, gsize bufferLength)
{
    hal_impl_t* pHalImpl = (hal_impl_t*)pHal;

    hal_impl_cmd_info_t* pCmdInfo = g_malloc(sizeof(hal_impl_cmd_info_t));
    pCmdInfo->type = HAL_CMD_TAG_NDEF_WRITE;
    pCmdInfo->tagId = tagId;
    pCmdInfo->buffer = g_memdup(buffer, bufferLength);
    pCmdInfo->bufferLength = bufferLength;

	hal_impl_call_cmd(pHalImpl, pCmdInfo);
}

gboolean hal_tag_is_readonly(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return TRUE;
	}

	g_rec_mutex_lock(&pTag->mutex);
	gboolean readonly;
	switch(pTag->status)
	{
	case hal_impl_nfc_ndef_status_readwrite:
	case hal_impl_nfc_ndef_status_formattable:
		readonly = FALSE;
		break;
	case hal_impl_nfc_ndef_status_readonly:
	case hal_impl_nfc_ndef_status_invalid:
	default:
		readonly = TRUE;
		break;
	}

	g_rec_mutex_unlock(&pTag->mutex);

	return readonly;
}

gboolean hal_tag_is_iso14443a(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return FALSE;
	}

	g_rec_mutex_lock(&pTag->mutex);
	gboolean iso14443a = HAL_IMPL_NFC_TYPE_IS_TAG_ISO14443A(pTag->type);
	g_rec_mutex_unlock(&pTag->mutex);

	return iso14443a;
}

void hal_tag_get_iso14443a_params(hal_t* pHal, guint tagId, guint8* atqa, guint8* sak, guint8* uid, gsize* pUidLength)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return;
	}

	g_rec_mutex_lock(&pTag->mutex);
	memcpy(atqa, pTag->iso14443a.atqa, 2);
	*sak = pTag->iso14443a.sak;
	memcpy(uid, pTag->iso14443a.uid, pTag->iso14443a.uidLength);
	*pUidLength = pTag->iso14443a.uidLength;
	g_rec_mutex_unlock(&pTag->mutex);
}

gboolean hal_tag_is_felica(hal_t* pHal, guint tagId)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return FALSE;
	}

	g_rec_mutex_lock(&pTag->mutex);
	gboolean felica = HAL_IMPL_NFC_TYPE_IS_TAG_FELICA(pTag->type);
	g_rec_mutex_unlock(&pTag->mutex);

	return felica;
}

void hal_tag_get_felica_params(hal_t* pHal, guint tagId, guint8* manufacturer, guint8* cid, guint8* ic, guint8* maxRespTimes)
{
	hal_impl_t* pHalImpl = (hal_impl_t*) pHal;

	g_mutex_lock(&pHalImpl->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHalImpl->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHalImpl->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return;
	}

	g_rec_mutex_lock(&pTag->mutex);
	memcpy(manufacturer, pTag->felica.manufacturer, sizeof(pTag->felica.manufacturer));
	memcpy(cid, pTag->felica.cid, sizeof(pTag->felica.cid));
	memcpy(ic, pTag->felica.ic, sizeof(pTag->felica.ic));
	memcpy(maxRespTimes, pTag->felica.maxRespTimes, sizeof(pTag->felica.maxRespTimes));
	g_rec_mutex_unlock(&pTag->mutex);
}

void hal_impl_tag_disconnected(hal_impl_t* pHal, guint tagId)
{
	g_mutex_lock(&pHal->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHal->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHal->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
	}
	g_rec_mutex_lock(&pTag->mutex);
	pTag->connected = FALSE;
	g_rec_mutex_unlock(&pTag->mutex);
}

void hal_impl_tag_ndef_write(hal_impl_t* pHal, guint tagId, guint8* buffer, gsize length)
{
	//Make sure tag won't get destroyed by other thread
	hal_tag_ref((hal_t*)pHal, tagId);

	g_mutex_lock(&pHal->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHal->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHal->tagTableMutex);

	if(pTag == NULL)
	{
		g_warning("Did not find hal_impl_tag_t instance of id %d", tagId);
		return;
	}

	gboolean connected;
	g_rec_mutex_lock(&pTag->mutex);
	connected = pTag->connected;
	g_rec_mutex_unlock(&pTag->mutex);

	if(connected)
	{
		rdlib_tag_ndef_write(pHal, tagId, buffer, length); //TODO callback?
	}
	else
	{
		g_warning("Tag is disconnected\r\n");
	}

	hal_tag_unref((hal_t*)pHal, tagId);
}

phStatus_t rdlib_tag_ndef_read(hal_impl_t* pHal, guint tagId)
{
    phStatus_t    status;

	g_mutex_lock(&pHal->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHal->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHal->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return PH_ERR_FAILED;
	}

    uint16_t length = 0;
    gsize allocSize;

	g_rec_mutex_lock(&pTag->mutex);
	allocSize = pTag->message.size;
	g_rec_mutex_unlock(&pTag->mutex);

	if(allocSize == 0)
	{
		return PH_ERR_SUCCESS; //NDEF message is empty
	}

	uint8_t* buffer = g_malloc(allocSize);

    status = phalTop_ReadNdef(&pHal->rdlib.tagop, buffer, &length);
    if((status & PH_ERR_MASK) == PH_ERR_SUCCESS)
    {
		g_rec_mutex_lock(&pTag->mutex);
		pTag->message.length = length;
		if(pTag->message.buffer != NULL)
		{
			g_free(pTag->message.buffer);
		}
		pTag->message.buffer = buffer;
		g_rec_mutex_unlock(&pTag->mutex);
    }
    if((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
    {
    	g_free(buffer);
    	return status;
    }

	return PH_ERR_SUCCESS;
}

phStatus_t rdlib_tag_ndef_write(hal_impl_t* pHal, guint tagId, guint8* buffer, gsize length)
{
    phStatus_t    status;

	g_mutex_lock(&pHal->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHal->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHal->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return PH_ERR_FAILED;
	}

	g_rec_mutex_lock(&pTag->mutex);
	hal_impl_nfc_ndef_status_t tagStatus = pTag->status;
	g_rec_mutex_unlock(&pTag->mutex);

	if( tagStatus == hal_impl_nfc_ndef_status_formattable )
	{
		//Try to format tag
		status = phalTop_FormatNdef(&pHal->rdlib.tagop);
		if((status & PH_ERR_MASK) != PH_ERR_SUCCESS && ((status & PH_ERR_MASK) != PHAL_TOP_ERR_FORMATTED_TAG))
	    {
	    	g_warning("Could not format tag");
	    }
	    else
	    {
	    	if(status & PH_ERR_MASK == PHAL_TOP_ERR_FORMATTED_TAG)
	    		g_info("Tag already formatted");

	    	g_rec_mutex_lock(&pTag->mutex);
			pTag->status = hal_impl_nfc_ndef_status_readwrite;
			g_rec_mutex_unlock(&pTag->mutex);
			tagStatus = hal_impl_nfc_ndef_status_readwrite;
	    }
	    /* check NDEF must not be performed because it leaves invalid TOP status which avoids subsequent write */
	}


    status = phalTop_WriteNdef(&pHal->rdlib.tagop, buffer, (guint16)length);
    if((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
    {
    	g_warning("Could not write tag");
    	return PH_ERR_FAILED;
    }
    else if(tagStatus != hal_impl_nfc_ndef_status_readwrite)
    {
    	g_rec_mutex_lock(&pTag->mutex);
		pTag->status = hal_impl_nfc_ndef_status_readwrite;
		g_rec_mutex_unlock(&pTag->mutex);
    }

    g_info("Tag written");

	return PH_ERR_SUCCESS;
}

phStatus_t rdlib_tag_presence_check(hal_impl_t* pHal, guint tagId)
{
    phStatus_t    status;

    uint8_t dummy[16];
    uint16_t length = 0;
    uint8_t rxNumBlocks;

    //Felica specific
    uint8_t bMCBlockList[2] = { 0x80, 0x88 }; /* Memory Configuration(MC) Block is 88h */
    uint8_t bReadServiceList[2] = { 0x0B, 0x00};

	g_mutex_lock(&pHal->tagTableMutex);
	hal_impl_tag_t* pTag = g_hash_table_lookup(pHal->pTagTable, GUINT_TO_POINTER(tagId));
	g_mutex_unlock(&pHal->tagTableMutex);

	if(pTag == NULL)
	{
		g_error("Did not find hal_impl_tag_t instance of id %d", tagId);
		return PH_ERR_FAILED;
	}

	g_rec_mutex_lock(&pTag->mutex);
	hal_impl_nfc_type_t type = pTag->type;
	g_rec_mutex_unlock(&pTag->mutex);

	switch(type)
	{
	case hal_impl_nfc_tag_type_1:
		status = phalT1T_ReadByte(&pHal->rdlib.alT1T, pHal->rdlib.t1tparam.bUid, 0x00, dummy, &length);
		break;
	case hal_impl_nfc_tag_type_2:
		status = phalMful_Read(&pHal->rdlib.alMful, 0x03, dummy);
		break;
	case hal_impl_nfc_tag_type_3:
		status = phalFelica_Read(&pHal->rdlib.alFelica, 0x01, bReadServiceList, 0x01,
				bMCBlockList, 0x02, &rxNumBlocks, dummy);
		break;
	case hal_impl_nfc_tag_type_4a:
		status = phpalI14443p4_PresCheck(&pHal->rdlib.palI14443p4);
		break;
	default:
		return PH_ERR_INVALID_PARAMETER;
	}

    if((status & PH_ERR_MASK) != PH_ERR_SUCCESS)
    {
    	return status;
    }

    return PH_ERR_SUCCESS;
}
