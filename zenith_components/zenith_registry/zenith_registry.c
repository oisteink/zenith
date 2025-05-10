// zenith_registry.c

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "time.h"
#include "zenith_registry.h"

#define ZENITH_REGISTRY_NVS_NAMESPACE "zenith_registry"
#define ZENITH_REGISTRY_NVS_KEY "nodes"
#define ZENITH_REGISTRY_VERSION 1
#define ZENITH_REGISTRY_MAX_NODES 10

struct zenith_registry_s {
    zenith_node_info_t nodes[ZENITH_REGISTRY_MAX_NODES];
    uint8_t node_count;
    zenith_node_runtime_t *runtime_buffers; // where we store the live sensor data
    uint8_t buffer_count; // number of buffers allocated
    zenith_registry_callback_t callback;
};

const char *TAG = "zenith_registry";

// ringbuffer support

// get the index of a MAC address in the runtime buffer - poor naming
static int _buffer_index_of_mac( zenith_registry_handle_t handle, const zenith_mac_address_t mac ) {
    if ( handle == NULL ) {
        ESP_LOGE( TAG, "Handle is NULL" );
        abort();
    }

    ESP_LOGD( TAG, "buffer_index_of_mac - Registry count: %d", handle->buffer_count );
    ESP_LOGD( TAG, "buffer_index_of_mac - MAC: "MACSTR, MAC2STR( mac ) );
    for ( uint8_t i = 0; i < handle->buffer_count; i++ )
        if ( memcmp( handle->runtime_buffers[i].mac, mac, ESP_NOW_ETH_ALEN ) == 0 ) 
            return i;

    return -1; // Not found
}

// Get the ringbuffer for a given sensor type or create it if not found
static esp_err_t _get_ringbuffer( zenith_node_runtime_t *node, zenith_sensor_type_t type, zenith_ringbuffer_t **ringbuffer ) {
    // Search existing rings
    for ( size_t i = 0; i < node->ring_count; ++i ) {
        if ( node->rings[i].type == type ) {
            *ringbuffer = &node->rings[i];
            return ESP_OK; // not happy with early returns, but this is cleaner
        }
    }

    // Not found — allocate new ring slot
    zenith_ringbuffer_t *new_rings = realloc( node->rings, (node->ring_count + 1) * sizeof( zenith_ringbuffer_t ) );
    ESP_RETURN_ON_FALSE( 
        new_rings, 
        ESP_ERR_NO_MEM, 
        TAG, "Failed to allocate memory for new ring" 
    );
    node->rings = new_rings;
    zenith_ringbuffer_t *new_ring = &node->rings[node->ring_count];
    node->ring_count++;

    memset( new_ring, 0, sizeof( *new_ring ) );
    new_ring->type = type;

    *ringbuffer = new_ring;

    return ESP_OK;
}

// Get the node runtime buffer for a given MAC address or create it if not found
static esp_err_t _get_node_runtime_data( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_node_runtime_t **out_runtime_data )
{
    ESP_RETURN_ON_FALSE( 
        handle && mac && out_runtime_data, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to get_node_runtime" 
    );

    int index = _buffer_index_of_mac( handle, mac );
    if ( index < 0 ) {
        // Reallocate the runtime buffers array to add a new one
        zenith_node_runtime_t *new_rings = realloc( handle->runtime_buffers, ( handle->buffer_count + 1 ) * sizeof( zenith_node_runtime_t ) );
        ESP_RETURN_ON_FALSE( 
            new_rings, 
            ESP_ERR_NO_MEM, 
            TAG, "Failed to (re)allocate memory for new runtime buffer" 
        );
        handle->runtime_buffers = new_rings;

        index = handle->buffer_count;
        handle->buffer_count++;

        // Initialize the new ring buffer
        zenith_node_runtime_t *new_ring = &handle->runtime_buffers[index];
        memset( new_ring, 0, sizeof( *new_ring ) );
        memcpy( new_ring->mac, mac, sizeof (zenith_mac_address_t ) );
    }

    *out_runtime_data = &handle->runtime_buffers[index];
    return ESP_OK;
}

