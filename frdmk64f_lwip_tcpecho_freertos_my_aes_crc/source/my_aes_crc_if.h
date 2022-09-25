/*
 * my_aes_crc_if.h
 *
 *  Created on: 24 sep 2022
 *      Author: Braulio Romero
 */

#ifndef MY_AES_CRC_IF_H_
#define MY_AES_CRC_IF_H_

#include <my_aes_crc.h>

err_t my_aes_crc_receive(struct netconn *conn, void **dataptr, uint16_t *lenght);

err_t my_aes_crc_send(struct netconn *conn, void *dataptr, uint16_t size);



#endif /* MY_AES_CRC_IF_H_ */
