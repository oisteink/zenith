// zenith_registry.c
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
    zenith_registry_nvs_blob_t *blob = NULL;
    if ( nvs_open( ZENITH_REGISTRY_NAMESPACE, NVS_READWRITE, &nvs_handle ) == ESP_OK)  
    {
        size_t blob_size;
        // Get the blob size
        if ( nvs_get_blob( nvs_handle, ZENITH_REGISTRY_NODE_KEY, NULL, &blob_size ) == ESP_OK ) 
        {
            blob = calloc( 1, blob_size );
            if ( blob )
            {
                if ( nvs_get_blob( nvs_handle, ZENITH_REGISTRY_NODE_KEY, blob, &blob_size ) == ESP_OK ) 
                {
                    if ( blob->header.registry_version != ZENITH_REGISTRY_VERSION  ) //|| blob_size != sizeof(zenith_registry_t) +  )
                    {
                        if ( nvs_erase_key( nvs_handle, ZENITH_REGISTRY_NODE_KEY ) == ESP_OK )
                            nvs_commit( nvs_handle );
                    }
                    else
                    {
                        zenith_node_handle_t node = NULL;

                        for ( int i = 0; i < blob->header.count; i++ )
                            zenith_registry_retrieve_node_by_mac( handle, blob->nodes[i].mac, &node );
                            // sett other fields that are stored in NVS. Maybe name and version etc

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

esp_err_t _store_to_nvs( zenith_registry_handle_t handle ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid handle"
    );

    nvs_handle_t nvs_handle;
    if ( nvs_open( ZENITH_REGISTRY_NAMESPACE, NVS_READWRITE, &nvs_handle ) == ESP_OK ) {
        size_t blob_size = sizeof( zenith_registry_nvs_blob_t ) + ( sizeof( zenith_node_info_t ) * handle->count );
        zenith_registry_nvs_blob_t *blob = calloc( 1, blob_size );
        ESP_RETURN_ON_FALSE(
            blob,
            ESP_ERR_NO_MEM,
            TAG, "Error allocating blob"
        );
        blob->header.registry_version = handle->registry_version;
        blob->header.count = handle->count;
        for ( int i = 0; i < handle->count; i++ ) 
            memcpy( blob->nodes[i].mac, handle->nodes[i]->info.mac, ESP_NOW_ETH_ALEN );
            // sett other fields that are stored in NVS. Maybe name and version etc

        if ( nvs_set_blob( nvs_handle, ZENITH_REGISTRY_NODE_KEY, handle, blob_size  ) == ESP_OK )
            nvs_commit( nvs_handle );

        nvs_close( nvs_handle );
    }

    return ret;
}

size_t zenith_registry_index_of_mac( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN] ) {
    if ( handle == NULL ) {
        ESP_LOGE( TAG, "Handle is NULL" );
        return -2;
    }

    for ( uint8_t i = 0; i < handle->count; i++ )
        if ( memcmp( handle->nodes[i]->info.mac, mac, ESP_NOW_ETH_ALEN ) == 0 ) 
            return i;

    return -1;
}

size_t zenith_registry_count( zenith_registry_handle_t handle) {
    if ( handle == NULL ) 
    {
        ESP_LOGE( TAG, "Handle is NULL" );
        return -2;
    }

    return handle->count;
}

esp_err_t zenith_registry_create( zenith_registry_handle_t* handle ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG, 
        TAG, "Null handle"
    );

    zenith_registry_handle_t registry = calloc(1, sizeof(zenith_registry_t));
    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_NO_MEM, 
        TAG, "Error allocating registry"
    );

    
    // Set registry version to current version
    registry->registry_version = ZENITH_REGISTRY_VERSION;
    _load_from_nvs( registry );

    *handle = registry;
    return ret;
}

esp_err_t zenith_registry_dispose( zenith_registry_handle_t handle )
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );
    // Loop through all nodes and free them
    for ( uint8_t i = 0; i < handle->count; i++ )
    {
        if ( handle->nodes[i] )
        {
            // Free the node data
            if ( handle->nodes[i]->data.datapoints_handle )
            {
                free( handle->nodes[i]->data.datapoints_handle );
                handle->nodes[i]->data.datapoints_handle = NULL;
            }

            free( handle->nodes[i] );
            handle->nodes[i] = NULL;
        }
    }
    free( handle );
    handle = NULL; // Should I do this? I've freed the pointer, so it is invalid now.
    return ESP_OK;
}

