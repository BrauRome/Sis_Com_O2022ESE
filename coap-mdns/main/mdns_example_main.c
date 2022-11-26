/* MDNS-SD Query and advertise Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_ip_addr.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "mdns.h"
#include "driver/gpio.h"
#include "netdb.h"
#include <sys/socket.h>
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "coap3/coap.h"

#define EXAMPLE_MDNS_INSTANCE CONFIG_MDNS_INSTANCE
#define EXAMPLE_BUTTON_GPIO     0

static const char * TAG = "mdns-test";
static char * generate_hostname(void);

static void initialise_mdns(void)
{
    char * hostname = generate_hostname();

    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK( mdns_instance_name_set(EXAMPLE_MDNS_INSTANCE) );

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("shoe_control", "_coap", "_udp", 80, serviceTxtData, 3) );

    //add another TXT item
    ESP_ERROR_CHECK( mdns_service_txt_item_set("_coap", "_udp", "path", "/foobar") );
    //change TXT item value
    ESP_ERROR_CHECK( mdns_service_txt_item_set_with_explicit_value_len("_coap", "_udp", "u", "admin", strlen("admin")) );
    free(hostname);
}

static void mdns_example_task(void *pvParameters)
{
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

#define EXAMPLE_COAP_LOG_DEFAULT_LEVEL CONFIG_COAP_LOG_DEFAULT_LEVEL

static char espressif_data[100];
static int espressif_data_len = 0;

static char shoe_shoelace[10];
static int shoe_shoelace_len = 0;

static char shoe_color[10];
static int shoe_color_len = 0;

static uint32_t shoe_steps_int = 0;
static char shoe_steps[10];
static int shoe_steps_len = 0;

static char shoe_size[10];
static int shoe_size_len = 0;

static char shoe_name[20];
static int shoe_name_len = 0;

#define INITIAL_DATA "Hello World!"
#define SHOELACE_INITIAL_DATA "untie"
#define COLOR_INITIAL_DATA "000000"
#define STEPS_INITIAL_DATA "0"
#define SIZE_INITIAL_DATA "20"
#define NAME_INITIAL_DATA "No name"

/*
 * The resource handler
 */
static void
hnd_espressif_get(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)espressif_data_len,
                                 (const u_char *)espressif_data,
                                 NULL, NULL);
}

static void
hnd_espressif_put(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    size_t offset;
    size_t total;
    const unsigned char *data;
    

    if (strcmp (espressif_data, INITIAL_DATA) == 0) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
    }

    /* coap_get_data_large() sets size to 0 on error */
    (void)coap_get_data_large(request, &size, &data, &offset, &total);


    if (size == 0) {      /* re-init */
        snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
        espressif_data_len = strlen(espressif_data);
    } else {
        espressif_data_len = size > sizeof (espressif_data) ? sizeof (espressif_data) : size;
        memcpy (espressif_data, data, espressif_data_len);
    }
}

static void
hnd_espressif_delete(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response)
{
    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_DELETED);
}


static void
hnd_shoe_get_shoelace(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)shoe_shoelace_len,
                                 (const u_char *)shoe_shoelace,
                                 NULL, NULL);
}

static void
hnd_shoe_put_shoelace(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    size_t offset;
    size_t total;
    const unsigned char *data;
    

    if (strcmp (shoe_shoelace, SHOELACE_INITIAL_DATA) == 0) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
    }

    /* coap_get_data_large() sets size to 0 on error */
    (void)coap_get_data_large(request, &size, &data, &offset, &total);


    if (size == 0) {      /* re-init */
        snprintf(shoe_shoelace, sizeof(shoe_shoelace), SHOELACE_INITIAL_DATA);
        shoe_shoelace_len = strlen(shoe_shoelace);
    } else {
        shoe_shoelace_len = size > sizeof (shoe_shoelace) ? sizeof (shoe_shoelace) : size;
        memcpy (shoe_shoelace, data, shoe_shoelace_len);
    }
}

