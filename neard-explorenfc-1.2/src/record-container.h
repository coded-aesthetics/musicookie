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
 * \file record-container.h
 */
/** \defgroup RecordContainerGp Record Container
 * A parent class implemented by device and tag (they can both contain records)
 *  @{
 */

#ifndef RECORD_CONTAINER_H_
#define RECORD_CONTAINER_H_

//DBUS-accessible methods
#include <glib.h>
#include <glib-object.h>

#include "generated-code.h"

struct adapter;
typedef struct adapter Adapter;

GType record_container_get_type();

#define TYPE_RECORD_CONTAINER   (record_container_get_type               ())
#define RECORD_CONTAINER(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_RECORD_CONTAINER, RecordContainer))
#define RECORD_CONTAINER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_RECORD_CONTAINER, RecordContainerClass))
#define RECORD_CONTAINER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_RECORD_CONTAINER, RecordContainerClass))

/** Record Container
 * This is a parent class implemented by device and tag (they can both contain records)
 */
struct record_container
{
	GObject parentInstance; ///< GObject instance

	gchar* objectPath; ///< Object path
	Adapter* pAdapter; ///< Reference to containing Adapter
};
typedef struct record_container RecordContainer; ///< Record Container

typedef GObjectClass RecordContainerClass; ///< Record Container Class

#endif /* RECORD_CONTAINER_H_ */

/**
 * @}
 * */