esp_err_t zenith_registry_create_node ( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_handle_t *node ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        mac,
        ESP_ERR_INVALID_ARG,
        TAG, "Null MAC address"
    );

    ESP_RETURN_ON_FALSE(
        node,
        ESP_ERR_INVALID_ARG,
        TAG, "Null node pointer"
    );

    // Check if node is allready registered
    int8_t index = zenith_registry_index_of_mac( handle, mac );
    if ( index >= 0 )
    {
        *node = handle->nodes[index];
        return ESP_OK;
    }  

    // Create new node
    zenith_node_t* new_node = calloc(1, sizeof( zenith_node_t ) );
    ESP_RETURN_ON_FALSE(
        new_node,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating node"
    );

    memcpy( new_node->info.mac, mac, ESP_NOW_ETH_ALEN );
    *node = new_node;

    return ret;
}

esp_err_t zenith_registry_forget_node( zenith_registry_handle_t handle, const uint8_t index ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        index < handle->count,
        ESP_ERR_INVALID_ARG,
        TAG, "Index out of bounds"
    );

    // Decrement count - by "chance" it's now the index of the last item.
    handle->count--;

    // Avoid fragmentation: Move last item into this place and clear it's old place (NULL)
    // That means we need to free index handle, and then move last handle item to index and NULL out last.

    zenith_node_handle_t node = handle->nodes[index];
    // The datapoints_handle is created upon receiving data, so we need to free it here. Older packets will be freed in the event handler when data is received.
    if ( node->data.datapoints_handle )
        free( node->data.datapoints_handle );
    // Free index!
    free( node );
    handle->nodes[index] = NULL;

    // Move last item to index if index isnt the last 
    if ( index != handle->count ) {   
        // Move last item to index
        handle->nodes[index] = handle->nodes[handle->count];
        handle->nodes[handle->count] = NULL;
    }

    return ret;
}

esp_err_t zenith_registry_retrieve_node  ( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_handle_t *data ) {
    ESP_RETURN_ON_FALSE(
        mac,
        ESP_ERR_INVALID_ARG,
        TAG, "Null MAC address"
    );

    ESP_RETURN_ON_FALSE(
        data,
        ESP_ERR_INVALID_ARG,
        TAG, "Null node pointer"
    );

    int8_t index = zenith_registry_index_of_mac( handle, mac );
    ESP_RETURN_ON_FALSE( 
        index >= 0,
        ESP_ERR_NOT_FOUND,
        TAG, "Node not found"
    );

    *data = handle->nodes[index];

    return ESP_OK;
}

esp_err_t zenith_registry_retrieve_node_by_index( zenith_registry_handle_t handle, const uint8_t index, zenith_node_handle_t *node )
{
    ESP_RETURN_ON_FALSE(
        index < handle->count,
        ESP_ERR_INVALID_ARG,
        TAG, "Index out of bounds"
    );

    *node = handle->nodes[index];

    return ESP_OK;
}

/* 
esp_err_t zenith_registry_store_node ( zenith_registry_handle_t handle, const zenith_node_handle_t node )
{
    esp_err_t ret = ESP_OK;
    // update existing node or add new. Index of mac returns -1 if not found
    int index = zenith_registry_index_of_mac( handle, node->info.mac );
    if ( index < 0 ) {
        ESP_RETURN_ON_FALSE(
            handle->count < ZENITH_REGISTRY_MAX_NODES,
            ESP_ERR_NO_MEM,
            TAG, "No room for more nodes"
        );

        index = handle->count;
        handle->count++;
    }

    // Copy data from input node to registry node. We need to free this on delete?
    ESP_RETURN_ON_ERROR(
        ESP_FAIL,
        TAG, "Actual storing Not implemented yet"
    );

    //memcpy( &handle->nodes[index], &node, sizeof( zenith_node_t ) );

    //_store_to_nvs( handle );

    return ESP_OK;
}
 */
