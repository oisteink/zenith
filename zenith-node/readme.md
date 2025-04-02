# Zenith Node

Battery powered sensor node that wakes up and reports values to the core at a set interval. It's fairly fault tolerand and self healing.

The node does not retain any information in NVS/Flash and uses the same logic from cold and warm boots. Pairing information is stored in RTC memory in between deep-sleeps. If there's consistent issues with sending data packets it will re-enter pairing mode. The sensor data and information is stored in the core unit - identified by node's mac address.

Current hardware:
- [ESP32c6 super mini](https://www.fibel.no/product/esp32-c6-super-mini/)
- [AHT30 temperature and humidity sensor](https://www.fibel.no/product/aht30-temperatur-og-fuktighetssensor/)
- Single cell LiPo battery from a drone I crashed and never reparied....

## Logic

- Pair if needed
- Read sensor
- Send data
- Deep sleep

```mermaid
flowchart TD
    A1["Initialize"]
    A1 --> A2{"Missed packets ≥ 10?"}
    A2 -->|Yes| B1
    A2 -->|No| A4{"Paired MAC in RTC?"}
    A4 -->|No| B1
    A4 -->|Yes| C1
    A5["Deep sleep"]
    B1["Start Pairing"] --> B2["Broadcast Request"]
    B2 --> B3{"Await ACK?"}
    B3 -->|Received| B5["Store MAC"]
    B3 -->|Timeout| B6["Increase Missed Counter"]
    B6 --> B7{"Missed ≥5?"}
    B7 -->|Yes| A5
    B7 -->|No| B2
    B5 --> C1
    C1["Start Data Mode"] --> C2["Initialize Sensor"]  
    C2 --> C3["Read Values"]  
    C3 --> C4["Send Data"]  
    C4 --> C5{"Ack Received?"}  
    C5 -->|Yes| C6["Reset missed packets"]  
    C5 -->|No| C7["Increment missed packets"]  
    C6 --> A5
    C7 --> A5
```
