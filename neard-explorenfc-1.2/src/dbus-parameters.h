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

#ifndef DBUS_PARAMETERS_H_
#define DBUS_PARAMETERS_H_

#define DBUS_BUS_NAME "org.neard"
#define DBUS_ROOT_OBJECT_PATH "/"
#define DBUS_COMMON_OBJECT_PATH "/org/neard" //"/com/nxp/neard_explorenfc" -- Unfortunately NeardAL needs that specific object path
#define DBUS_AGENT_MANAGER_OBJECT_PATH DBUS_COMMON_OBJECT_PATH

#define DBUS_ADAPTER_OBJECT_PATH "/nfc%d"
#define DBUS_DEVICE_OBJECT_PATH "/device%d"
#define DBUS_TAG_OBJECT_PATH "/tag%d"
#define DBUS_RECORD_OBJECT_PATH "/record%d"

#define DBUS_ADAPTER_INTERFACE_NAME "org.neard.Adapter"
#define DBUS_TAG_INTERFACE_NAME "org.neard.Tag"

#define DBUS_ADAPTER_METHOD_START_POLLING_LOOP "StartPollLoop"
#define DBUS_ADAPTER_METHOD_STOP_POLLING_LOOP "StopPollLoop"

#define DBUS_ADAPTER_PROPERTY_MODE 			"Mode"
#define DBUS_ADAPTER_PROPERTY_POWERED 		"Powered"
#define DBUS_ADAPTER_PROPERTY_POLLING 		"Polling"
#define DBUS_ADAPTER_PROPERTY_PROTOCOLS		"Protocols"

#define DBUS_ERROR_NOT_SUPPORTED "org.neard.Error.NotSupported"

#endif /* DBUS_PARAMETERS_H_ */
