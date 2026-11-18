# Air Sensor Matter Firmware (XIAO nRF52840)

**Matter over Thread** firmware for the **Seeed XIAO nRF52840** (`xiao_ble/nrf52840`), built with **nRF Connect SDK v3.4.1**. The device is a Thread sleepy end device (ICD with LIT support) that exposes a Temperature Measurement cluster and is commissioned over Bluetooth LE.

**Work in progress.** The project is based on Nordic's Matter *Temperature Sensor* sample. No real sensor is read yet. The measured value is simulated (see [How it works](#how-it-works)).

## Features

- **Matter Temperature Sensor** device type (0x0302) on endpoint 1, with the Identify cluster
- **Thread MTD / sleepy end device**, Long Idle Time (LIT) ICD, TX power 0 dBm
- **BLE commissioning** starts automatically on boot, with extended advertising
- **No bootloader, no OTA, no factory data**, so the application gets the whole UF2 code partition
- **USB CDC ACM console** with the Matter shell enabled in the debug build
- Separate **release** configuration: logs, shell and console disabled, watchdog and LTO enabled

## Hardware

| Item | Details |
| --- | --- |
| Board | Seeed Studio XIAO nRF52840 (`xiao_ble/nrf52840`) |
| SoC | Nordic nRF52840 (1 MB flash, 256 KB RAM) |
| Radio | 802.15.4 (Thread) + Bluetooth LE (commissioning) |
| LEDs | Onboard RGB LED (`led0` red, `led1` green, `led2` blue) used by the Nordic board library for device state |
| Console | USB CDC ACM (`board_cdc_acm_uart`), so no external UART adapter is needed |

No buttons are used. [boards/xiao_ble_nrf52840.overlay](boards/xiao_ble_nrf52840.overlay) defines four placeholder `gpio-keys` (D3–D6) only because `CONFIG_DK_LIBRARY` and the NCS Matter board library need a `buttons` node to build. Nothing is connected to these pins.

## Build & flash

Requires the [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK) v3.4.1 with its toolchain (`west` and `gn` on `PATH`, for example through the nRF Connect for VS Code extension or `nrfutil toolchain-manager`).

```bash
west build -p -b xiao_ble/nrf52840                              # debug build
west build -p -b xiao_ble/nrf52840 -- -DFILE_SUFFIX=release     # release build (prj_release.conf)
```

> Always pass `-b xiao_ble/nrf52840` after a pristine (`-p`) build. Without `-b`, `west` reuses the board cached in `build/`. If that is an nRF52840 DK, the build fails with `region 'FLASH' overflowed`, because that board's default partitions give the app only 472 KB.

Flashing with the factory **Adafruit UF2 bootloader**:

1. Double-tap the reset button. The board mounts as a USB drive.
2. Copy `build/air-sensor-matter-nrf52840/zephyr/zephyr.uf2` to that drive.

Flashing over SWD (J-Link or another debugger) also works (`west flash`). It overwrites the UF2 bootloader.

Flash layout comes from the board's `nrf52840_partition_uf2_sdv7.dtsi`. Partition Manager is disabled (`SB_CONFIG_PARTITION_MANAGER=n`):

| Partition | Address | Size |
| --- | --- | --- |
| Reserved (SoftDevice area of the UF2 bootloader) | 0x00000 | 156 KB |
| Application (`code_partition`) | 0x27000 | 788 KB |
| Settings (`storage_partition`) | 0xEC000 | 32 KB |
| UF2 bootloader | 0xF4000 | 48 KB |

The debug build uses about **703 KB of flash (89%)** and **164 KB of RAM (64%)**. Cost of fitting Matter into the UF2 layout: **no MCUboot, no OTA DFU**.

## Configuration

