#include "zenith_registry.h"
#include "esp_check.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "string.h"

static char *TAG = "zenith_registry";

esp_err_t _load_from_nvs( zenith_registry_handle_t handle ) 
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid handle"
    );

    nvs_handle_t nvs_handle;
    uint8_t *blob = NULL;
    if ( nvs_open( ZENITH_REGISTRY_NAMESPACE, NVS_READWRITE, &nvs_handle ) == ESP_OK)  
    {
        size_t blob_size;
        // Get the blob size
        if ( nvs_get_blob( nvs_handle, ZENITH_REGISTRY_NODE_KEY, NULL, &blob_size ) == ESP_OK ) 
        {
            blob = malloc( blob_size );
            if (blob)
            {
                if ( nvs_get_blob( nvs_handle, ZENITH_REGISTRY_NODE_KEY, blob, &blob_size ) == ESP_OK ) 
                {
                    if ( blob[0] != ZENITH_REGISTRY_VERSION || blob_size != sizeof(zenith_registry_t) )
                    {
                        if ( nvs_erase_key( nvs_handle, ZENITH_REGISTRY_NODE_KEY ) == ESP_OK )
                            nvs_commit( nvs_handle );
                    }
                    else
                    {
                        memcpy( handle, blob, blob_size );
                    }
                }

                free( blob );
                ESP_LOGI(TAG, "Loaded %d nodes from nvs", handle->count);
            }
        }
        nvs_close(nvs_handle);
    }
    return ret;
}

esp_err_t _store_to_nvs( zenith_registry_handle_t handle )
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid handle"
    );

    nvs_handle_t nvs_handle;
    if ( nvs_open( ZENITH_REGISTRY_NAMESPACE, NVS_READWRITE, &nvs_handle ) == ESP_OK )
    {
        if ( nvs_set_blob( nvs_handle, ZENITH_REGISTRY_NODE_KEY, handle, sizeof( zenith_registry_t )  ) == ESP_OK )
            nvs_commit(nvs_handle);

        nvs_close( nvs_handle );
    }

    return ret;
}

int8_t zenith_registry_index_of_mac( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN] )
{
    assert( handle );
    for ( uint8_t i = 0; i < handle->count; i++ )
    {
        if ( memcmp( handle->nodes[i].mac, mac, ESP_NOW_ETH_ALEN ) == 0 ) 
        {
            return i;
        }
    }

    return -1;
}

esp_err_t zenith_registry_create( zenith_registry_handle_t* handle )
{
    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG, 
        TAG, "Null handle"
    );

    zenith_registry_t *registry = calloc(1, sizeof(zenith_registry_t));
    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_NO_MEM, 
        TAG, "Error allocating registry"
    );

    *handle = registry;
    return ESP_OK;
}

esp_err_t zenith_registry_init( zenith_registry_handle_t handle )
{
    // Set registry version to current version
    handle->registry_version = ZENITH_REGISTRY_VERSION;
    _load_from_nvs( handle );

    return ESP_OK;
}

esp_err_t zenith_registry_deinit( zenith_registry_handle_t handle )
{
    if ( handle )
        free( handle );

    return ESP_OK;
}

esp_err_t zenith_registry_add( zenith_registry_handle_t handle, const zenith_node_t node )
{
    // Ignore if node is allready registred
    if ( zenith_registry_index_of_mac( handle, node.mac ) >= 0 )
        return ESP_OK;

    ESP_RETURN_ON_FALSE(
        handle->count < ZENITH_REGISTRY_MAX_NODES,
        ESP_ERR_NO_MEM,
        TAG, "No room for more nodes"
    );
    
    memcpy( &handle->nodes[handle->count], &node, sizeof( zenith_node_t ) );
    handle->count++;

    _store_to_nvs( handle );

    return ESP_OK;
}

esp_err_t zenith_registry_remove( zenith_registry_handle_t handle, const uint8_t index )
{
    handle->count--;
    if ( index != handle->count )
    {
        memcpy( &handle->nodes[index], &handle->nodes[handle->count], sizeof( zenith_node_t ) );
    }
    memset( &handle->nodes[handle->count], 0, sizeof( zenith_node_t ) );

    return ESP_OK;
}

esp_err_t zenith_registry_get( zenith_registry_handle_t handle, const uint8_t index, zenith_node_t* node )
{
    ESP_RETURN_ON_FALSE(
        index < handle->count,
        ESP_ERR_INVALID_ARG,
        TAG, "Index out of bounds"
    );

    *node = handle->nodes[index];
    return ESP_OK;
}

esp_err_t zenith_registry_count( zenith_registry_handle_t handle, uint8_t* count )
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        handle,
        ESP_FAIL,
        TAG, "Handle is fucked" 
    );
    *count = handle->count;

    return ret;
}

esp_err_t zenith_registry_update( zenith_registry_handle_t handle, const uint8_t index, zenith_node_t node )
{
    ESP_LOGE(TAG, "zenith_registry_update is not implemented");
    return ESP_FAIL;
}

esp_err_t zenith_registry_get_mac( zenith_registry_handle_t handle, uint8_t index, uint8_t mac[ESP_NOW_ETH_ALEN] )
{
    memcpy( mac, handle->nodes[index].mac, ESP_NOW_ETH_ALEN );

    return ESP_OK;
}