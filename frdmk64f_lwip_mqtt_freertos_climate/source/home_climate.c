/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "lwip/opt.h"

#if LWIP_IPV4 && LWIP_RAW && LWIP_NETCONN && LWIP_DHCP && LWIP_DNS

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_phy.h"

#include "lwip/api.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dhcp.h"
#include "lwip/netdb.h"
#include "lwip/netifapi.h"
#include "lwip/prot/dhcp.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "enet_ethernetif.h"
#include "lwip_mqtt_id.h"

#include "ctype.h"
#include "stdio.h"
#include "string.h"

#include "fsl_phyksz8081.h"
#include "fsl_enet_mdio.h"
#include "fsl_device_registers.h"

#include "timers.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* The software timer period. */
#define SW_TIMER_PERIOD_MS (1400 / portTICK_PERIOD_MS)
/* The software timer period. */
#define SW_TIMER1_PERIOD_MS (2000 / portTICK_PERIOD_MS)

#define NUM_TIMERS 2
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void vTimerCallback( TimerHandle_t pxTimer );

/* @TEST_ANCHOR */

/* MAC address configuration. */
#ifndef configMAC_ADDR
#define configMAC_ADDR                     \
    {                                      \
        0x02, 0x12, 0x13, 0x10, 0x15, 0x90 \
    }
#endif

/* Address of PHY interface. */
#define EXAMPLE_PHY_ADDRESS BOARD_ENET0_PHY_ADDRESS

/* MDIO operations. */
#define EXAMPLE_MDIO_OPS enet_ops

/* PHY operations. */
#define EXAMPLE_PHY_OPS phyksz8081_ops

/* ENET clock frequency. */
#define EXAMPLE_CLOCK_FREQ CLOCK_GetFreq(kCLOCK_CoreSysClk)

/* GPIO pin configuration. */
#define BOARD_LED_GPIO       BOARD_LED_RED_GPIO
#define BOARD_LED_GPIO_PIN   BOARD_LED_RED_GPIO_PIN
#define BOARD_SW_GPIO        BOARD_SW3_GPIO
#define BOARD_SW_GPIO_PIN    BOARD_SW3_GPIO_PIN
#define BOARD_SW_PORT        BOARD_SW3_PORT
#define BOARD_SW_IRQ         BOARD_SW3_IRQ
#define BOARD_SW_IRQ_HANDLER BOARD_SW3_IRQ_HANDLER
#define BOARD_SW_NAME        BOARD_SW3_NAME


#ifndef EXAMPLE_NETIF_INIT_FN
/*! @brief Network interface initialization function. */
#define EXAMPLE_NETIF_INIT_FN ethernetif0_init
#endif /* EXAMPLE_NETIF_INIT_FN */

/*! @brief MQTT server host name or IP address. */
#define EXAMPLE_MQTT_SERVER_HOST "driver.cloudmqtt.com"

/*! @brief MQTT server port number. */
#define EXAMPLE_MQTT_SERVER_PORT 18591

/*! @brief Stack size of the temporary lwIP initialization thread. */
#define INIT_THREAD_STACKSIZE 1024

/*! @brief Priority of the temporary lwIP initialization thread. */
#define INIT_THREAD_PRIO DEFAULT_THREAD_PRIO

/*! @brief Stack size of the temporary initialization thread. */
#define APP_THREAD_STACKSIZE 1024

/*! @brief Priority of the temporary initialization thread. */
#define APP_THREAD_PRIO DEFAULT_THREAD_PRIO

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void connect_to_mqtt(void *ctx);

/*******************************************************************************
 * Variables
 ******************************************************************************/

static mdio_handle_t mdioHandle = {.ops = &EXAMPLE_MDIO_OPS};
static phy_handle_t phyHandle   = {.phyAddr = EXAMPLE_PHY_ADDRESS, .mdioHandle = &mdioHandle, .ops = &EXAMPLE_PHY_OPS};

/*! @brief MQTT client data. */
static mqtt_client_t *mqtt_client;

/*! @brief MQTT client ID string. */
static char client_id[40];