static void
hnd_shoe_get_color(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)shoe_color_len,
                                 (const u_char *)shoe_color,
                                 NULL, NULL);
}

static void
hnd_shoe_put_color(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    size_t offset;
    size_t total;
    const unsigned char *data;
    

    if (strcmp (shoe_color, COLOR_INITIAL_DATA) == 0) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
    }

    /* coap_get_data_large() sets size to 0 on error */
    (void)coap_get_data_large(request, &size, &data, &offset, &total);


    if (size == 0) {      /* re-init */
        snprintf(shoe_color, sizeof(shoe_color), COLOR_INITIAL_DATA);
        shoe_color_len = strlen(shoe_color);
    } else {
        shoe_color_len = size > sizeof (shoe_color) ? sizeof (shoe_color) : size;
        memcpy (shoe_color, data, shoe_color_len);
    }
}

static void
hnd_shoe_delete_color(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response)
{
    snprintf(shoe_color, sizeof(shoe_color), COLOR_INITIAL_DATA);
    shoe_color_len = strlen(shoe_color);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_DELETED);
}

static void
hnd_shoe_get_steps(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    sprintf(shoe_steps, "%d", shoe_steps_int);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)shoe_steps_len,
                                 (const u_char *)shoe_steps,
                                 NULL, NULL);
}

static void
hnd_shoe_delete_steps(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response)
{
    shoe_steps_int = 0;
    snprintf(shoe_steps, sizeof(shoe_steps), STEPS_INITIAL_DATA);
    shoe_steps_len = strlen(shoe_steps);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_DELETED);
}

static void
hnd_shoe_get_size(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)shoe_size_len,
                                 (const u_char *)shoe_size,
                                 NULL, NULL);
}

static void
hnd_shoe_get_name(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)shoe_name_len,
                                 (const u_char *)shoe_name,
                                 NULL, NULL);
}

static void
hnd_shoe_put_name(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    size_t offset;
    size_t total;
    const unsigned char *data;
    

    if (strcmp (shoe_name, NAME_INITIAL_DATA) == 0) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
    }

    /* coap_get_data_large() sets size to 0 on error */
    (void)coap_get_data_large(request, &size, &data, &offset, &total);


    if (size == 0) {      /* re-init */
        snprintf(shoe_name, sizeof(shoe_name), NAME_INITIAL_DATA);
        shoe_name_len = strlen(shoe_name);
    } else {
        shoe_name_len = size > sizeof (shoe_name) ? sizeof (shoe_name) : size;
        memcpy (shoe_name, data, shoe_name_len);
    }
}

static void
hnd_shoe_delete_name(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response)
{
    snprintf(shoe_name, sizeof(shoe_name), NAME_INITIAL_DATA);
    shoe_name_len = strlen(shoe_name);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_DELETED);
}

void
coap_log_handler (coap_log_t level, const char *message)
{
    uint32_t esp_level = ESP_LOG_INFO;
    char *cp = strchr(message, '\n');

    if (cp)
        ESP_LOG_LEVEL(esp_level, TAG, "%.*s", (int)(cp-message), message);
    else
        ESP_LOG_LEVEL(esp_level, TAG, "%s", message);
}

