// zenith-core.c
#include <stdio.h>
#include "string.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_check.h"

#include "esp_console.h"

#include "zenith_now.h"
#include "zenith_blink.h"
#include "zenith_registry.h"

#include "zenith_ui_core.h"
#include "zenith_registry.h"
#include "zenith_data.h"
#include "cmd_system.h"
#include "argtable3/argtable3.h"



#define WS2812_GPIO GPIO_NUM_8

static const char *TAG = "zenith-core";

zenith_registry_handle_t node_registry = NULL;
//zenith_datapoints_handle_t datapoints_handles[ZENITH_REGISTRY_MAX_NODES];

esp_err_t initialize_nvs(void){
    // Initialize default NVS partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Itsa fucked - erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}


/// @brief Receive callback function for Zenith Core
/// @param mac Mac address of the sender of the packet received
/// @param packet The packet received
static void core_rx_callback(const uint8_t *mac, const zenith_now_packet_t *packet)
{
    ESP_RETURN_VOID_ON_FALSE(
        mac && packet,
        TAG, "NULL pointer passed to core_rx_callback"
    );

    ESP_RETURN_VOID_ON_FALSE(
        packet->header.version == ZENITH_NOW_VERSION,
        TAG, "Version mismatch: %d != %d", packet->header.version, ZENITH_NOW_VERSION
    );

    switch ( packet->header.type )
    {
        case ZENITH_PACKET_PAIRING:
            ESP_LOGI( TAG, "Received pairing request from mac: "MACSTR, MAC2STR( mac ) );
            // Check if any flags or versions or whatnot that are set in the pairing payload and if all is ok:

            // Store the node in registry
            zenith_node_info_t node_info;
            memcpy(&node_info.mac, mac, sizeof( zenith_mac_address_t ) );

            ESP_ERROR_CHECK(
                zenith_registry_store_node_info( node_registry, &node_info )
            );

            // Send ack
            ESP_ERROR_CHECK( 
                zenith_now_send_ack( mac, ZENITH_PACKET_PAIRING ) 
            );

            // Do the pairing complete blink
            ESP_ERROR_CHECK( 
                zenith_blink( BLINK_PAIRING_COMPLETE ) 
            );

            break;

        case ZENITH_PACKET_DATA:
            zenith_blink( BLINK_DATA_RECEIVE );
            ESP_ERROR_CHECK( 
                zenith_now_send_ack( mac, packet->header.type ) 
            );
            
            zenith_datapoints_t *data = ( zenith_datapoints_t * )packet->payload;
            
            ESP_LOGI( TAG, "Received data from mac: "MACSTR, MAC2STR( mac ) );
            ESP_LOGI( TAG, "number_of_datapoints: %d", data->num_datapoints );          
            for ( int i = 0; i < data->num_datapoints; ++i ) {
                ESP_LOGI( TAG, "datapoint %d: type: %d value: %.2f", i, data->datapoints[i].reading_type, data->datapoints[i].value );
            }
            
            ESP_ERROR_CHECK( zenith_registry_store_datapoints( node_registry, mac, data->datapoints, data->num_datapoints ) );
            //ESP_ERROR_CHECK( zenith_registry_full_contents_to_log( node_registry ) );

            //ESP_LOGI( TAG, "Free heap: %u bytes", heap_caps_get_free_size( MALLOC_CAP_DEFAULT ) );
            //UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark( NULL );
            //ESP_LOGI( TAG, "Stack high water mark: %u words (%u bytes)", high_water_mark, high_water_mark * sizeof( StackType_t ) );
            break;
        default:
            ESP_LOGI( TAG, "default unhandled type %d from mac: "MACSTR, packet->header.type, MAC2STR( mac ) );
            break;
        }
}

#define PROMPT_STR CONFIG_IDF_TARGET

static struct {
    struct arg_str *component;
    struct arg_end *end;
} dump_component_args;

typedef enum dump_component_e {
    DUMP_TARGET_REGISTRY=0,
    DUMP_TARGET_MAX
} dump_component_t;

static const char* s_dump_component_names[] = {
    "registry",
};


static int command_dump_component(int argc, char **argv) {
    int nerrors = arg_parse( argc, argv, (void **) &dump_component_args );
    if ( nerrors ) {
        arg_print_errors(stderr, dump_component_args.end, argv[0]);
        return 1;
    }

    if ( dump_component_args.component->count != 1 ) {
        printf("Invalid number of arguments\n");
        return 1;
    }

    char * target_str = dump_component_args.component->sval[0];
    size_t target_len = strlen(target_str);
    dump_component_t target;

    for ( target = DUMP_TARGET_REGISTRY; target < DUMP_TARGET_MAX; target++ ) {
        if (memcmp(target_str, s_dump_component_names[ target ], target_len ) == 0) {
            break;
        }
    }

    switch ( target ) {
        case DUMP_TARGET_REGISTRY:
            ESP_ERROR_CHECK( zenith_registry_full_contents_to_log( node_registry ) );
            break;
        default:
            if ( target == DUMP_TARGET_MAX ) {
                printf( "Invalid dump target '%s', choose from registry|(more-to-come)\n", target_str );
                return 1;
            }
            ESP_LOGW( TAG, "Implementation missing for target %s", target_str );
            break;
    }

    return 0;
}


static void register_dump(void)
{
    dump_component_args.component = arg_str1( NULL, NULL, "target", "To be implemented. The target that you want to dump. Currently just dumps registry" );
    dump_component_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "dump",
        .help = "Dumps information and statistics from various compontents. Currently only supported component is registry.",
        .hint = NULL,
        .func = &command_dump_component,
        .argtable = &dump_component_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register( &cmd ) );
}