/*! @brief MQTT client information. */
static const struct mqtt_connect_client_info_t mqtt_client_info = {
    .client_id   = (const char *)&client_id[0],
    .client_user = "braulio",
    .client_pass = "hola",
    .keep_alive  = 100,
    .will_topic  = NULL,
    .will_msg    = NULL,
    .will_qos    = 0,
    .will_retain = 0,
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    .tls_config = NULL,
#endif
};

/*! @brief MQTT broker IP address. */
static ip_addr_t mqtt_addr;

/*! @brief Indicates connection to MQTT broker. */
static volatile bool connected = false;

static int count = 0;
static int my_delay = 0;
static int my_on_off = 0;
static bool flag_on_off_room1 = false;
static bool flag_desired_temp_room1 = false;
static bool flag_speed_room1 = false;
static bool flag_on_off_room2 = false;
static bool flag_desired_temp_room2 = false;
static bool flag_speed_room2 = false;
static int on_off[2] = {0};
static int led_blue[2] = {0};
static int led_red[2] = {0};
static int temperature[2] = {0};
static int speed[2] = {1,1};
static int trgt_temperature[2] = {0};
static int step[2] = {0};
static int prev_led_blue[2] = {0};
static int prev_led_red[2] = {0};
static int prev_temperature[2] = {0};
static int prev_speed[2] = {1};
static int prev_trgt_temperature[2] = {0};
static int prev_step[2] = {0};

static int timers_ticks[2] = {2800, 3200};

volatile bool g_ButtonPress = false;

TimerHandle_t SwTimerHandle = NULL;
TimerHandle_t SwTimerHandle1 = NULL;

TimerHandle_t xTimers[ NUM_TIMERS ];
int32_t lExpireCounters[ NUM_TIMERS ] = { 0 };
/*******************************************************************************
 * Code
 ******************************************************************************/

void BOARD_SW_IRQ_HANDLER(void)
{
    /* Clear external interrupt flag. */
    GPIO_PortClearInterruptFlags(BOARD_SW_GPIO, 1U << BOARD_SW_GPIO_PIN);

    /* Change state of button. */
    g_ButtonPress = true;
    SDK_ISR_EXIT_BARRIER;
}

static void init_button_and_led(void)
{
    /* Define the init structure for the input switch pin */
    gpio_pin_config_t sw_config = {
        kGPIO_DigitalInput,
        0,
    };

    /* Define the init structure for the output LED pin */
    gpio_pin_config_t led_config = {
        kGPIO_DigitalOutput,
        0,
    };

    /* Print a note to terminal. */
    PRINTF("\r\n Init SW3 and LED pins\r\n");


    PORT_SetPinInterruptConfig(BOARD_SW_PORT, BOARD_SW_GPIO_PIN, kPORT_InterruptFallingEdge);
    EnableIRQ(BOARD_SW_IRQ);
    GPIO_PinInit(BOARD_SW_GPIO, BOARD_SW_GPIO_PIN, &sw_config);

    /* Init output LED GPIO. */
    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &led_config);
    GPIO_PinInit(BOARD_LED_BLUE_GPIO, BOARD_LED_BLUE_GPIO_PIN, &led_config);
}

/*!
 * @brief Called when subscription request finishes.
 */
static void mqtt_topic_subscribed_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        PRINTF("Subscribed to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to subscribe to the topic \"%s\": %d.\r\n", topic, err);
    }
}

/*!
 * @brief Called when there is a message on a subscribed topic.
 */
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    LWIP_UNUSED_ARG(arg);
    char top[40]={0};

    PRINTF("Received %u bytes from the topic \"%s\": \"", tot_len, topic);

    sprintf(top, "climate/%s/r1/on_off", client_id);
    if((strcmp(top,topic)) == 0){
    	flag_on_off_room1 = true;
    }
    else{
    	sprintf(top, "climate/%s/r1/trgt_temp", client_id);
		if((strcmp(top,topic)) == 0){
			flag_desired_temp_room1 = true;
		}
		else
		{
			sprintf(top, "climate/%s/r1/speed", client_id);
			if((strcmp(top,topic)) == 0){
				flag_speed_room1 = true;
			}
			else
			{
				sprintf(top, "climate/%s/r2/on_off", client_id);
				if((strcmp(top,topic)) == 0){
					flag_on_off_room2 = true;
				}
				else
				{
					sprintf(top, "climate/%s/r2/trgt_temp", client_id);
					if((strcmp(top,topic)) == 0){
						flag_desired_temp_room2 = true;
					}
					else
					{
						sprintf(top, "climate/%s/r2/speed", client_id);
						if((strcmp(top,topic)) == 0){
							flag_speed_room2 = true;
						}
						else
						{
							PRINTF("\r\nNO MATCH!");
						}
					}
				}
			}
		}
    }
}

