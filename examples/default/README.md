# Example for `dht` driver

## What it does

It reads the sensor once a second and prints the temperature and the humidity.
A failed read is retried up to three times before the error is printed.

## Wiring

Connect the sensor to the ESP32:

| Sensor | ESP32 |
|--------|-------|
| `VCC` | 3.3 V |
| `DATA` | `CONFIG_EXAMPLE_DATA_GPIO`, see below |
| `GND` | GND |

`DATA` must have a pull-up resistor of 4.7 to 10 kOhm to `VCC`. Modules on a PCB
usually have one; for a bare sensor add it yourself.

| Name | Description | Defaults |
|------|-------------|----------|
| `CONFIG_EXAMPLE_DATA_GPIO` | GPIO number for the `DATA` pin | "4" for `esp32c2`, `esp32c3`, `esp32c5`, `esp32c6`, `esp32c61`, `esp32h2`, `esp32p4`; "17" for `esp32`, `esp32s2`, `esp32s3` |

## Configuration

`menuconfig` -> `Example configuration`:

- the chip type: DHT11, DHT21/DHT22/AM2301/AM2302/AM2321 (default) or Itead
  Si7021;
- the data GPIO number;
- enable the internal pull-up resistor if there is no external one. It is a
  fallback for a breadboard with short wires: about 45 kOhm instead of the
  required 4.7 to 10 kOhm makes the edges slow and the readings unstable.

## Build and run

```sh
idf.py build flash monitor
```

## Expected output

```
Humidity: 45.3% Temp: 22.4C
Humidity: 45.4% Temp: 22.4C
Could not read data from sensor: ESP_ERR_TIMEOUT
```

The first read happens a second after the start: the sensor needs that time
after the power is applied.

## Notes

- The DHT protocol has no clock line, so a read is sensitive to interrupts and
  to the length of the wire. If you see `Initialization error, problem in
  phase 'B'` in the log, check the pull-up and the pin first.
- Do not read the sensor more often than once a second: it heats up and the
  values drift.
- `Si7021` in the menu means a device that speaks this protocol, not the
  Silicon Labs chip: that one is an I2C part.
