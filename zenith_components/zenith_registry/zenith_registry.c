// zenith_registry.c
#include "zenith_registry.h"
#include "esp_check.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_mac.h"
#include "esp_log.h"


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

                        for ( int i = 0; i < blob->header.count; i++ ) {
                            ESP_LOGD(TAG, "Loading node %d with mac: "MACSTR, i, MAC2STR( blob->nodes[i].mac ) );
                            zenith_registry_get_node( handle, blob->nodes[i].mac, &node );
                            //node->info->version = blob->nodes[i].version;
                            // sett other fields that are stored in NVS. Maybe name and version etc
                        }

                    }
                }

                free( blob );
                ESP_LOGD(TAG, "Loaded %d nodes from nvs", handle->count);
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
            memcpy( blob->nodes[i].mac, handle->nodes[i]->info->mac, ESP_NOW_ETH_ALEN );
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
    ESP_LOGD( TAG, "index_of_mac - Registry count: %d", handle->count );
    ESP_LOGD( TAG, "index_of_mac - MAC: "MACSTR, MAC2STR( mac ) );
    for ( uint8_t i = 0; i < handle->count; i++ )
        if ( memcmp( handle->nodes[i]->info->mac, mac, ESP_NOW_ETH_ALEN ) == 0 ) 
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
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

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
    ESP_LOGD( TAG, "Registry version: %d", registry->registry_version );
    ESP_LOGD( TAG, "Registry count: %d", registry->count );
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
            if ( handle->nodes[i]->data->datapoints_handle )
            {
                free( handle->nodes[i]->data->datapoints_handle );
                handle->nodes[i]->data->datapoints_handle = NULL;
            }

            free( handle->nodes[i] );
            handle->nodes[i] = NULL;
        }
    }
    free( handle );
    handle = NULL; // Should I do this? I've freed the pointer, so it is invalid now.
    return ret;
}


esp_err_t zenith_registry_create_node_data ( zenith_registry_handle_t registry, uint8_t number_of_datapoints, zenith_node_data_handle_t *out_data ) {
    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );
    ESP_RETURN_ON_FALSE(
        out_data,
        ESP_ERR_INVALID_ARG,
        TAG, "Null data pointer"
    );

    // Create new data
    zenith_node_data_handle_t new_data = calloc(1, sizeof( zenith_node_data_t ) );
    size_t datapoints_size = zenith_datapoints_calculate_size( number_of_datapoints );
    zenith_datapoints_handle_t new_datapoints = calloc( 1, datapoints_size );
    ESP_RETURN_ON_FALSE(
        new_data,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating node data"
    );
    new_datapoints->num_datapoints = number_of_datapoints;
    
    new_data->datapoints_handle = new_datapoints;
    new_data->timestamp = time( NULL );

    *out_data = new_data;
    ESP_LOGD(TAG, "Created new node data with %d datapoints", number_of_datapoints);

    return ESP_OK;
}


esp_err_t zenith_registry_copy_node_data( zenith_datapoints_handle_t src, zenith_datapoints_handle_t *dst ) {
    ESP_RETURN_ON_FALSE(
        src,
        ESP_ERR_INVALID_ARG,
        TAG, "Null source data"
    );
    ESP_RETURN_ON_FALSE(
        dst,
        ESP_ERR_INVALID_ARG,
        TAG, "Null destination data pointer"
    );

    size_t data_size = zenith_datapoints_calculate_size( src->num_datapoints );
    ESP_LOGD(TAG, "Copying node data with size: %d", data_size);
    zenith_datapoints_handle_t copy = malloc( data_size);
    ESP_RETURN_ON_FALSE(
        copy,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating node data"
    );

    memcpy( copy, src, data_size );

    *dst = copy;

    return ESP_OK;
}


