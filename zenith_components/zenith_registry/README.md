# Zenith Registry

The Zenith Registry stores infomration about paired Zenith Nodes on the Zenith Core

## Use case


### Use Case 1: Core receives data 

Actors: Zenith Node, Zenith Core Event Handler, Zenith Registry

1. Create registry
2. Receive data from node
3. Store the data for the node in the registry
5. Release the node

---

### Use Case 2: Zenith Core UI wants to get to the goodies

This will be Very similar to Zigbee perhaps?

Actors: Zenith Core UI, Zenith Registry

1. Receives handle to registry
2. Retrieve data for a node from the registry. No touch, only watch!

---

### Use Case 3: Peering request

Actors: Zenith Node, Zenith Core Event Handler, Zenith Registry, Zenith Core App

1. APP: Create registry
2. Node: sends pairing request to Core
3. Core: Acknowledge pairing request
4. Core: Updates the info section in the registry
5. Registry: Creates a new handle and copies data 
5. Registry: detects existing handle with data.
     - Frees existing handle
5. Registry:
     - Registry creates new handle, with the data.
     - Updates NVS

     - Frees the existing handle
     - Create
5. Copy over the info
5. Registry saves state to NVS

1. 
---

zenith_registry_create( &handle );

zenith_node_data_handle_t node_data;
zenith_registry_get_node_data( handle, node_mac, &node_data );

zenith_node_info_handle_t node_info;
zenith_registry_create_info( handle, &node_info );

zenith_registry_save_node_info( handle, node_mac, node_info );






```mermaid
classDiagram
    class ZenithRegistryNvsBlob {
        +zenith_registry_nvs_header_t header
        +zenith_node_info_t[] nodes
        +_load_from_nvs(handle)
        +_store_to_nvs(handle)
    }

    class ZenithRegistry {
        +uint8_t registry_version
        +uint8_t count
        +zenith_node_handle_t nodes[ZENITH_REGISTRY_MAX_NODES]
        +zenith_registry_create(handle*)
        +zenith_registry_dispose(handle)
        +zenith_registry_retrieve_node_by_index(index, data*)
        +zenith_registry_retrieve_node(mac, data*)
        +zenith_registry_forget_node(index)
        +zenith_registry_count()
        +zenith_registry_index_of_mac(mac)
    }

    class ZenithNodeInfo {
        +uint8_t mac[ESP_NOW_ETH_ALEN]
    }

    class ZenithNodeData {
        +time_t timestamp
        +zenith_datapoints_handle_t datapoints_handle
    }

    class ZenithNode {
        +ZenithNodeInfo info
        +ZenithNodeData data
    }

    ZenithRegistryNvsBlob --> ZenithNodeInfo : contains
    ZenithRegistry --> ZenithNode : contains
    ZenithNode --> ZenithNodeInfo : composes
    ZenithNode --> ZenithNodeData : composes
    ZenithRegistry --> ZenithRegistryNvsBlob : persists via
``` 




```mermaid
sequenceDiagram
    participant App as Application
    participant Reg as ZenithRegistry
    participant NodeData as ZenithNodeInfo
    participant NVS as NVS Storage
    participant Node as ZenithNode

    Note over App: 1. Create Registry
    App->>Reg: zenith_registry_create()
    Reg->>NVS: nvs_get_blob(ZENITH_REGISTRY_NODE_KEY)
    alt Blob exists
        NVS-->>Reg: Existing node data
        Reg->>Reg: Populate nodes array
    else No blob
        NVS-->>Reg: Empty response
        Reg->>Reg: Initialize empty registry
    end
    Reg-->>App: Registry handle

    note over App: 2. Recieve data from node
    Node-->>Reg: assign data payload to node

    Note over App: 3. Fetch the node data from registry
    App->>Reg: zenith_registry_retrieve_node_data(mac)
    Reg->>Reg: Check existing nodes in registry
    alt Node exists
        Reg-->>App: Existing node handle's data
    else New node
        Reg->>Reg: zenith_registry_create_node()
        Reg->>Reg: Add to nodes array
        Reg->>NVS: nvs_set_blob() (persist new state)
        Reg-->>App: New node handle's data
    end



    Note over App: 4. Add Data to Node
    App->>Node: Update timestamp
    App->>Node: Set datapoints_handle
    App->>Reg: Call some kind of commit for the changes we've done
    Reg->>NVS: nvs_set_blob() (persist updated data)

    note over App: 5. 
```