void app_main( void )
{
    esp_log_level_set( "*", ESP_LOG_INFO );
    ESP_LOGI( TAG, "app_main()" );
    // Initialize default NVS partition
    ESP_ERROR_CHECK( initialize_nvs() );
    ESP_ERROR_CHECK( zenith_registry_new( &node_registry ) );
    ESP_ERROR_CHECK( zenith_registry_full_contents_to_log( node_registry ) );
    /* size_t count;
    ESP_ERROR_CHECK( zenith_registry_get_node_count( node_registry, &count ) ); 
    ESP_LOGI( TAG, "Registry has %d values", count);*/

    // Initialize display and load UI
    zenith_ui_config_t ui_config = {
        .spi_host = SPI2_HOST,
        .sclk_pin = GPIO_NUM_6,
        .mosi_pin = GPIO_NUM_7,
        .miso_pin = GPIO_NUM_2,

        .lcd_backlight_pin = GPIO_NUM_21,
        .lcd_cs_pin = GPIO_NUM_18,
        .lcd_dc_pin = GPIO_NUM_20,
        .lcd_reset_pin = GPIO_NUM_19,

        .touch_cs_pin = GPIO_NUM_22,
        .touch_irq_pin = GPIO_NUM_NC,

        .node_registry = node_registry,
    };
    zenith_ui_handle_t core_ui_handle = NULL;
    ESP_ERROR_CHECK( zenith_ui_new_core( &ui_config, &core_ui_handle ) );
    ESP_ERROR_CHECK( zenith_ui_test( core_ui_handle ) );
    zenith_ui_core_fade_lcd_brightness(75, 1500);

    // Initialize blinker
    ESP_ERROR_CHECK( init_zenith_blink( WS2812_GPIO ) );

    // Initialize Zenith Now
    zenith_now_config_t zn_config = {
        .rx_cb = core_rx_callback,
    };
    zenith_now_init( &zn_config );


    // Staying alive!
    //vTaskDelay(portMAX_DELAY);

    // Initialize linenoise and console
    //initialize_console_peripheral();
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;
    esp_console_register_help_command();
    register_system_common();
    register_dump();
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(
        esp_console_new_repl_usb_serial_jtag( &hw_config, &repl_config, &repl) 
    );

    ESP_ERROR_CHECK(
        esp_console_start_repl( repl )
    );
} 