esp_err_t zenith_registry_get_node_data( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_datapoints_handle_t *data ) {
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_INVALID_ARG, TAG, "Null handle" );
    ESP_RETURN_ON_FALSE( mac, ESP_ERR_INVALID_ARG,TAG, "Null MAC address" );
    ESP_RETURN_ON_FALSE( data, ESP_ERR_INVALID_ARG, TAG, "Null data pointer" );

    zenith_node_handle_t node = NULL;
    ESP_RETURN_ON_ERROR( zenith_registry_get_node( handle, mac, &node ), TAG, "Node not found" );

    // Copy data pointer to output
    zenith_registry_copy_node_data( node->data, data );

    *data = node->data->datapoints_handle;

    return ESP_OK;
}


esp_err_t zenith_registry_set_node_data( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], const zenith_datapoints_handle_t data ) {
    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );
    ESP_RETURN_ON_FALSE(
        mac,
        ESP_ERR_INVALID_ARG,
        TAG, "Null MAC address"
    );
    ESP_RETURN_ON_FALSE(
        data,
        ESP_ERR_INVALID_ARG,
        TAG, "Null data pointer"
    );

    zenith_node_handle_t node = NULL;
    ESP_RETURN_ON_ERROR(
        zenith_registry_get_node( handle, mac, &node ),
        TAG, "Error getting node in set_node_data"
    );

//    if ( node->data->datapoints_handle )
//        free( node->data->datapoints_handle );

    node->data->datapoints_handle  = data;
    return  ESP_OK; 
}


esp_err_t zenith_registry_dispose_node_data( zenith_registry_handle_t handle, zenith_node_data_handle_t *data ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

    ESP_RETURN_ON_FALSE(
        data,
        ESP_ERR_INVALID_ARG,
        TAG, "Null data pointer"
    );

    if ( *data )
    {
        if ( (*data)->datapoints_handle )
            free( (*data)->datapoints_handle );
        free( *data );
        *data = NULL;
    }

    return ret;
}


esp_err_t zenith_registry_create_node_info ( zenith_registry_handle_t registry, zenith_node_info_handle_t *out_info ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

    // Create new info
    zenith_node_info_handle_t new_info = calloc(1, sizeof( zenith_node_info_t ) );
    ESP_RETURN_ON_FALSE(
        new_info,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating node info"
    );

    *out_info = new_info;

    return ret;
}



esp_err_t zenith_registry_get_node_info( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_info_handle_t *info ) {
    ESP_RETURN_ON_FALSE( handle, ESP_ERR_INVALID_ARG, TAG, "Null handle" );
    ESP_RETURN_ON_FALSE( mac, ESP_ERR_INVALID_ARG, TAG, "Null MAC address" );
    ESP_RETURN_ON_FALSE( info, ESP_ERR_INVALID_ARG, TAG, "Null info pointer" );

    zenith_node_handle_t node = NULL;
    ESP_RETURN_ON_ERROR(
        zenith_registry_get_node( handle, mac, &node ),
        TAG, "Node not found"
    );
    *info = node->info;

    return ESP_OK;
}


esp_err_t zenith_registry_set_node_info( zenith_registry_handle_t registry, const uint8_t mac[ESP_NOW_ETH_ALEN], const zenith_node_info_handle_t info ) {
    ESP_RETURN_ON_FALSE( registry, ESP_ERR_INVALID_ARG, TAG, "Null handle" );
    ESP_RETURN_ON_FALSE( mac, ESP_ERR_INVALID_ARG, TAG, "Null MAC address" );
    ESP_RETURN_ON_FALSE( info, ESP_ERR_INVALID_ARG, TAG, "Null info pointer" );

    zenith_node_handle_t node = NULL;
    ESP_RETURN_ON_ERROR(
        zenith_registry_get_node( registry, mac, &node ),
        TAG, "Node not found"
    );

    // Free old info
    if ( node->info ) 
        free( node->info );

    node->info = info;
    _store_to_nvs( registry );

    return ESP_OK;
}


esp_err_t zenith_registry_dispose_node_info( zenith_registry_handle_t registry, zenith_node_info_handle_t *info ) {
    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

    ESP_RETURN_ON_FALSE(
        info,
        ESP_ERR_INVALID_ARG,
        TAG, "Null info pointer"
    );

    if ( *info )
    {
        free( *info );
        *info = NULL;
    }

    return ESP_OK;
}


