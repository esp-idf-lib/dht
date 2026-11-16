/*
 * Copyright (c) 2016 Jonathan Hartsuiker <https://github.com/jsuiker>
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file dht.h
 * @defgroup dht dht
 * @{
 *
 * ESP-IDF driver for DHT11, AM2301 (DHT21, DHT22, AM2302, AM2321), Itead Si7021
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2016 Jonathan Hartsuiker <https://github.com/jsuiker>\n
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>\n
 *
 * BSD Licensed as described in the file LICENSE
 *
 * @note A pull-up resistor of 4.7 to 10 kOhm between the data line and the
 * supply is required. The internal pull-up of the ESP32 is about 45 kOhm and
 * is only a fallback for a breadboard with short wires: with it the edges are
 * slow and the readings become unreliable, which shows up as "phase B" errors.
 * Sensors mounted on a PCB, like the common blue DHT22 modules, already have
 * the resistor.
 *
 * @note A transaction holds the CPU with the interrupts disabled for about
 * 26 ms: the driver has to keep the line low for 20 ms to start the sensor and
 * then read the 40 bits by polling. During that time the other tasks of the
 * same core do not run, the tick is delayed, and Wi-Fi or Bluetooth may lose
 * packets. Do not read the sensor more often than once a second: the sensor
 * heats up and the readings drift.
 *
 * @note The driver is not thread safe in the usual sense: concurrent calls are
 * serialized by an internal lock, but the second caller waits for the whole
 * transaction, and on a dual-core chip it blocks the other core as well.
 *
 * @note DHT_TYPE_SI7021 means a device that speaks this single-wire protocol,
 * not the Silicon Labs Si7021 chip: that chip is an I2C part and cannot be read
 * by this driver. Some small sensors built around it, for example the Itead
 * module, wrap it into a DHT-compatible protocol, and that is the case the
 * type is for.
 *
 */
#ifndef __DHT_H__
#define __DHT_H__

#include <driver/gpio.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sensor type
 */
typedef enum
{
    DHT_TYPE_DHT11 = 0,   //!< DHT11
    DHT_TYPE_AM2301,      //!< AM2301 (DHT21, DHT22, AM2302, AM2321)
    DHT_TYPE_SI7021       //!< Itead Si7021
} dht_sensor_type_t;

/**
 * @brief Read integer data from sensor on specified pin
 *
 * Humidity and temperature are returned as integers.
 * For example: humidity=625 is 62.5 %, temperature=244 is 24.4 degrees Celsius
 *
 * @param sensor_type DHT11, AM2301 or SI7021
 * @param pin GPIO pin connected to sensor OUT; must be an output capable pin
 * @param[out] humidity Humidity, percents * 10, nullable
 * @param[out] temperature Temperature, degrees Celsius * 10, nullable
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if both outputs are NULL, the pin cannot be used
 *        as an output, or the sensor type is unknown
 *      - `ESP_ERR_TIMEOUT` if the sensor did not answer or the frame was
 *        interrupted
 *      - `ESP_ERR_INVALID_CRC` if the checksum does not match
 */
esp_err_t dht_read_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
                        int16_t *humidity, int16_t *temperature);

/**
 * @brief Read float data from sensor on specified pin
 *
 * Humidity and temperature are returned as floats.
 *
 * @param sensor_type DHT11, AM2301 or SI7021
 * @param pin GPIO pin connected to sensor OUT; must be an output capable pin
 * @param[out] humidity Humidity, percents, nullable
 * @param[out] temperature Temperature, degrees Celsius, nullable
 * @return the same codes as dht_read_data()
 */
esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
                              float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif

/**@}*/

#endif  // __DHT_H__