// Add a reading to the ringbuffer
static esp_err_t _ringbuffer_add_reading( zenith_ringbuffer_t *ring, zenith_reading_datatype_t value ) {
    zenith_reading_t *slot = &ring->entries[ring->head];
    slot->timestamp = time( NULL ); // Get current time in seconds since epoch
    slot->value = value;

    ring->head = (ring->head + 1) % ZENITH_RING_CAPACITY;
    if ( ring->size < ZENITH_RING_CAPACITY ) {
        ring->size++;
    }
    return ESP_OK;
}

static int _index_of_mac( zenith_registry_handle_t handle, const zenith_mac_address_t mac ) {
    if ( handle == NULL ) {
        ESP_LOGE( TAG, "Handle is NULL" );
        abort();
    }

    ESP_LOGD( TAG, "index_of_mac - Registry count: %d", handle->node_count );
    ESP_LOGD( TAG, "index_of_mac - MAC: "MACSTR, MAC2STR( mac ) );
    for ( uint8_t i = 0; i < handle->node_count; i++ )
        if ( memcmp( handle->nodes[i].mac, mac, ESP_NOW_ETH_ALEN ) == 0 ) 
            return i;

    return -1; // Not found
}

static esp_err_t zenith_registry_load_from_nvs( zenith_registry_handle_t handle )
{
    esp_err_t ret = ESP_OK;

    zenith_registry_nvs_blob_t *blob = NULL;

    nvs_handle_t nvs;
    //create if not exists
    ESP_GOTO_ON_ERROR( 
        nvs_open( ZENITH_REGISTRY_NVS_NAMESPACE, NVS_READWRITE, &nvs ),
        end, TAG, "Failed to open NVS namespace %s", ZENITH_REGISTRY_NVS_NAMESPACE
    );

    size_t required_size = 0;
    ESP_GOTO_ON_ERROR(
        nvs_get_blob( nvs, ZENITH_REGISTRY_NVS_KEY, NULL, &required_size ),
        end, TAG, "Failed to get size of NVS key %s", ZENITH_REGISTRY_NVS_KEY
    );


    ESP_GOTO_ON_FALSE(
        required_size >= sizeof( zenith_registry_nvs_header_t ),
        ESP_OK,
        end, TAG, "NVS key %s is too small", ZENITH_REGISTRY_NVS_KEY
    );

    blob = malloc( required_size );
    ESP_GOTO_ON_FALSE(
        blob,
        ESP_ERR_NO_MEM,
        end, TAG, "Failed to allocate memory for NVS key %s", ZENITH_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_FALSE(
        nvs_get_blob( nvs, ZENITH_REGISTRY_NVS_KEY, blob, &required_size ) == ESP_OK,
        ESP_OK,
        end, TAG, "Failed to get NVS key %s", ZENITH_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_FALSE(
        blob->header.registry_version == ZENITH_REGISTRY_VERSION,
        ESP_OK,
        end, TAG, "NVS key %s has invalid version %d", ZENITH_REGISTRY_NVS_KEY, blob->header.registry_version
    );

    ESP_GOTO_ON_FALSE(
        blob->header.count <= ZENITH_REGISTRY_MAX_NODES,
        ESP_OK,
        end, TAG, "NVS key %s has too many nodes %d", ZENITH_REGISTRY_NVS_KEY, blob->header.count
    );

    memcpy( handle->nodes, blob->nodes, blob->header.count * sizeof( zenith_node_info_t ) );
    handle->node_count = blob->header.count;
    ESP_LOGD( TAG, "Loaded %d nodes from NVS", handle->node_count );

end:
    nvs_close( nvs );

    if ( blob)
        free( blob );
    
    if ( ret == ESP_ERR_NVS_NOT_FOUND )
        ret = ESP_OK; // No data yet — not an error
    
    return ret;
}

static esp_err_t zenith_registry_save_to_nvs( zenith_registry_handle_t handle )
{
    esp_err_t ret = ESP_OK;
    size_t blob_size = sizeof( zenith_registry_nvs_header_t ) + handle->node_count * sizeof( zenith_node_info_t );

    zenith_registry_nvs_blob_t *blob = NULL;
    blob = malloc( blob_size );
    ESP_RETURN_ON_FALSE(
        blob,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for NVS key %s", ZENITH_REGISTRY_NVS_KEY
    );

    blob->header.registry_version = ZENITH_REGISTRY_VERSION;
    blob->header.count = handle->node_count;
    memcpy( blob->nodes, handle->nodes, handle->node_count * sizeof( zenith_node_info_t ) );

    nvs_handle_t nvs;
    ESP_GOTO_ON_ERROR(
        nvs_open( ZENITH_REGISTRY_NVS_NAMESPACE, NVS_READWRITE, &nvs ),
        end, 
        TAG, "Failed to open NVS namespace %s", ZENITH_REGISTRY_NVS_NAMESPACE
    );

    ESP_GOTO_ON_ERROR(
        nvs_set_blob( nvs, ZENITH_REGISTRY_NVS_KEY, blob, blob_size ),
        end, 
        TAG, "Failed to set NVS key %s", ZENITH_REGISTRY_NVS_KEY
    );

    ESP_GOTO_ON_ERROR(
        nvs_commit( nvs ),
        end,
        TAG, "Failed to commit NVS key %s", ZENITH_REGISTRY_NVS_KEY
    );
    
end:
    nvs_close( nvs );
    if ( blob )
        free( blob );

    return ret;
}

esp_err_t zenith_registry_new( zenith_registry_handle_t *out_handle )
{
    ESP_RETURN_ON_FALSE( out_handle, ESP_ERR_INVALID_ARG, TAG, "out_handle is NULL" );

    zenith_registry_handle_t handle = NULL;
    handle = calloc( 1, sizeof( struct zenith_registry_s ) );
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_NO_MEM, TAG, "Failed to allocate registry handle" );

    if ( zenith_registry_load_from_nvs( handle ) != ESP_OK ) 
        ESP_LOGD( TAG, "Failed to load registry from NVS" );
    
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t zenith_registry_delete( zenith_registry_handle_t handle )
{
    if ( handle )
        free( handle );
    return ESP_OK;
}

esp_err_t zenith_registry_register_callback( zenith_registry_handle_t handle, zenith_registry_callback_t callback )
{
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL" );
    handle->callback = callback;
    return ESP_OK;
}

esp_err_t zenith_registry_store_node_info( zenith_registry_handle_t handle, const zenith_node_info_t *info )
{
    ESP_RETURN_ON_FALSE( 
        handle && info, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to store_node_info" 
    );

    zenith_registry_event_t event = ZENITH_REGISTRY_EVENT_NODE_ADDED;

    // Search for existing
    int index = _index_of_mac( handle, info->mac );

    if ( index >= 0 ) {
        handle->nodes[ index ] = *info;
        event = ZENITH_REGISTRY_EVENT_NODE_UPDATED;
    } else {
        ESP_RETURN_ON_FALSE( handle->node_count < ZENITH_REGISTRY_MAX_NODES, ESP_ERR_NO_MEM, TAG, "Max node limit reached" );
        handle->nodes[handle->node_count++] = *info;
    }

    ESP_RETURN_ON_ERROR( zenith_registry_save_to_nvs( handle ), TAG, "Failed to save updated node list to NVS" );

    if ( handle->callback ) {
        handle->callback( event, info->mac );
    }

    return ESP_OK;
}

esp_err_t zenith_registry_get_node_info( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_node_info_t *out_info )
{
    ESP_RETURN_ON_FALSE( 
        handle && mac && out_info, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to get_node_info" 
    );

    int index = _index_of_mac( handle, mac );
    if ( index >= 0 ) {
        out_info = &handle->nodes[index];
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t zenith_registry_forget_node( zenith_registry_handle_t handle, const zenith_mac_address_t mac )
{
    ESP_RETURN_ON_FALSE( 
        handle && mac, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to remove_node" 
    );

    int index = _index_of_mac( handle, mac ); 

    if ( index >= 0 ) {
        ESP_LOGD( TAG, "Forget node %d", index );   

        // Copy last node to current index
        if ( index != handle->node_count - 1 )
            handle->nodes[index] = handle->nodes[handle->node_count - 1]; 

        // Clear last node
        handle->nodes[handle->node_count - 1] = (zenith_node_info_t){0}; 
        handle->node_count--;
        zenith_registry_save_to_nvs( handle );

        if ( handle->callback ) {
            handle->callback( ZENITH_REGISTRY_EVENT_NODE_REMOVED, mac );
        }

        return ESP_OK;

    } else {
        ESP_LOGD( TAG, "Forget node %d not found", index );   
        return ESP_ERR_NOT_FOUND;
    }
   
}

esp_err_t zenith_registry_store_datapoints( zenith_registry_handle_t handle, const zenith_mac_address_t mac, const zenith_datapoint_t *datapoints, size_t count ) {
    ESP_RETURN_ON_FALSE( 
        handle && mac && datapoints, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to store_readings" 
    );

    zenith_node_runtime_t *node = NULL;
    ESP_RETURN_ON_ERROR(
        _get_node_runtime_data( handle, mac , &node ),
        TAG, "Failed to get node runtime data"
    );

    ESP_RETURN_ON_FALSE( 
        node, 
        ESP_ERR_NO_MEM, 
        TAG, "Failed to allocate node runtime" 
    );

    for ( size_t i = 0; i < count; ++i ) {
        const zenith_datapoint_t *dp = &datapoints[i];
        zenith_ringbuffer_t *ring = NULL;
        ESP_RETURN_ON_ERROR(
            _get_ringbuffer( node, dp->reading_type, &ring ),
            TAG, "Failed to get ringbuffer for sensor type %d", dp->reading_type
        );
        ESP_RETURN_ON_FALSE( 
            ring, 
            ESP_ERR_NO_MEM, 
            TAG, "Failed to allocate ring for sensor type %d", dp->reading_type 
        );
        _ringbuffer_add_reading( ring, dp->value );
    }

    if ( handle->callback ) {
        handle->callback( ZENITH_REGISTRY_EVENT_READING_UPDATED, mac );
    }

    return ESP_OK;
}

esp_err_t zenith_registry_get_node_count( zenith_registry_handle_t handle, size_t *out_count )
{
    ESP_RETURN_ON_FALSE( 
        handle && out_count, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to get_node_count" 
    );

    *out_count = handle->node_count;
    return ESP_OK;
}

esp_err_t zenith_registry_get_all_node_macs( zenith_registry_handle_t handle, zenith_mac_address_t *out_macs, size_t *inout_count )
{
    ESP_RETURN_ON_FALSE( 
        handle && inout_count, 
        ESP_ERR_INVALID_ARG, 
        TAG, "Invalid args to get_node_macs" 
    );

    if ( out_macs == NULL ) {
        // return required size
        *inout_count = handle->node_count;
        return ESP_OK;
    }

    if ( *inout_count < handle->node_count ) {
        ESP_LOGE( TAG, "Not enough space for node macs" );
        return ESP_ERR_NO_MEM;
    }

    for ( size_t i = 0; i < handle->node_count; i++ ) {
        memcpy( out_macs[i], handle->nodes[i].mac, ESP_NOW_ETH_ALEN );
    }

    *inout_count = handle->node_count;
    return ESP_OK;
}

esp_err_t zenith_registry_full_contents_to_log( zenith_registry_handle_t handle ) {
    ESP_LOGI( TAG, "---- Zenith Registry Dump ----" );
    ESP_LOGI( TAG, "Nodes: %u", (unsigned) handle->node_count );
    ESP_LOGI( TAG, "------------------------------" );
    
    ESP_LOGI( TAG, "Node information:" );
    for ( size_t i = 0; i < handle->node_count; ++i ) {
        zenith_node_info_t *node = &handle->nodes[ i ];
        ESP_LOGI( TAG, " Node %zu — MAC: "MACSTR, i, MAC2STR( node->mac ) );
    }

    ESP_LOGI( TAG, "Runtime data:" );
    for ( size_t i = 0; i < handle->buffer_count; ++i ) {
        zenith_node_runtime_t *node = &handle->runtime_buffers[ i ];
        ESP_LOGI( TAG, " Node %zu — MAC: "MACSTR", Rings: %zu", i, MAC2STR( node->mac ), node->ring_count );

        for ( size_t j = 0; j < node->ring_count; ++j ) {
            zenith_ringbuffer_t *ring = &node->rings[j];
            ESP_LOGI( TAG, "   Sensor Type: %u — Readings: %u",
                      (unsigned) ring->type, (unsigned) ring->size );

            for ( size_t k = 0; k < ring->size; ++k ) {
                zenith_reading_t *r = &ring->entries[ k ];
                if ( r->timestamp == 0 ) {
                    continue; // skip uninitialized entries
                }

                ESP_LOGI( TAG, "     [%zu] ts=%llu, value=%.2f",
                          k, r->timestamp, r->value );
            }
        }
    }

    ESP_LOGI( TAG, "------------------------------" );
    return ESP_OK;
}