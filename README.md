# Schneggi sensor 🐌
A low-power ZigBee sensor integrated with HomeAssistant for monitoring temperature, humidity, and CO2.

- High accuracy temperature and humidity measurements ([Sensirion SHTC3](https://www.sensirion.com/products/catalog/SHTC3/))
- High accuracy CO2 measurement ([Sensirion SCD40](https://sensirion.com/products/catalog/SCD40)) - optional board
- Low power consumption and long battery life
  - Hardware: nRF52840 chip, low-current linear regulator, battery monitor with on/off capability 
  - Software: Zigbee sleepy end device
  - Measured: 3uA average over 1 minute (without CO2). 3mA average over 1 minute (with CO2 low-power mode)
- Can be powered with 3V to 6V (JST connector), e.g. 350mAh li-poly.
- Small footprint (5cm x 4,5cm)
- Tested with [Home Assistant](https://www.home-assistant.io/) and [SkyConnect](https://www.home-assistant.io/skyconnect/)

## PCB
The PCB was designed with KiCad 7 and manufactured/assembled with JLCPCB. All relevant files can be found in the [repo](hardware)

![base](https://github.com/user-attachments/assets/ae48fca6-d7ca-4260-b0ca-0d7fdca1a1e8)
![co2](https://github.com/user-attachments/assets/a420414e-1857-45f5-8971-0bba9cf12d0e)

## HomeAssistant

![image](https://github.com/user-attachments/assets/fe9de769-8348-4b40-8632-f8fcfae44b9a)

## How to build?
- Install VSCode + nRF plugins
- Install nRF SDK and toolchain 2.5.0
- Add a build configuration with custom board `adafruit_feather_nrf52840`

## Resources
- The PCB design was inspired by [Getting Started With nRF52 MCU in a PCB](https://resources.altium.com/p/getting-started-nrf52-mcu-pcb#getting-started-schematics)
- The repos [zigbee-plant-sensor](https://github.com/stanvn/zigbee-plant-sensor) and [b-parasite](https://github.com/rbaron/b-parasite) are a great start
- The [Zyphr driver for SCD40](https://github.com/nobodyguy/sensirion_zephyr_drivers) 
- The [nRF Sniffer](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/zigbee/tools.html) with [https://www.wireshark.org](Wireshark) is a helpful tool to debug Zigbee communication
- The [ZigBee specification 22 1.0](https://csa-iot.org/wp-content/uploads/2022/01/docs-05-3474-22-0csg-zigbee-specification-1.pdf) and [ZigBee Cluster Library R8](https://zigbeealliance.org/wp-content/uploads/2021/10/07-5123-08-Zigbee-Cluster-Library.pdf)
