# ZENITH - Zigbee ENabled Iot Temperature and Humidity

A small personal project for learning about ESP-IDF, Iot protocols and refreshing C knowledge

## Idea

System will consist of a central unit (core) with screen that receives data from remote sensors (nodes) over ESP-NOW, and relay this info over Zigbee using Zigbee HA

### Core

Core will have a screen to display data, and the option to relay the data over Zigbee HA to a home automation system like Home Assistant.
The Core will also have its own sensor. 

### Mode

Simple Temp/Humidity sensor
Nodes can be battery powered and will enter deep-sleep between reports. 

## Todo

In random order
- [ ] Get screen working
- [ ] Zigbee HA
- [ ] Core user interface
- [ ] Node user interface
- [ ] Solid pairing
- [ ] Saving config to NVS
- [ ] Reset of device
