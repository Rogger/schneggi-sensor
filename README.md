# Schneggi sensor 🐌
A low power ZigBee temperature and humidity sensor integrated with [Home Assistant](https://www.home-assistant.io/).

- High accuracy temperature and humidity measurements ([Sensirion SHTC3](https://www.sensirion.com/products/catalog/SHTC3/))
- Low power consumption and long battery life. E.g. a 350mAH li-poly lasts for 2+ years (15uA avg).
  - Hardware: nRF52840 chip, low-current linear regulator, battery monitor with on/off 
  - Software: Deep sleep
- Can be powered by
  - Battery with JST connector and 3V to 6,5V (e.g. 350mAh li-poly)
  - Power supply with USB-C connector
- Small footprint (5,5cm x 4,5cm)
- Tested with [Home Assistant](https://www.home-assistant.io/) and [SkyConnect](https://www.home-assistant.io/skyconnect/)

## PCB
The PCB was designed with KiCad 6 and manufactured/assembled with JLCPCB. All relevant files can be found in the [repo](hardware/E73-2G4M08S1C-52840)

![E73-2G4M08S1C-52840](https://github.com/Rogger/schneggi-sensor/assets/371835/75e2d0dc-5a9e-4166-911b-2f92178287d6)

## Case
A 3D printable case was generated with the help of [Ultimate Box Maker](https://github.com/jbebel/Ultimate-Box-Maker).

![Ultimate_Box](https://github.com/Rogger/schneggi-sensor/assets/371835/782dbbfe-b442-4105-b787-0a193e914e4e)

## HomeAssistant
Works out of the box

![Screenshot from 2023-11-21 21-22-59](https://github.com/Rogger/schneggi-sensor/assets/371835/cefebd48-2896-4cb5-bfa6-d6ba029c3226)

![Screenshot from 2023-11-21 21-27-04](https://github.com/Rogger/schneggi-sensor/assets/371835/61f52585-6c13-4973-a659-87bb132f250d)

## How to build?
- Install VSCode + nRF plugins.
- Install nRF SDK and toolchain 2.5.0
- Add a build configuration with custom board `adafruit_feather_nrf52840`
- You can program the board with the nRF52840 DK, the following wiring was used https://devzone.nordicsemi.com/f/nordic-q-a/84893/programming-a-custom-board-with-an-nrf52840-dev-kit. Additionally the pin RESET is connected, for automatic board reset.

## Resources
- The PCB design was inspired by [Getting Started With nRF52 MCU in a PCB](https://resources.altium.com/p/getting-started-nrf52-mcu-pcb#getting-started-schematics)
- The repos [zigbee-plant-sensor](https://github.com/stanvn/zigbee-plant-sensor) and [b-parasite](https://github.com/rbaron/b-parasite) are a great start
- The [nRF Sniffer](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/zigbee/tools.html) with [https://www.wireshark.org](Wireshark) is a helpful tool to debug Zigbee communication
- The [ZigBee specification 22 1.0](https://csa-iot.org/wp-content/uploads/2022/01/docs-05-3474-22-0csg-zigbee-specification-1.pdf) and [ZigBee Cluster Library R8](https://zigbeealliance.org/wp-content/uploads/2021/10/07-5123-08-Zigbee-Cluster-Library.pdf)
