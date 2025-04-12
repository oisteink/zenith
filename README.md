# ZENITH - Zigbee ENabled Iot Temperature and Humidity

A small personal project for learning about ESP-IDF, Iot protocols and refreshing C knowledge

## Idea

System will consist of a central unit (Zenith Core) with screen that receives data from remote sensors (Zenith Nodes) over ESP-NOW. This information can be presented over Zigbee as Zigbee HA sensors

### Core

Core will have a screen to display data, and the option to relay the data over Zigbee HA to a home automation system like Home Assistant.
The Core will also have its own sensor. 

### Node

Simple Temp/Humidity sensor
Nodes can be battery powered and will enter deep-sleep between reports. 

## Project stucture

- zenith-components: components
- zenith-core: core
- zenith-node: node

## Setup for Node / Core

The core uses 802.15.4, so at current that means C6, H2, H4. I'm using C6.
```
idf.py set-target ESP32C6
ifd.py menuconfig
idf.py reconfigure
```
