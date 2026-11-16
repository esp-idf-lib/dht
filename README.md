# esp-idf-lib/dht

[![Build examples](https://github.com/esp-idf-lib/dht/actions/workflows/build.yml/badge.svg)](https://github.com/esp-idf-lib/dht/actions/workflows/build.yml)
[![Build docs](https://github.com/esp-idf-lib/dht/actions/workflows/build-docs.yml/badge.svg)](https://github.com/esp-idf-lib/dht/actions/workflows/build-docs.yml)
[![Validation](https://github.com/esp-idf-lib/dht/actions/workflows/validate-component.yml/badge.svg)](https://github.com/esp-idf-lib/dht/actions/workflows/validate-component.yml)

Driver for DHT11, AM2301 (DHT21, DHT22, AM2302, AM2321) and for devices that
speak the same single-wire protocol, for example small modules built around
the Silicon Labs Si7021.

* [Documentation](https://esp-idf-lib.github.io/dht/)
* [Repository](https://github.com/esp-idf-lib/dht)
* [Issues](https://github.com/esp-idf-lib/dht/issues)
* [Discussions and questions](https://github.com/esp-idf-lib/core/discussions)
* [Component page at the ESP Component Registry](https://components.espressif.com/components/esp-idf-lib/dht)

## Features

- One data pin, no clock line: the driver sends a start pulse and shifts in the
  40-bit answer.
- Integer and floating point APIs: tenths of a percent and tenths of a degree,
  or `float`.
- DHT11 and the 16-bit AM2301 family are decoded by their own rules, including
  the sign bit of the temperature.
- Checksum is verified, the checksum error is reported as `ESP_ERR_INVALID_CRC`.

## Wiring

| Sensor | ESP32 |
|--------|-------|
| `VCC` | 3.3 V (DHT11 also runs from 5 V) |
| `DATA` | any GPIO that can be an output, for example 4 or 17 |
| `GND` | GND |

`DATA` needs a pull-up resistor of 4.7 to 10 kOhm to `VCC`. Sensors on a PCB,
like the common blue DHT22 modules, already have one. The internal pull-up of
the ESP32 is about 45 kOhm: it is enough for a breadboard with short wires, but
the edges become slow and the readings unreliable, which usually shows up as
`Initialization error, problem in phase 'B'`.

Long wires make it worse: the sensor drives the line with an open drain, and a
capacitive cable turns every edge into a slow ramp. Keep the wire short, or use
a smaller pull-up.

## How a read works

The driver pulls the line low for 20 ms (500 us for the Si7021 type), releases
it, waits for the sensor to answer with an 80 us low and an 80 us high, and then
reads 40 bits. Every bit starts with a 50 us low, and the length of the high
that follows tells whether it is a zero (about 27 us) or a one (about 70 us).

The driver polls the line every two microseconds and measures the pulses with
`esp_timer_get_time()`, so the whole transaction, including the start pulse,
runs with the interrupts disabled on the calling core for about 26 ms. The
other tasks of that core do not run during this time, and Wi-Fi or Bluetooth
may lose packets: do not read the sensor from a real-time task.

Do not read the sensor more often than once a second. The sensor heats up
itself, and both the temperature and the humidity drift.

## Usage

```c
#include <dht.h>

#define SENSOR_PIN GPIO_NUM_4

void read_sensor(void)
{
    float temperature, humidity;

    esp_err_t err = dht_read_float_data(DHT_TYPE_AM2301, SENSOR_PIN, &humidity, &temperature);
    if (err == ESP_OK)
        printf("Humidity: %.1f%%, temperature: %.1f C\n", humidity, temperature);
    else
        printf("Could not read the sensor: %s\n", esp_err_to_name(err));
}
```

`dht_read_data()` returns the same values as integers in tenths:

```c
int16_t humidity, temperature;

if (dht_read_data(DHT_TYPE_DHT11, SENSOR_PIN, &humidity, &temperature) == ESP_OK)
    printf("Humidity: %d.%d%%, temperature: %d.%d C\n",
           humidity / 10, humidity % 10, temperature / 10, temperature % 10);
```

Either output may be `NULL` if only one value is needed, but not both.

Errors:

- `ESP_ERR_INVALID_ARG` — both outputs are `NULL`, the pin cannot drive a line
  (input-only pins like GPIO34..39 on the ESP32), or the sensor type is unknown;
- `ESP_ERR_TIMEOUT` — the sensor did not answer, or the frame was interrupted;
- `ESP_ERR_INVALID_CRC` — the checksum does not match.

The error log tells which phase failed: `'B'` means the sensor did not pull the
line low after the start pulse, which in practice almost always means a missing
or too weak pull-up, a wrong pin or an unpowered sensor.

## Sensor types

| Type | Sensor | Resolution |
|------|--------|------------|
| `DHT_TYPE_DHT11` | DHT11 | 1 % RH, 1 C |
| `DHT_TYPE_AM2301` | DHT21, DHT22, AM2301, AM2302, AM2321 | 0.1 % RH, 0.1 C |
| `DHT_TYPE_SI7021` | a device that speaks this protocol around a Si7021 | as the device reports |

`DHT_TYPE_SI7021` does not mean the chip itself: the Silicon Labs Si7021 is an
I2C part and cannot be read by this driver. The type is for small boards that
wrap it into a DHT-compatible single-wire protocol.

## Configuration

The driver has no options of its own. The example has two: the sensor type and
the data GPIO, plus a switch that enables the internal pull-up.

## Example

[`examples/default`](examples/default) reads the sensor in a loop and prints the
values, retrying a failed read a couple of times.

## Installation

```sh
idf.py add-dependency esp-idf-lib/dht
```

## Support

For questions and discussions about the component, please use
[Discussions](https://github.com/esp-idf-lib/core/discussions)
at [esp-idf-lib/core](https://github.com/esp-idf-lib/core).

## Contributing

Please read [CONTRIBUTING.md](https://github.com/esp-idf-lib/core/blob/main/CONTRIBUTING.md)
at [esp-idf-lib/core](https://github.com/esp-idf-lib/core).
