/*
 * my_aes_crc.h
 *
 *  Created on: 24 sep 2022
 *      Author: Braulio Romero
 */

#ifndef MY_AES_CRC_H_
#define MY_AES_CRC_H_

#include <my_aes_crc_config.h>
#include <my_aes_crc_if.h>

#include "lwip/opt.h"

#include <aes.h>
#include <fsl_crc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/api.h"
#include "lwip/memp.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/priv/api_msg.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/priv/tcpip_priv.h"

#define RESET 			"\x1B[0m"
#define BOLD 			"\x1B[1m"      //Bold Text Formula...
#define RESET_COLOR 	"\033[0m"
#define RED_COLOR		"\033[0;31m"
#define GREEN_COLOR		"\033[0;32m"
#define YELLOW_COLOR	"\033[0;33m"
#define BLUE_COLOR		"\033[0;34m"
#define PURPLE_COLOR	"\033[0;35m"
#define CYAN_COLOR		"\033[0;36m"


static void InitCrc32(CRC_Type *base, uint32_t seed);


#endif /* MY_AES_CRC_H_ */
