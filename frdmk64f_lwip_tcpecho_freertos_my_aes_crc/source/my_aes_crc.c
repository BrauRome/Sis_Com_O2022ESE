/*
 * my_aes_crc.c
 *
 *  Created on: 24 sep 2022
 *      Author: Braulio Romero
 */

#include <my_aes_crc_if.h>
#include <my_aes_crc_config.h>
#include <my_aes_crc.h>


/* CRC data */
CRC_Type *base = CRC0;


static void InitCrc32(CRC_Type *base, uint32_t seed)
{
    crc_config_t config;

    config.polynomial         = 0x04C11DB7U;
    config.seed               = seed;
    config.reflectIn          = true;
    config.reflectOut         = true;
    config.complementChecksum = true;
    config.crcBits            = kCrcBits32;
    config.crcResult          = kCrcFinalChecksum;

    CRC_Init(base, &config);
}

/**
 * @ingroup my_aes_crc
	 * Receive data (in form of a netbuf containing a packet buffer) from a netconn
	 * 				check CRC and decrypt data.
 *
 * @param conn the netconn from which to receive data
 * @param dataptr pointer where a new netbuf is stored when received data
 * 				have been decrypted and CRC substracted
 * @param lenght lenght of the application decrypted data to send
 * @return ERR_OK if data has been received, an error code otherwise (timeout,
 *                memory error or another error)
 */
err_t
my_aes_crc_receive(struct netconn *conn, void **dataptr, uint16_t *lenght)
{
	err_t err;
	void *data;
    u16_t len;
    char * msg;

	uint32_t checksum32;
	uint16_t padded_len;
	void *ptr_padded_msg;
    int32_t crc = 0;
    struct AES_ctx ctx;
    struct netbuf *buf;
    u16_t index = 0;

    PRINTF("\r\n%s%s****--RECEIVING--****%s%s\r\n",BLUE_COLOR,BOLD,RESET,RESET_COLOR);
    err = netconn_recv(conn, &buf);

    //PRINTF("tcpecho: netconn state: \"%d\"\r\n", conn->state);
    if(err == ERR_OK){
    	do {
			 netbuf_data(buf, &data, &len);
			 ptr_padded_msg = malloc(len*sizeof(uint8_t));
			 memcpy(ptr_padded_msg+index,data,len);
			 index+=len;

		} while (netbuf_next(buf) >= 0);
		netbuf_delete(buf);
		PRINTF("%sReceived Message Lenght:%s%s %d%s\r\n",BOLD,RESET,CYAN_COLOR,len,RESET_COLOR);
		msg = (char *)ptr_padded_msg;
		PRINTF("%sReceived Message: %s%s",BOLD,RESET,PURPLE_COLOR);
		for(uint16_t i=0; i<len; i++) {
			PRINTF("0x%02x,", *msg++);
		}
		PRINTF("%s\r\n",RESET_COLOR);
		//PRINTF("ptr_padded_msg: 0x%x\r\n",ptr_padded_msg);
		//PRINTF("tcpecho: netconn state: \"%d\"\r\n", conn->state);
		padded_len = index - sizeof(uint32_t);

		//memcpy(padded_msg, test_string, len);

		memcpy(&crc,ptr_padded_msg+padded_len,sizeof(uint32_t));
		crc = lwip_htonl(crc);
		PRINTF("%sCRC32 extract:%s %s0x%08x%s\r\n",BOLD,RESET,CYAN_COLOR,crc,RESET_COLOR);

		InitCrc32(base, SEED);
		CRC_WriteData(base, (uint8_t *)ptr_padded_msg,padded_len);
		checksum32 = CRC_Get32bitResult(base);
		checksum32 = lwip_htonl(checksum32);
		if (checksum32 != crc){
			PRINTF("%s%sRC32 Check fail.%s Expected: 0x%x%s\r\n",RED_COLOR,BOLD,RESET,crc,RESET_COLOR);
			err = ERR_ARG;
		}
		else{
			PRINTF("%s%sCRC32 Check OK.%s%s\r\n",GREEN_COLOR,BOLD,RESET,RESET_COLOR);
			/* Init the AES context structure */
			AES_init_ctx_iv(&ctx, KEY, IV);
			AES_CBC_decrypt_buffer(&ctx, (uint8_t *)ptr_padded_msg, padded_len);

			msg = (char *)ptr_padded_msg;
			PRINTF("%sDecrypted Message:%s %s",BOLD,RESET, CYAN_COLOR);
			for(uint16_t i=0; i<padded_len; i++) {
				PRINTF("%c", *msg++);
			}
			PRINTF("%s\r\n",RESET_COLOR);
			PRINTF("%sDecrypted Message Lenght:%s%s%d %s\r\n ",BOLD, RESET,CYAN_COLOR,padded_len,RESET_COLOR);
			*dataptr = ptr_padded_msg;
			*lenght = padded_len;
			err = ERR_OK;
		}

		CRC_Deinit(base);
		free(ptr_padded_msg);
    }
    else
    {
    	PRINTF("%tcpecho: netconn_recv: error%s \"%s\"\r\n",RED_COLOR,RESET_COLOR, lwip_strerr(err));
    }


	return err;
}