/*!
 * @brief Called when recieved incoming published message fragment.
 */
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    int i;
    u8_t datos = 0;
    char * msg;
	void *ptr_data;

    LWIP_UNUSED_ARG(arg);

    for (i = 0; i < len; i++)
    {
        if (isprint(data[i]))
        {
            PRINTF("%c", (char)data[i]);
        }
        else
        {
            PRINTF("\\x%02x", data[i]);
        }
    }
    if (flags & MQTT_DATA_FLAG_LAST)
	{
		PRINTF("\"\r\n");
	}

    ptr_data = malloc(len*sizeof(uint8_t));
    memcpy(ptr_data,data,len);

    if(flag_on_off_room1){
    	on_off[0] = atoi(ptr_data);
		PRINTF("\r\n%son_off[0]= %s%d\r\n",BOLD,RESET,on_off[0]);
		flag_on_off_room1 = false;
	}else if(flag_desired_temp_room1){
		trgt_temperature[0] = atoi(ptr_data);
		PRINTF("\r\n%strgt_temperature[0]= %s%d\r\n",BOLD,RESET,trgt_temperature[0]);
		flag_desired_temp_room1 = false;
	}else if(flag_speed_room1){
		speed[0] = atoi(ptr_data);
		PRINTF("\r\n%sspeed[0]= %s%d\r\n",BOLD,RESET,speed[0]);
		flag_speed_room1 = false;
	}else if(flag_on_off_room2){
		on_off[1] = atoi(ptr_data);
		PRINTF("\r\n%son_off[1]= %s%d\r\n",BOLD,RESET,on_off[1]);
		flag_on_off_room2 = false;
	}else if(flag_desired_temp_room2){
		trgt_temperature[1] = atoi(ptr_data);
		PRINTF("\r\n%strgt_temperature[1]= %s%d\r\n",BOLD,RESET,trgt_temperature[1]);
		flag_desired_temp_room2 = false;
	}else if(flag_speed_room2){
		speed[1] = atoi(ptr_data);
		PRINTF("\r\n%sspeed[1]= %s%d%\r\n",BOLD,RESET,speed[1]);
		flag_speed_room2 = false;
	}else{
		//Do nothing
	}


}

/*!
 * @brief Subscribe to MQTT topics.
 */
static void mqtt_subscribe_topics(mqtt_client_t *client)
{
    char topic[40] = {0};
    err_t err;

    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb,
                            LWIP_CONST_CAST(void *, &mqtt_client_info));

	sprintf(topic, "climate/%s/r1/on_off",client_id);
	err = mqtt_subscribe(client, topic, 1, mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, "Room 1 on_off"));
	if (err == ERR_OK) PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topic, 1);
	else PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topic, 1, err);

	sprintf(topic, "climate/%s/r1/trgt_temp",client_id);
	err = mqtt_subscribe(client, topic, 1, mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, "Room 1 desired_temp"));
	if (err == ERR_OK) PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topic, 1);
	else PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topic, 1, err);

	sprintf(topic, "climate/%s/r1/speed",client_id);
	err = mqtt_subscribe(client, topic, 1, mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, "Room 1 Speed"));
	if (err == ERR_OK) PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topic, 1);
	else PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topic, 1, err);

	sprintf(topic, "climate/%s/r2/on_off",client_id);
	err = mqtt_subscribe(client, topic, 1, mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, "Room 2 on_off"));
	if (err == ERR_OK) PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topic, 1);
	else PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topic, 1, err);

	sprintf(topic, "climate/%s/r2/trgt_temp",client_id);
	err = mqtt_subscribe(client, topic, 1, mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, "Room 2 desired_temp"));
	if (err == ERR_OK) PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topic, 1);
	else PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topic, 1, err);

	sprintf(topic, "climate/%s/r2/speed",client_id);
	err = mqtt_subscribe(client, topic, 1, mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, "Room 2 Speed"));
	if (err == ERR_OK) PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topic, 1);
	else PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topic, 1, err);
}

