/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LWIP_MQTT_ID_H__
#define __LWIP_MQTT_ID_H__

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "stdint.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define MQTT_ID_SIZE 4

#define RESET 			"\x1B[0m"
#define BOLD 			"\x1B[1m"      //Bold Text Formula...
#define RESET_COLOR 	"\033[0m"
#define RED_COLOR		"\033[0;31m"
#define GREEN_COLOR		"\033[0;32m"
#define YELLOW_COLOR	"\033[0;33m"
#define BLUE_COLOR		"\033[0;34m"
#define PURPLE_COLOR	"\033[0;35m"
#define CYAN_COLOR		"\033[0;36m"

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

void get_mqtt_id(uint32_t *id);

#endif /* __LWIP_MQTT_ID_H__ */
