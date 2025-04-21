# Zenith Core

The central unit that node units report their sensor data to the core at set intervals. This data is presented on a screen and can optionally be sent over Zigbee HA to a Zigbee home automation system.

To be implemented:
- Storing data
- New UI
- Node configuration
  - Report interval
- Zigbee HA

## Logic

- Event loop for receiving data
- Touch enabled UI
- Timer that loops through stored nodes when idle

app main logic

```mermaid
sequenceDiagram
    participant AppMain
    participant NVS
    participant Registry
    participant UI
    participant Blinker
    participant ZenithNow
    participant ZenithNode

    AppMain->>NVS: Initialize NVS (initialize_nvs)
    NVS-->>AppMain: ESP_OK

    AppMain->>Registry: Create and initialize registry
    Registry-->>AppMain: ESP_OK

    AppMain->>UI: Configure and initialize UI
    UI-->>AppMain: ESP_OK

    AppMain->>Blinker: Initialize blinker (init_zenith_blink)
    Blinker-->>AppMain: ESP_OK

    AppMain->>ZenithNow: Configure Zenith Now (configure_zenith_now)
    ZenithNow-->>AppMain: ESP_OK

    AppMain->>ZenithNow: Set RX callback (core_rx_callback)

    ZenithNode->>ZenithNow: Send packet (pairing or data)
    ZenithNow->>AppMain: Trigger RX callback (core_rx_callback)

    AppMain->>Registry: Check if peer is in registry
    Registry-->>AppMain: Peer not found (reg_index < 0)

    AppMain->>Registry: Add peer to registry
    Registry-->>AppMain: ESP_OK

    AppMain->>ZenithNow: Send ACK for pairing or data
    ZenithNow-->>ZenithNode: ACK sent

    AppMain->>Blinker: Blink for pairing or data received
    Blinker-->>AppMain: Blink complete

    AppMain->>Registry: Store received data in registry
    Registry-->>AppMain: Data stored

    loop Staying Alive
        AppMain->>AppMain: vTaskDelay(portMAX_DELAY)
    end
```

```mermaid
flowchart TD
    A["app_main()"] --> B["Initialize the peer list"]
    B --> C["Initialize ESP-NOW"]
    C --> D["Initialize event loop"]
    D --> E["Initialize Screen"]
    E --> F["Initialize display timer"]
    F --> G["Idle loop"]
```

Event loop logic
```mermaid
flowchart TD
    E1["ESP-NOW Initialize"] --> E2["Wait for Event"]
    E2 --> E3{"Event Type?"}
    E3 -->|Pairing Request| E4["Add MAC to Nodes List"] --> E5["Send ACK"]
    E3 -->|Data Packet| E6["Update Values in Nodes List"]
    E5 & E6 --> E2
```

Timer logic
```mermaid
flowchart TD
    T1["Timer Initialize (30s interval)"] --> T2["Wait 30s"]
    T2 --> T3["Access Nodes List<br>(Read Only)"]
    T3 --> T4{"Any Nodes?"}
    T4 -->|No| T5["Display Nothing"] --> T2
    T4 -->|Yes| T6["Get Next Node Index<br>(Last + 1 mod count)"]
    T6 --> T7["Display Node Data"] --> T8["Store Current Index"]
    T8 --> T2
```

Data access
```mermaid
flowchart LR
    subgraph NodesList["Nodes List (Shared State)"]
        direction TB
        N1["MAC1: Temp/Humidity"] 
        N2["MAC2: Temp/Humidity"]
    end
    
    TimerThread -->|Reads| NodesList
    EventThread -->|Writes| NodesList
```