/**
 * @ingroup my_aes_crc
 * Encrypt data and add CRC32 bytes to the buffer and send it over a TCP netconn.
 *
 * @param conn the TCP netconn over which to send data
 * @param dataptr pointer to the application buffer that contains the data to send
 * @param size size of the application data to send
 * @return ERR_OK if data was sent, any other err_t on error
 */
err_t
my_aes_crc_send(struct netconn *conn, void *dataptr, uint16_t  size)
{
	err_t err;
	struct AES_ctx ctx;
	void * ptr_padded_msg_to_send;
	uint32_t checksum32, checksum32_htonl;
	void *crc_ptr;
	uint16_t padded_len;
    char * msg;

    PRINTF("\r\n%s%s****--SENDING--****%s%s\r\n",BLUE_COLOR,BOLD,RESET, RESET_COLOR);
    //PRINTF("Send ptr_padded_msg: 0x%x\r\n", dataptr);
    //PRINTF("Send ptr_padded_msg_len: %d\r\n", size);
    //PRINTF("\r\ntcpecho: netconn state: \"%d\"\r\n", conn->state);

    PRINTF("%sMessage to Encrypt Lenght:%s %s%d%s\r\n",BOLD, RESET,GREEN_COLOR,size,RESET_COLOR);
	msg = (char*)dataptr;
	PRINTF("%sMessage to Encrypt:%s %s",BOLD, RESET, GREEN_COLOR);
	for(uint16_t i=0; i<size; i++) {
		PRINTF("%c", *msg++);
	}
	PRINTF("%s\r\n",RESET_COLOR);

	/* Init the AES context structure */
	AES_init_ctx_iv(&ctx, KEY, IV);

	 /* To encrypt an array its lenght must be a multiple of 16 so we add zeros */
	if((size%16) > 0)
	{
		padded_len = size + (16 - (size%16) );
	}
	else
	{
		padded_len = size;
	}
	ptr_padded_msg_to_send = malloc((padded_len*sizeof(uint8_t))+(sizeof(uint32_t)));
	memcpy(ptr_padded_msg_to_send, dataptr, size);

	AES_CBC_encrypt_buffer(&ctx, (uint8_t*) ptr_padded_msg_to_send, padded_len);
	msg = (char*)ptr_padded_msg_to_send;
	PRINTF("%sEncrypted Message:%s %s",BOLD, RESET,PURPLE_COLOR);
	for(uint16_t i=0; i<padded_len; i++) {
		PRINTF("0x%02x,", *msg++);
	}
	PRINTF("%s\r\n",RESET_COLOR);

	/* Init CRC32 */
	InitCrc32(base, SEED);

	CRC_WriteData(base, (uint8_t *)ptr_padded_msg_to_send, padded_len);
	checksum32 = CRC_Get32bitResult(base);
	//PRINTF("\nTesting CRC32 0x%08x\r\n",checksum32);
	checksum32_htonl = lwip_htonl(checksum32);
	PRINTF("%sCRC-32:%s %s0x%08x%s\r\n",BOLD, RESET,GREEN_COLOR, checksum32_htonl, RESET_COLOR);
	crc_ptr = &checksum32;
	memcpy(ptr_padded_msg_to_send+padded_len,crc_ptr,sizeof(uint32_t));

	padded_len += sizeof(uint32_t);

	PRINTF("%sMessage to Send Lenght:%s %s%d%s\r\n",BOLD, RESET,GREEN_COLOR,padded_len,RESET_COLOR);
	msg = (char*)ptr_padded_msg_to_send;
	PRINTF("%sMessage to Send:%s %s",BOLD, RESET,PURPLE_COLOR);
	for(uint16_t i=0; i<(padded_len); i++) {
		PRINTF("0x%02x,", *msg++);
	}
	PRINTF("%s\r\n",RESET_COLOR);
	//err = netconn_err(conn);
	//PRINTF("tcpecho: netconn error: \"%s\"\r\n", lwip_strerr(err));
	//PRINTF("tcpecho: netconn state: \"%d\"\r\n", conn->state);

	err = netconn_write(conn, ptr_padded_msg_to_send,padded_len, NETCONN_COPY);
	if (err != ERR_OK) {
	  PRINTF("tcpecho: netconn_write: error \"%s\"\r\n", lwip_strerr(err));
	}
	else
	{
	  PRINTF("%s%sMessage Sent%s%s\r\n",GREEN_COLOR,BOLD,RESET,RESET_COLOR);
	  err = ERR_OK;
	}

	CRC_Deinit(base);
	free(ptr_padded_msg_to_send);

	return err;

}