/*!
 * @brief Called when connection state changes.
 */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

    connected = (status == MQTT_CONNECT_ACCEPTED);

    switch (status)
    {
        case MQTT_CONNECT_ACCEPTED:
            PRINTF("MQTT client \"%s\" connected.\r\n", client_info->client_id);
            mqtt_subscribe_topics(client);
            break;

        case MQTT_CONNECT_DISCONNECTED:
            PRINTF("MQTT client \"%s\" not connected.\r\n", client_info->client_id);
            /* Try to reconnect 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_TIMEOUT:
            PRINTF("MQTT client \"%s\" connection timeout.\r\n", client_info->client_id);
            /* Try again 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
        case MQTT_CONNECT_REFUSED_SERVER:
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
            PRINTF("MQTT client \"%s\" connection refused: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;

        default:
            PRINTF("MQTT client \"%s\" connection status: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;
    }
}

/*!
 * @brief Starts connecting to MQTT broker. To be called on tcpip_thread.
 */
static void connect_to_mqtt(void *ctx)
{
    LWIP_UNUSED_ARG(ctx);

    PRINTF("Connecting to MQTT broker at %s...\r\n", ipaddr_ntoa(&mqtt_addr));

    mqtt_client_connect(mqtt_client, &mqtt_addr, EXAMPLE_MQTT_SERVER_PORT, mqtt_connection_cb,
                        LWIP_CONST_CAST(void *, &mqtt_client_info), &mqtt_client_info);
}

/*!
 * @brief Called when publish request finishes.
 */
static void mqtt_message_published_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        PRINTF("Published to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to publish to the topic \"%s\": %d.\r\n", topic, err);
    }
}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_count(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/count", client_id);
    sprintf(message, "%d", count);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"count");
}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_led_blue0(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r1/l_blue", client_id);
    sprintf(message, "%d", led_blue[0]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 1 Blue Led");

}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_led_blue1(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r2/l_blue", client_id);
    sprintf(message, "%d", led_blue[1]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 2 Blue Led");

}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_led_red0(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r1/l_red", client_id);
    sprintf(message, "%d", led_red[0]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 1 Red Led");

}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_led_red1(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r2/l_red", client_id);
    sprintf(message, "%d", led_red[1]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 2 Red Led");

}
/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_temperature0(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r1/temp", client_id);
    sprintf(message, "%d", temperature[0]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 1 temperature");

}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_temperature1(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};

    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r2/temp", client_id);
    sprintf(message, "%d", temperature[1]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 2 temperature");

}


/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_refresh_received(void *ctx)
{
    char topic[40] = {0};
    char message[40] = {0};
    LWIP_UNUSED_ARG(ctx);

    sprintf(topic, "climate/%s/r1/on_off_r",client_id);
	sprintf(message, "%d", on_off[0]);
	PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

	mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 1 On/Off");

	sprintf(topic, "climate/%s/r1/trgt_temp_r",client_id);
	sprintf(message, "%d", trgt_temperature[0]);
	PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

	mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 1 Trgt Temp");

	sprintf(topic, "climate/%s/r1/speed_r",client_id);
	sprintf(message, "%d", speed[0]);
	PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

	mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 1 Speed");

	sprintf(topic, "climate/%s/r2/on_off_r",client_id);
	sprintf(message, "%d", on_off[1]);
	PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

	mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 2 On/Off");

    sprintf(topic, "climate/%s/r2/trgt_temp_r", client_id);
    sprintf(message, "%d", trgt_temperature[1]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 2 Trgt Temp");

    sprintf(topic, "climate/%s/r2/speed_r",client_id);
    sprintf(message, "%d", speed[1]);
    PRINTF("Going to publish to the topic: \"%s\", message:  \"%s\"...\r\n", topic, message);

    mqtt_publish(mqtt_client, topic, message, strlen(message), 1, 0, mqtt_message_published_cb, (void *)"Room 2 Speed");

}

/*!
 * @brief Application thread.
 */
static void app_thread(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    struct dhcp *dhcp;
    err_t err;
    int x = 0;

    static bool turned_on[2];

    /* Wait for address from DHCP */

    PRINTF("Getting IP address from DHCP...\r\n");

    do
    {
        if (netif_is_up(netif))
        {
            dhcp = netif_dhcp_data(netif);
        }
        else
        {
            dhcp = NULL;
        }

        sys_msleep(20U);

    } while ((dhcp == NULL) || (dhcp->state != DHCP_STATE_BOUND));

    PRINTF("\r\nIPv4 Address     : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
    PRINTF("IPv4 Subnet mask : %s\r\n", ipaddr_ntoa(&netif->netmask));
    PRINTF("IPv4 Gateway     : %s\r\n\r\n", ipaddr_ntoa(&netif->gw));

    /*
     * Check if we have an IP address or host name string configured.
     * Could just call netconn_gethostbyname() on both IP address or host name,
     * but we want to print some info if goint to resolve it.
     */

    if (ipaddr_aton(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr) && IP_IS_V4(&mqtt_addr))
    {
        /* Already an IP address */
        err = ERR_OK;
    }
    else
    {
        /* Resolve MQTT broker's host name to an IP address */
        PRINTF("Resolving \"%s\"...\r\n", EXAMPLE_MQTT_SERVER_HOST);
        err = netconn_gethostbyname(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr);
    }

    if (err == ERR_OK)
    {
        /* Start connecting to MQTT broker from tcpip_thread */
        err = tcpip_callback(connect_to_mqtt, NULL);
        if (err != ERR_OK)
        {
            PRINTF("Failed to invoke broker connection on the tcpip_thread: %d.\r\n", err);
        }
    }
    else
    {
        PRINTF("Failed to obtain IP address: %d.\r\n", err);
    }

    init_button_and_led();
    /* Publish some messages */
    for (;;)
    {
        if (connected)
        {

            if (g_ButtonPress)
            {
                PRINTF(" %s is pressed \r\n", BOARD_SW_NAME);
                /* Toggle LED. */
                GPIO_PortToggle(BOARD_LED_GPIO, 1U << BOARD_LED_GPIO_PIN);
                /* Reset state of button. */
                g_ButtonPress = false;
                count++;
                err = tcpip_callback(publish_count, NULL);
                if (err != ERR_OK)
                {
                    PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
                }
            }

            for(int i=0; i<2 ; i++){
            	if(on_off[i] == 1 && !turned_on[i]){
            		if( xTimerStart( xTimers[ i ], 0 ) != pdPASS )
					{
						// The timer could not be set into the Active state.
						PRINTF("\r\nTimer %d could not be set into the Active state\r\n",i);
					}
            		turned_on[i] = true;
            	}else if(on_off[i] == 0 && turned_on[i]){
            		if( xTimerStop( xTimers[ i ], 0 ) != pdPASS )
					{
						// The timer could not be set into the Active state.
						PRINTF("\r\nTimer %d could not be set into the Non Active state\r\n",i);
					}
					turned_on[i] = false;
					led_red[i] = 0;
					led_blue[i] = 0;
            	}
            	else{
            		/* Do nothing */
            	}

            	if(temperature[i] != prev_temperature[i]){
            		if(i == 0)
            			err = tcpip_callback(publish_temperature0, NULL);
            		if(i == 1)
            			err = tcpip_callback(publish_temperature1, NULL);

					if (err != ERR_OK)
					{
						PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
					}
					prev_temperature[i] = temperature[i];
            	}

            	if(led_red[i] != prev_led_red[i]){
            		if(i == 0)
						err = tcpip_callback(publish_led_red0, NULL);
					if(i == 1)
						err = tcpip_callback(publish_led_red1, NULL);

					if (err != ERR_OK)
					{
						PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
					}
					prev_led_red[i] = led_red[i];
					/* Toggle LED. */
					GPIO_PinWrite(BOARD_LED_RED_GPIO,BOARD_LED_RED_GPIO_PIN,led_red[i]);
            	}

            	if(led_blue[i] != prev_led_blue[i]){
					if(i == 0)
						err = tcpip_callback(publish_led_blue0, NULL);
					if(i == 1)
						err = tcpip_callback(publish_led_blue1, NULL);

					if (err != ERR_OK)
					{
						PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
					}
					prev_led_blue[i] = led_blue[i];
					GPIO_PinWrite(BOARD_LED_BLUE_GPIO,BOARD_LED_BLUE_GPIO_PIN,led_red[i]);
				}
            }
            x ++;
			if(x > 100){
				x=0;
				err = tcpip_callback(publish_refresh_received, NULL);
				if (err != ERR_OK)
				{
					PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
				}
			}
        }
        sys_msleep(100U);
    }
    vTaskDelete(NULL);
}

static void generate_client_id(void)
{
    uint32_t mqtt_id[MQTT_ID_SIZE];
    int res;

    get_mqtt_id(&mqtt_id[0]);

    res = snprintf(client_id, sizeof(client_id), "%08lx", mqtt_id[0]);
    if ((res < 0) || (res >= sizeof(client_id)))
    {
        PRINTF("snprintf failed: %d\r\n", res);
        while (1)
        {
        }
    }
}

/*!
 * @brief Initializes lwIP stack.
 *
 * @param arg unused
 */
static void stack_init(void *arg)
{
    static struct netif netif;
    ip4_addr_t netif_ipaddr, netif_netmask, netif_gw;
    ethernetif_config_t enet_config = {
        .phyHandle  = &phyHandle,
        .macAddress = configMAC_ADDR,
    };

    LWIP_UNUSED_ARG(arg);
    generate_client_id();

    mdioHandle.resource.csrClock_Hz = EXAMPLE_CLOCK_FREQ;

    IP4_ADDR(&netif_ipaddr, 0U, 0U, 0U, 0U);
    IP4_ADDR(&netif_netmask, 0U, 0U, 0U, 0U);
    IP4_ADDR(&netif_gw, 0U, 0U, 0U, 0U);

    tcpip_init(NULL, NULL);

    LOCK_TCPIP_CORE();
    mqtt_client = mqtt_client_new();
    UNLOCK_TCPIP_CORE();
    if (mqtt_client == NULL)
    {
        PRINTF("mqtt_client_new() failed.\r\n");
        while (1)
        {
        }
    }

    netifapi_netif_add(&netif, &netif_ipaddr, &netif_netmask, &netif_gw, &enet_config, EXAMPLE_NETIF_INIT_FN,
                       tcpip_input);
    netifapi_netif_set_default(&netif);
    netifapi_netif_set_up(&netif);

    netifapi_dhcp_start(&netif);

    PRINTF("\r\n************************************************\r\n");
    PRINTF(" MQTT client Home_Climate\r\n");
    PRINTF("************************************************\r\n");

    if (sys_thread_new("app_task", app_thread, &netif, APP_THREAD_STACKSIZE, APP_THREAD_PRIO) == NULL)
    {
        LWIP_ASSERT("stack_init(): Task creation failed.", 0);
    }

	//xTimerStart( SwTimerHandle1, 0 );

    vTaskDelete(NULL);
}

/*!
 * @brief Main function
 */
int main(void)
{

    SYSMPU_Type *base = SYSMPU;
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    SystemCoreClockUpdate();
    /* Disable SYSMPU. */
    base->CESR &= ~SYSMPU_CESR_VLD_MASK;

    /* Initialize lwIP from thread */
    if (sys_thread_new("main", stack_init, NULL, INIT_THREAD_STACKSIZE, INIT_THREAD_PRIO) == NULL)
    {
        LWIP_ASSERT("main(): Task creation failed.", 0);
    }

    // Create then start some timers.  Starting the timers before the scheduler
    // has been started means the timers will start running immediately that
    // the scheduler starts.

    for(int x = 0; x < NUM_TIMERS; x++ )
    {
    	xTimers[ x ] = xTimerCreate("Timer",timers_ticks[x],  pdTRUE, ( void * ) x, vTimerCallback);

    	if( xTimers[ x ] == NULL )
		{
			// The timer was not created.
			PRINTF("\r\nTimer %d was not created\r\n",x);
		}
		else
		{
			// Start the timer.  No block time is specified, and even if one was
			// it would be ignored because the scheduler has not yet been
			// started.
			//if( xTimerStart( xTimers[ x ], 0 ) != pdPASS )
			//{
				// The timer could not be set into the Active state.
				//PRINTF("\r\nTimer %d could not be set into the Active state\r\n",x);
			//}
		}

   	}


	/* Create the software timer. */
    /*
	SwTimerHandle = xTimerCreate("SwTimer",          /* Text name. */
	/*							 SW_TIMER_PERIOD_MS, /* Timer period. */
	//							 pdTRUE,             /* Enable auto reload. */
	//							 0,                  /* ID is not used. */
	//							 SwTimerCallback);   /* The callback function. */

	/* Create the software timer. */
	//SwTimerHandle1 = xTimerCreate("SwTimer1",          /* Text name. */
	//							SW_TIMER1_PERIOD_MS, /* Timer period. */
	//							 pdTRUE,             /* Enable auto reload. */
	//							 0,                  /* ID is not used. */
	//							 SwTimer1Callback);   /* The callback function. */


	//xTimerStart( SwTimerHandle, 0 );
    vTaskStartScheduler();

    /* Will not get here unless a task calls vTaskEndScheduler ()*/
    return 0;
}

void vTimerCallback( TimerHandle_t pxTimer ){
	int32_t lArrayIndex;
	const int32_t xMaxExpiryCountBeforeStopping = 20;
	// Optionally do something if the pxTimer parameter is NULL.
	configASSERT( pxTimer );
	static bool reset = false;
	static bool incrementing[2] = {true};

	// Which timer expired?
	lArrayIndex = ( int32_t ) pvTimerGetTimerID( pxTimer );

	// Increment the number of times that pxTimer has expired.


	// If the timer has expired 10 times then stop it from running.
	//if( lExpireCounters[ lArrayIndex ] == xMaxExpiryCountBeforeStopping )
	//{
		// Do not use a block time if calling a timer API function from a
		// timer callback function, as doing so could cause a deadlock!
		//xTimerStop( pxTimer, 0 );
	//}

	if((temperature[lArrayIndex] >= (trgt_temperature[lArrayIndex]+1)) && (trgt_temperature[lArrayIndex] != 0)){
		incrementing[lArrayIndex] = false;
	}else if((temperature[lArrayIndex] <= (trgt_temperature[lArrayIndex]-4)) && (trgt_temperature[lArrayIndex] != 0)){
		incrementing[lArrayIndex] = true;
	}else if( lExpireCounters[ lArrayIndex ] == xMaxExpiryCountBeforeStopping && incrementing[lArrayIndex])	{
		//xTimerStop( pxTimer, 0 );
		incrementing[lArrayIndex] = false;
	}else if(lExpireCounters[ lArrayIndex ] == 0 && !incrementing[lArrayIndex]){
		incrementing[lArrayIndex] = true;
	}

	if(incrementing[lArrayIndex]){
		lExpireCounters[ lArrayIndex ] += 1;
		temperature[lArrayIndex] += speed[lArrayIndex];
		led_red[lArrayIndex] = 1;
		led_blue[lArrayIndex] = 0;
	}else{
		if(lExpireCounters[ lArrayIndex ] > 0)
			lExpireCounters[ lArrayIndex ] -= 1;
		if(temperature[lArrayIndex] > 0)
			temperature[lArrayIndex] -= speed[lArrayIndex];
		led_red[lArrayIndex] = 0;
		led_blue[lArrayIndex] = 1;
	}

	PRINTF("\r\n%stemperature[%d]= %s%d\r\n",BOLD,lArrayIndex,RESET,temperature[lArrayIndex]);
	PRINTF("%slExpireCounters[%d]= %s%d\r\n",BOLD,lArrayIndex,RESET,lExpireCounters[lArrayIndex]);
	PRINTF("%sincrementing[%d]= %s%b\r\n",BOLD,lArrayIndex,RESET,incrementing[lArrayIndex]);
	PRINTF("%strgt_temperature[%d]= %s%d\r\n",BOLD,lArrayIndex,RESET,trgt_temperature[lArrayIndex]);
	PRINTF("%sSpeed[%d]= %s%d\r\n",BOLD,lArrayIndex,RESET,speed[lArrayIndex]);

}

#endif