esp_err_t zenith_registry_create_node ( zenith_registry_handle_t registry, zenith_node_handle_t *node ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

    ESP_RETURN_ON_FALSE(
        node,
        ESP_ERR_INVALID_ARG,
        TAG, "Null node pointer"
    );

    // Create new node
    zenith_node_t* new_node = calloc(1, sizeof( zenith_node_t ) );
    ESP_RETURN_ON_FALSE(
        new_node,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating node"
    );
    ESP_RETURN_ON_ERROR(
        zenith_registry_create_node_info( registry, &new_node->info ),
        TAG, "Error creating node info" 
    );

    ESP_RETURN_ON_ERROR(
        zenith_registry_create_node_data( registry, 0, &new_node->data ),
        TAG, "Error creating node data" 
    );

    *node = new_node;

    return ret;
}

esp_err_t zenith_registry_create_node_from_mac( zenith_registry_handle_t registry, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_handle_t *node ) {
    ESP_RETURN_ON_FALSE(
        mac,
        ESP_ERR_INVALID_ARG,
        TAG, "Null MAC address"
    );

    zenith_node_handle_t new_node = NULL;
    // Check if node is allready registered
    int8_t index = zenith_registry_index_of_mac( registry, mac );
        if ( index >= 0 )
        {
            new_node = registry->nodes[index];
        }  
        else {
            ESP_RETURN_ON_ERROR(
                zenith_registry_create_node( registry, &new_node ),
                TAG, "Error creating node"
            );
            
            // Copy MAC address to node
            memcpy( new_node->info->mac, mac, ESP_NOW_ETH_ALEN );
            ESP_LOGD(TAG, "Creating new node with MAC: "MACSTR, MAC2STR( mac ) );}

    *node = new_node;

    return ESP_OK;
}

esp_err_t zenith_registry_get_node( zenith_registry_handle_t registry, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_handle_t *out_node ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

    ESP_RETURN_ON_FALSE(
        mac,
        ESP_ERR_INVALID_ARG,
        TAG, "Null MAC address"
    );

    ESP_RETURN_ON_FALSE(
        out_node,
        ESP_ERR_INVALID_ARG,
        TAG, "Null node pointer"
    );

    int8_t index = zenith_registry_index_of_mac( registry, mac );

    if ( index < 0 ) {
        // Node not found, create new node
        ESP_RETURN_ON_FALSE(
            registry->count < ZENITH_REGISTRY_MAX_NODES,
            ESP_ERR_NO_MEM,
            TAG, "No room for more nodes"
        );

        zenith_node_handle_t new_node = NULL;
        ESP_RETURN_ON_ERROR(
            zenith_registry_create_node_from_mac( registry, mac, &new_node ),
            TAG, "Error creating node"
        );

        index = registry->count;
        registry->count++;

        registry->nodes[index] = new_node;
    }   

    *out_node = registry->nodes[index];

    return ret;
}

esp_err_t zenith_registry_get_node_by_index( zenith_registry_handle_t registry, const uint8_t index, zenith_node_handle_t *out_node ) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

    ESP_RETURN_ON_FALSE(
        out_node,
        ESP_ERR_INVALID_ARG,
        TAG, "Null node pointer"
    );

    ESP_RETURN_ON_FALSE(
        index < registry->count,
        ESP_ERR_INVALID_ARG,
        TAG, "Index out of bounds"
    );

    *out_node = registry->nodes[index];

    return ret;
}

esp_err_t zenith_registry_dispose_node( zenith_registry_handle_t registry, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_handle_t *node ) {
    ESP_RETURN_ON_FALSE(
        registry,
        ESP_ERR_INVALID_ARG,
        TAG, "Null handle"
    );

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

    int8_t index = zenith_registry_index_of_mac( registry, mac );
    ESP_RETURN_ON_FALSE(
        index >= 0,
        ESP_OK,
        TAG, "Node not found"
    );

    zenith_registry_dispose_node_data( registry, &(*node)->data);
    zenith_registry_dispose_node_info( registry, &(*node)->info );
    // Free node
    free( node );
    registry->nodes[index] = NULL;

    return ESP_OK;
}