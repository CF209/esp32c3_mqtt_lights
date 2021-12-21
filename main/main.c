/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_err.h"

#include "lights_ledc.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "van_wifi"
#define EXAMPLE_ESP_WIFI_PASS      "applesauce"
#define EXAMPLE_ESP_MAXIMUM_RETRY  (25)

#define CONFIG_BROKER_URI "mqtt://192.168.1.101:1883"

#define CONFIG_LIGHT0_BRIGHTNESS        "esp32/main_lights/brightness"
#define CONFIG_LIGHT0_BRIGHTNESS_SET    "esp32/main_lights/brightness/set"
#define CONFIG_LIGHT0_STATUS            "esp32/main_lights/status"
#define CONFIG_LIGHT0_SWITCH            "esp32/main_lights/switch"

#define CONFIG_LIGHT1_BRIGHTNESS        "esp32/bed_lights/brightness"
#define CONFIG_LIGHT1_BRIGHTNESS_SET    "esp32/bed_lights/brightness/set"
#define CONFIG_LIGHT1_STATUS            "esp32/bed_lights/status"
#define CONFIG_LIGHT1_SWITCH            "esp32/bed_lights/switch"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "MQTT Light Control";

static int s_retry_num = 0;

char main_lights_state[5] = "OFF";
char main_lights_brightness[5] = "0";
char bed_lights_state[5] = "OFF";
char bed_lights_brightness[5] = "0";


/*
static void initialize_lights_from_flash(void)
{
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");

        // Read
        printf("Reading brightness0 from NVS ... ");
        int32_t brightness = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_i32(my_handle, "brightness0", &brightness);
        switch (err) {
            case ESP_OK:
                printf("Done\n");
                printf("Initializing brightness to %d\n", brightness);
                sprintf(main_lights_brightness, "%d", brightness);
                lights_set_brightness(brightness, 0);
                if (brightness > 0) {
                    strcpy(main_lights_state, "ON");
                }
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        printf("Reading brightness1 from NVS ... ");
        brightness = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_i32(my_handle, "brightness1", &brightness);
        switch (err) {
            case ESP_OK:
                printf("Done\n");
                printf("Initializing brightness to %d\n", brightness);
                sprintf(bed_lights_brightness, "%d", brightness);
                lights_set_brightness(brightness, 1);
                if (brightness > 0) {
                    strcpy(bed_lights_state, "ON");
                }
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        // Close
        nvs_close(my_handle);
    }
}

static void write_brightness_to_flash(int light, int brightness)
{
    char brightness_key[12];
    if (light == 0) {
        strcpy(brightness_key, "brightness0");
    } else if (light == 1) {
        strcpy(brightness_key, "brightness1");
    } else {
        printf("Error: Invalid light number\n");
        return;
    }

    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");

        // Write
        printf("Updating light%d brightness in NVS ... ", light);
        err = nvs_set_i32(my_handle, brightness_key, brightness);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(my_handle);
    }
}

*/

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            esp_restart();
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            msg_id = esp_mqtt_client_subscribe(client, CONFIG_LIGHT0_SWITCH, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, CONFIG_LIGHT0_BRIGHTNESS_SET, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, CONFIG_LIGHT1_SWITCH, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, CONFIG_LIGHT1_BRIGHTNESS_SET, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT0_STATUS, main_lights_state, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT0_BRIGHTNESS, main_lights_brightness, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT1_STATUS, bed_lights_state, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT1_BRIGHTNESS, bed_lights_brightness, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            esp_restart();
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            char t[100];
            strncpy(t, event->topic, event->topic_len);
            t[event->topic_len] = '\0';
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            if (strcmp(t, CONFIG_LIGHT0_BRIGHTNESS_SET) == 0) {
                strncpy(main_lights_brightness, event->data, event->data_len);
                main_lights_brightness[event->data_len] = '\0';
                strcpy(main_lights_state, "ON");
                lights_set_brightness(atoi(main_lights_brightness), 0);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT0_STATUS, main_lights_state, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT0_BRIGHTNESS, main_lights_brightness, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                // write_brightness_to_flash(0, atoi(main_lights_brightness));
            }
            else if (strcmp(t, CONFIG_LIGHT0_SWITCH) == 0) {
                strcpy(main_lights_brightness, "0");
                strcpy(main_lights_state, "OFF");
                lights_set_brightness(0, 0);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT0_STATUS, main_lights_state, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT0_BRIGHTNESS, main_lights_brightness, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                // write_brightness_to_flash(0, 0);
            }
            else if (strcmp(t, CONFIG_LIGHT1_BRIGHTNESS_SET) == 0) {
                strncpy(bed_lights_brightness, event->data, event->data_len);
                bed_lights_brightness[event->data_len] = '\0';
                strcpy(bed_lights_state, "ON");
                lights_set_brightness(atoi(bed_lights_brightness), 1);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT1_STATUS, bed_lights_state, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT1_BRIGHTNESS, bed_lights_brightness, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                // write_brightness_to_flash(1, atoi(bed_lights_brightness));
            }
            else if (strcmp(t, CONFIG_LIGHT1_SWITCH) == 0) {
                strcpy(bed_lights_brightness, "0");
                strcpy(bed_lights_state, "OFF");
                lights_set_brightness(0, 1);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT1_STATUS, bed_lights_state, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                msg_id = esp_mqtt_client_publish(client, CONFIG_LIGHT1_BRIGHTNESS, bed_lights_brightness, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                // write_brightness_to_flash(1, 0);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                                                                strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URI,
        // .user_context = (void *)your_context
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing_LEDC");
    lights_ledc_init();

    // ESP_LOGI(TAG, "Initializing_Flash");
    // initialize_lights_from_flash();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    mqtt_app_start();
}
