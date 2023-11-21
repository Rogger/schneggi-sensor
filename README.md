# Schneggi sensor 🐌
A low power ZigBee temperature and humidity sensor integrated with HomeAssistant.

## Design Goals
- High accuracy temperature and humidity measurements ([Sensirion SHTC3](https://www.sensirion.com/products/catalog/SHTC3/))
- Designed for low power consumption and long battery life
  - Hardware: nRF52840 chip, low-current linear regulator, battery monitor with on/off capability 
  - Software: Deep sleep
- Can be powered by
  - Battery with JST connector and 3V to 6,5V (e.g. 350mAh li-poly)
  - Power supply with USB-C connector
- Small footprint (5,5cm x 4,5cm)
- Tested with [Home Assistant](https://www.home-assistant.io/) and [SkyConnect](https://www.home-assistant.io/skyconnect/)

## PCB
The PCB was designed with KiCad 6 and manufactured/assembled with JLCPCB. All relevant files can be found at [JLCPCB](hardware/E73-2G4M08S1C-52840/jlcpcb)

![image](https://github.com/Rogger/schneggi-sensor/assets/371835/be12c999-ec39-4161-95e4-73d42402109f)

## Case
A 3D printable case was generated with the help of [Ultimate Box Maker](https://github.com/jbebel/Ultimate-Box-Maker).

![image](https://github.com/Rogger/schneggi-sensor/assets/371835/ab5e8014-a209-424d-a499-19e3f091526b)
![image](https://github.com/Rogger/schneggi-sensor/assets/371835/b7e83cb2-98f3-4bfb-9744-4b7735aa9ba5)

## Resources
- The PCB design was inspired by [Getting Started With nRF52 MCU in a PCB](https://resources.altium.com/p/getting-started-nrf52-mcu-pcb#getting-started-schematics)
- The repos [zigbee-plant-sensor](https://github.com/stanvn/zigbee-plant-sensor) and [b-parasite](https://github.com/rbaron/b-parasite) are a great start
- The [nRF Sniffer](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/zigbee/tools.html) with [https://www.wireshark.org](Wireshark) is a helpful tool to debug Zigbee communication
- The [ZigBee specification 22 1.0](https://csa-iot.org/wp-content/uploads/2022/01/docs-05-3474-22-0csg-zigbee-specification-1.pdf) and [ZigBee Cluster Library R8](https://csa-iot.org/wp-content/uploads/2022/01/docs-05-3474-22-0csg-zigbee-specification-1.pdf)  