| What | Where |
| --- | --- |
| Product name and Product ID (still the sample's `32781`) | [prj.conf:4-5](prj.conf#L4-L5) |
| BLE device name | [prj.conf:19](prj.conf#L19) |
| BLE advertising duration (minutes) | [prj.conf:17](prj.conf#L17) |
| Data model (ZAP file) | [prj.conf:6](prj.conf#L6), [src/default_zap/temperature_sensor.zap](src/default_zap/temperature_sensor.zap) |
| Measurement interval (10 s) and simulated step (1 °C) | [src/app_task.h:35-36](src/app_task.h#L35-L36) |
| Sensor read (to be replaced with a real driver) | [src/app_task.cpp:33](src/app_task.cpp#L33) |
| ICD / Thread defaults (LIT, SIT poll limit, TX power, MTD) | [Kconfig](Kconfig) |
| Bootloader, OTA, factory data switches | [sysbuild.conf](sysbuild.conf), [boards/xiao_ble_nrf52840.conf](boards/xiao_ble_nrf52840.conf) |

Factory data is disabled (`CONFIG_CHIP_FACTORY_DATA_NONE=y`), so the device uses the **test onboarding credentials**: passcode `20202021`, discriminator `0xF00` (3840), VID `0xFFF1`. These are fine for development only.

## Project structure

```
src/main.cpp                       entry point, calls AppTask::StartApp()
src/app_task.cpp, app_task.h       Matter init, measurement timer, attribute updates
src/chip_project_config.h          CHIP project config overrides (empty)
src/default_zap/                   ZAP data model (.zap, .matter) and generated code
boards/xiao_ble_nrf52840.overlay   placeholder buttons required by the DK library
boards/xiao_ble_nrf52840.conf      board-specific Kconfig (MCUboot off)
prj.conf                           debug configuration
prj_release.conf                   release configuration (FILE_SUFFIX=release)
sysbuild.conf                      sysbuild: no bootloader, no OTA, no Partition Manager
Kconfig, Kconfig.sysbuild          application and sysbuild Kconfig defaults
```

## How it works

`main()` calls `AppTask::StartApp()`, which:

1. Prepares the Matter server, initializes the Nordic board library (LEDs) and registers the default Matter event handler that drives the status LED.
2. Initializes the Identify cluster on endpoint 1 and starts the Matter server. BLE advertising starts on its own (`CONFIG_CHIP_ENABLE_PAIRING_AUTOSTART=y`).
3. Reads `MinMeasuredValue` (-1000 = -10.00 °C) and `MaxMeasuredValue` (3000 = 30.00 °C) from the data model.
4. Starts a periodic `k_timer` every **10 s**. The timer callback schedules work on the Matter thread (`PlatformMgr().ScheduleWork`), because attributes may only be written from the Matter context. That work updates the value and writes `TemperatureMeasurement::MeasuredValue`.
5. Loops on `Nrf::DispatchNextTask()` to run tasks posted by the board library.

The measured value is in units of 0.01 °C, as the cluster requires. The current `UpdateTemperatureMeasurement()` is a placeholder. It ramps the value by +1 °C per tick from min to max and wraps around.

ICD settings (resolved Kconfig):

| Parameter | Value |
| --- | --- |
| Idle mode duration | 300 s |
| Active mode duration | 300 ms |
| Active mode threshold | 5000 ms |
| Slow poll interval | 300 s |
| SIT slow poll limit | 5000 ms |
| Fast poll interval | 500 ms |

## Usage

1. Flash the firmware and power the board over USB.
2. The device starts BLE advertising for commissioning. It lasts 60 minutes. After that, reset the board to start it again.
3. Commission it from a Matter controller with a Thread Border Router (Apple Home, Google Home, Home Assistant, `chip-tool`). Use the test passcode `20202021` / discriminator `3840`.
4. The temperature attribute updates every 10 s.
5. Factory reset: remove the device from the controller, or run `matter device factoryreset` in the shell (debug build, USB console).

## Known limitations

- **No real sensor** — the temperature is a simulated ramp, so a controller sees values from -10 °C to 30 °C in 1 °C steps.
- **Test credentials** — no factory data partition, so every device has the same passcode and discriminator, and the vendor is the test VID. Not suitable for production.
- **No OTA** — there is no bootloader and no second slot. Updates require UF2 or SWD. The ZAP model still contains the `OtaSoftwareUpdateRequestor` cluster on endpoint 0.
- **Sample identity** — product name, Product ID `32781` and BLE name `MatterTemperature` still come from the Nordic sample.
- **Thin flash margin** — 89% of the 788 KB code partition is used, so there is about 85 KB left for the sensor driver and new clusters in the debug build.

## TODO

- [ ] Read a real air sensor and add the matching clusters (e.g. humidity) to the ZAP model
- [ ] Set own product name, Product ID and BLE device name
- [ ] Remove the OTA cluster from the ZAP model
- [ ] Drop the placeholder buttons (needs LED handling without `CONFIG_DK_LIBRARY`)