static void coap_example_server(void *p)
{
    coap_context_t *ctx = NULL;
    coap_address_t serv_addr;
    coap_resource_t *resource = NULL;

    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    snprintf(shoe_shoelace, sizeof(shoe_shoelace), SHOELACE_INITIAL_DATA);
    shoe_shoelace_len = strlen(shoe_shoelace);
    snprintf(shoe_color, sizeof(shoe_color), COLOR_INITIAL_DATA);
    shoe_color_len = strlen(shoe_color);
    snprintf(shoe_steps, sizeof(shoe_steps), STEPS_INITIAL_DATA);
    shoe_steps_len = strlen(shoe_steps);
    snprintf(shoe_size, sizeof(shoe_size), SIZE_INITIAL_DATA);
    shoe_size_len = strlen(shoe_size);
    snprintf(shoe_name, sizeof(shoe_name), NAME_INITIAL_DATA);
    shoe_name_len = strlen(shoe_name);

    coap_set_log_handler(coap_log_handler);
    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);

    ESP_LOGI(TAG, "CoAP server example started!");

    while (1) {
        coap_endpoint_t *ep = NULL;
        unsigned wait_ms;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin6.sin6_family = AF_INET6;
        serv_addr.addr.sin6.sin6_port   = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }
        coap_context_set_block_mode(ctx,
                                    COAP_BLOCK_USE_LIBCOAP|COAP_BLOCK_SINGLE_BODY);

        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
        if (!ep) {
            ESP_LOGE(TAG, "udp: coap_new_endpoint() failed");
            goto clean_up;
        }
        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_TCP);
        if (!ep) {
            ESP_LOGE(TAG, "tcp: coap_new_endpoint() failed");
            goto clean_up;
        }

        resource = coap_resource_init(coap_make_str_const("Espressif"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_espressif_get);
        coap_register_handler(resource, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(resource, COAP_REQUEST_DELETE, hnd_espressif_delete);

        coap_add_resource(ctx, resource);

        resource = NULL;
        resource = coap_resource_init(coap_make_str_const("shoe/shoelace"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_shoe_get_shoelace);
        coap_register_handler(resource, COAP_REQUEST_PUT, hnd_shoe_put_shoelace);

        coap_add_resource(ctx, resource);

        resource = NULL;
        resource = coap_resource_init(coap_make_str_const("shoe/color"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_shoe_get_color);
        coap_register_handler(resource, COAP_REQUEST_PUT, hnd_shoe_put_color);
        coap_register_handler(resource, COAP_REQUEST_DELETE, hnd_shoe_delete_color);

        coap_add_resource(ctx, resource);

        resource = NULL;
        resource = coap_resource_init(coap_make_str_const("shoe/steps"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_shoe_get_steps);
        coap_register_handler(resource, COAP_REQUEST_DELETE, hnd_shoe_delete_steps);

        coap_add_resource(ctx, resource);

        resource = NULL;
        resource = coap_resource_init(coap_make_str_const("shoe/size"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_shoe_get_size);

        coap_add_resource(ctx, resource);

        resource = NULL;
        resource = coap_resource_init(coap_make_str_const("shoe/name"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_shoe_get_name);
        coap_register_handler(resource, COAP_REQUEST_PUT, hnd_shoe_put_name);
        coap_register_handler(resource, COAP_REQUEST_DELETE, hnd_shoe_delete_name);

        coap_add_resource(ctx, resource);

        wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;

        while (1) {
            int result = coap_io_process(ctx, wait_ms);
            if (result < 0) {
                break;
            } else if (result && (unsigned)result < wait_ms) {
                /* decrement if there is a result wait time returned */
                wait_ms -= result;
            }
            if (result) {
                /* result must have been >= wait_ms, so reset wait_ms */
                wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
                shoe_steps_int ++;
            }

            vTaskDelay(50 / portTICK_PERIOD_MS);
            
        }
    }
clean_up:
    coap_free_context(ctx);
    coap_cleanup();

    vTaskDelete(NULL);
}


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialise_mdns();

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(&coap_example_server, "coap_example_server",  8 * 1024, NULL, 5, NULL);
}

/** Generate host name based on sdkconfig, optionally adding a portion of MAC address to it.
 *  @return host name string allocated from the heap
 */
static char* generate_hostname(void)
{
#ifndef CONFIG_MDNS_ADD_MAC_TO_HOSTNAME
    return strdup(CONFIG_MDNS_HOSTNAME);
#else
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", CONFIG_MDNS_HOSTNAME, mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
#endif
}
