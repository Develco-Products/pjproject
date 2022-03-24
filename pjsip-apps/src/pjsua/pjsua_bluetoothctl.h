#ifndef _PJSUA_BLUETOOTHCTL_H_
#define _PJSUA_BLUETOOTHCTL_H_

#include "pjsua_app_common.h"

pj_status_t bluetoothctl_controller_status(char* device_status, pj_ssize_t buffer_size);
pj_status_t bluetoothctl_device_status(char* device_status, pj_ssize_t buffer_size);

#endif /* _PJSUA_BLUETOOTHCTL_H_ */

