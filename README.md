# Silicon Labs firmware builder repository

This repository contains Dockerfiles and GitHub actions that build Silicon Labs firmware. It also hosts unofficial Zigbee Coordinator and Thread (OpenThread) firmware builds that community members can experiment with at your own risk.

The firmware builder uses the Silicon Labs [Gecko SDK (GSDK)](https://github.com/SiliconLabs/gecko_sdk) and proprietary Silicon Labs tools such as the Silicon Labs Configurator (slc) and the Simplicity Commander standalone utility. This is a fork of the [Silabs firmware builder by Nabu Casa](https://github.com/NabuCasa/silabs-firmware-builder), adding support for additional radio adapter hardware models.

Again, please note that the pre-compiled firmware builds hosted in this repository are both unofficial and experimental or cutting-edge releases with  minimal testing which may brick your radio adapter so that it requires manual recovery via a compatible debug probe adapter, however, the builds offered via the Web Flasher are the latest versions recommended by the community.


## Legacy firmware builds

If you are looking for Gecko v7 builds, they can be found here
[Legacy Gecko SDK builds](https://github.com/darkxst/silabs-firmware-builder/tree/main/firmware_builds)

## Supported hardware:
* Sonoff ZBDongle-E by ITead (based on EFR32MG21) – includes Zigbee NCP, Zigbee Router (`manifests/sonoff/zbdonglee_zigbee_router.yaml`), and OpenThread RCP
* Sonoff Dongle Plus Mg24 (based on EFR32MG24)
* Sonoff iHost by ITead (also based on EFR32MG21 and uses the same firmware as Sonoff ZBDongle-E)
* EasyIot ZB-GW04 v1.1 and v1.2 (based on EFR32MG21)
* SMLIGHT SLZB-07 (which may require unlocked [bootloader](https://github.com/darkxst/silabs-firmware-builder/raw/main/firmware_builds/slzb-07/BTL_SLZB07.gbl) first) (based on EFR32MG21)
* SMLIGHT SLZB-07Mg24 (based on EFR32MG24)
* SMLIGHT SLZB-06M (based on EFR32MG21)
* SMLIGHT SLZB-06Mg24 (based on EFR32MG24)
* SMLIGHT SLZB-06Mg26 (based on EFR32MG26)
* Aeotec Zi-Stick (model “ZGA008” based on EFR32MG21)
* Sparkfun Things Matter MGM240P (requires [bootloader](https://github.com/darkxst/silabs-firmware-builder/blob/main/firmware_builds/mgm240p/bootloader-uart-xmodem_NCP.hex) to be flashed first using Silabs [Simplicity Commander](https://community.silabs.com/s/article/simplicity-commander?language=en_US)) (based on EFR32MG24)
* SeeedStudio Xiao MG24 (based on EFR32MG24), requires bootloader to be [flashed with OpenOCD](https://github.com/darkxst/silabs-firmware-builder/issues/188#issuecomment-2670476887)
* Xiaomi Multimode Gateway 2/Mi Smart Hub Pro 2 (based on EFR32MG21)
* Elelabs Zigbee USB Adapter ELU013 (based on EFR32MG13P) [legacy only, 7.4.x builds]
* Elelabs Zigbee Raspberry Pi Shield ELR023 (based on EFR32MG13P) [legacy only, 7.4.x builds]

## Different firmware variants

Four network protocol application firmware variants are available:

* **EmberZNet NCP** = Zigbee NCP (Network Co-Processor) is used as a dedicated Zigbee Coordinator for Zigbee-only environments, for direct use with Zigbee2MQTT, Home Assistant's ZHA integration, other Zigpy based Zigbee Gateway implementations, or other Zigbee gateways/frameworks that support the EZSP (EmberZNet Serial Protocol) interface.
* **Zigbee Router** = Dedicated Zigbee 3.0 Router / repeater firmware (`src/zigbee_router`) with full support for custom EUI64 (IEEE) addresses stored in NVM3 or `USERDATA`, as well as interactive serial CLI commands for viewing/changing the EUI64 and managing network steering.
* **OpenThread RCP** firmware (experimental) = This [Thread](https://en.wikipedia.org/wiki/Thread_(network_protocol)) RCP (Radio Co-Processor) is used directly as a dedicated Thread Border Router in Thread-only environments, used for [OpenThread Border Router add-on](https://github.com/home-assistant/addons/blob/master/openthread_border_router/DOCS.md) or wpantund.
* **RCP Multi-PAN** (no longer recommended) = Multiprotocol firmware for concurrent communication over Zigbee and Thread via Home Assistant [SiliconLabs Multiprotocol add-on](https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md).

**Note!** RCP MultiPAN in multiprotocol mode is no longer available. Running multi-protocol with multiple active networks on a single radio adapter has proven unstable when using Zigbee and Thread simultaneously. It also increases software component complexity. If you're currently using RCP Multi-PAN, it's highly recommended to migrate to separate dedicated radio adapters (using Zigbee NCP and Thread RCP firmware respectively), even if your current setup appears to be working fine. Note that we do not support Sonoff's re-branded multi-PAN attempts either.

External reference explaining these different co-processor designs at a high level:
  * https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md
  * https://openthread.io/platforms/co-processor

## Web Flasher
Flash directly from your browser (only Chrome and Edge supported) [SL Web Flasher](https://darkxst.github.io/silabs-firmware-builder/)

Read this [blog post](https://dialedin.com.au/blog/sonoff-zbdongle-e-rcp-firmware) for more details and instructions for using RCP Multi-pan firmware.

## Home Assistant Add-on
**RCP MultiPan firmware ONLY**  
You can install this HA add-on to keep your dongle up to date with latest the RCP  Multi-Pan firmware. This is only for use if you are using the [SiliconLabs Multiprotocol add-on](https://github.com/home-assistant/addons/blob/master/silabs-multiprotocol/DOCS.md).

[Multipan Flasher Addon](https://github.com/darkxst/multipan_flasher/tree/main)

It can also be used for the initial conversion to MultiPan firmware, however you will need to disable ZHA or Zigbee2MQTT while you do this initial flash. You can then install and configure the Silabs Multiprotocol Add-on.

## Pre-Compiled Firmware
Firmware builds can be found in the [firmware_builds](https://github.com/darkxst/silabs-firmware-builder/tree/main/firmware_builds) folder.

`ncp-uart-hw-` EmberZnet pure Zigbee  
`rcp-uart-802154-` RCP MultiPan  
`ot-rcp-` OpenThread Only  

ZBDongle-E and ZB-GW04 v1.1 do not support hardware flow control. Yellow, SkyConnect and ZB-GW04 v1.2 are built with hardware flow control. Various baudrates are available as listed at the end of the filename.

Use NabuCasa's [Universal-Silabs-Flasher](https://github.com/NabuCasa/universal-silabs-flasher) to flash the `.gbl` files.


## Zigbee Router with Custom EUI64 (IEEE Address) Support

When migrating a Zigbee network to a new Coordinator by cloning the old adapter's IEEE/EUI64 address, the old adapter cannot join the network as a Zigbee Router unless its EUI64 address is changed.

Standard Silicon Labs Zigbee Router firmware ignores custom EUI64 addresses stored in NVM3 (`TOKEN_STACK_RESTORED_EUI64`) and always falls back to the factory hardware address (`TOKEN_MFG_EMBER_EUI_64` in `DEVINFO`). In this repository, `src/zigbee_router` has been extended to support custom EUI64 addresses and includes a dedicated build manifest for the **SONOFF ZBDongle-E** (`manifests/sonoff/zbdonglee_zigbee_router.yaml`).

### How Custom EUI64 Resolution Works
1. **Priority order on boot (`sl_zigbee_get_restored_eui64`)**:
   1. **NVM3 Restored EUI64 (`TOKEN_STACK_RESTORED_EUI64` / `0x1E12A`)**: Written by tools such as `universal-silabs-flasher write-ieee`, `bellows`, `ember-zli`, or the router's built-in `eui64 set` serial CLI command.
   2. **USERDATA Custom EUI64 (`TOKEN_MFG_CUSTOM_EUI_64`)**: Written to the `USERDATA` flash page via Simplicity Commander or bootloader. If present, it is also mirrored into `TOKEN_STACK_RESTORED_EUI64` in NVM3.
   3. **Factory Hardware EUI64 (`TOKEN_MFG_EMBER_EUI_64` in `DEVINFO`)**: Used only if neither custom token contains a valid address (i.e. not `FF:FF:FF:FF:FF:FF:FF:FF` and not `00:00:00:00:00:00:00:00`).
2. **Automatic Coordinator-to-Router NVRAM Cleanup**:
   - If the adapter previously ran Coordinator (NCP) firmware and still has non-router network data in `TOKEN_STACK_NODE_DATA` (`nodeType != SL_ZIGBEE_ROUTER`), the router firmware automatically resets `TOKEN_STACK_NODE_DATA` on first boot while preserving the custom EUI64 in `TOKEN_STACK_RESTORED_EUI64`.

### Router Serial CLI Commands (`115200` baud, 8N1)
Connect to the router's USB serial port at `115200` baud (no flow control) using any serial terminal (e.g. PuTTY, screen, or Chrome Serial Terminal) to inspect or change the EUI64 directly on the running router firmware:

| Command | Description |
| :--- | :--- |
| `eui64 info` | Prints the `Active EUI64`, `NVM3 Restored EUI64`, `USERDATA Custom EUI64`, and `Hardware EUI64`. |
| `eui64 set <EUI64>` | Saves a new custom EUI64 (e.g. `eui64 set 00:12:4B:00:29:AA:BB:CC`) to NVM3 and reboots. |
| `eui64 reset` | Clears the custom EUI64 in NVM3 (`FF:FF:FF:FF:FF:FF:FF:FF`) and reboots. |
| `bootloader info` | Displays the installed Gecko Bootloader type and version. |
| `bootloader reboot` | Reboots the adapter into Gecko Bootloader firmware download mode. |
| `network leave` | Leaves the current Zigbee network. |
| `plugin network-steering start 0` | Starts Zigbee network steering (pairing) to join an open network. |

### Building via GitHub Actions & Flashing

1. **Automated Cloud Build (GitHub Actions)**:
   - Pushing to a branch or opening a pull request triggers `.github/workflows/build.yaml`, which builds all manifests in `manifests/` (including `manifests/sonoff/zbdonglee_zigbee_router.yaml`) using the prebuilt Docker container image.
   - Once the workflow completes, download the `.gbl` artifact (e.g. `firmware-build-zbdonglee_zigbee_router_8.0.3.0_115200` -> `zbdonglee_zigbee_router_8.0.3.0_115200.gbl`) from the **Actions** tab on GitHub.
2. **Flashing the `.gbl` Firmware**:
   - **Browser Web Flasher**: Open [SONOFF Dongle Flasher](https://dongle.sonoff.tech/sonoff-dongle-flasher/) or [SL Web Flasher](https://darkxst.github.io/silabs-firmware-builder/) in Chrome/Edge, connect to the dongle, choose **Change Firmware -> Custom Firmware**, and select `zbdonglee_zigbee_router_8.0.3.0_115200.gbl`.
   - **Command Line**:
     ```sh
     universal-silabs-flasher --device COMx flash --firmware zbdonglee_zigbee_router_8.0.3.0_115200.gbl
     ```


## Building locally with VSCode

To build firmware locally, simply open this repository in the included devcontainer.

You can then use the `Run Task` feature and select "Select Manifest" to choose the manifest you want to build.

To build manually and keep build artifacts, run:
```sh
./tools/buildit.sh <manifest.yaml> --no-clean-build-dir
```

## Support

If you would like to help support further development of my `Silabs firmware` projects consider buying me a coffee!

<a href="https://www.buymeacoffee.com/darkxst" target="_blank"><img src="img/blue-button.png" alt="Buy Me A Coffee" height="41" width="174"></a>
