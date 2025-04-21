# zenith_now - esp now abstraction

## purpose

Handle all network connectivity. Currently only ESP-NOW is supported, but I know I might end up abstracting it and doing bt-le. also thread looks very interesting

### Protocol

[zenith_now.h](include/zenith_now.h)
```mermaid
flowchart TD
 subgraph Payload["Payload"]
    direction TB
        E["zenith_now_payload_ack_t"]
        F["zenith_now_payload_pairing_t"]
        G["zenith_now_payload_data_t"]
  end
 subgraph DataPayload["DataPayload"]
    direction TB
        H["zenith_node_datapoint_t"]
  end
    A["zenith_now_packet_t"] -- 1 byte --> B["type: zenith_now_packet_type_t"] & C["version: uint8_t"]
    A -- Variable --> D["payload: uint8_t[]"]
    D --> E & F & G
    G -- Array --> H
```

```mermaid
classDiagram
    class zenith_now_packet_t {
        +zenith_now_packet_type_t type
        +uint8_t version
        +uint8_t payload[]
    }

    class zenith_now_payload_ack_t {
        +zenith_now_packet_type_t ack_for_type
    }

    class zenith_now_payload_pairing_t {
        +uint8_t flags
    }

    class zenith_node_datapoint_t {
        +uint8_t reading_type
        +uint16_t value
    }

    class zenith_now_payload_data_t {
        +uint8_t num_points
        +zenith_node_datapoint_t datapoints[]
    }

    zenith_now_packet_t --> zenith_now_payload_ack_t : "Payload (ACK)"
    zenith_now_packet_t --> zenith_now_payload_pairing_t : "Payload (Pairing)"
    zenith_now_packet_t --> zenith_now_payload_data_t : "Payload (Data)"
    zenith_now_payload_data_t --> zenith_node_datapoint_t : "Contains multiple"
```

```mermaid
---
title: "Zenith NOW Packet"
---
packet-beta
0-7: "Packet Type"
8-15: "Version"
16-24: "Payload [Variable length]"
```

```mermaid
---
title: "Zenith NOW data payload"
---
packet-beta
0-7: "[uint8] Reading type"
8-24: "[int16] Reading value"
```

```mermaid
---
title: "Zenith NOW ack payload"
---
packet-beta
0-7: "[uint8] Type of packet that the ack is for"
```

```mermaid
---
title: "Zenith NOW pairing payload"
---
packet-beta
0-7: "[uint8] Flag - future use for things like protocol supported, number of sensors, version of node fw"
```

