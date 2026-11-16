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
 * @file dht.c
 *
 * ESP-IDF driver for DHT11, AM2301 (DHT21, DHT22, AM2302, AM2321), Itead Si7021
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2016 Jonathan Hartsuiker <https://github.com/jsuiker>\n
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>\n
 *
 * BSD Licensed as described in the file LICENSE
 */
#include "dht.h"

#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>

// DHT timer precision in microseconds
#define DHT_TIMER_INTERVAL 2
#define DHT_DATA_BITS 40
#define DHT_DATA_BYTES (DHT_DATA_BITS / 8)

/*
 *  Note:
 *  A suitable pull-up resistor should be connected to the selected GPIO line
 *
 *  __           ______          _______                              ___________________________
 *    \    A    /      \   C    /       \   DHT duration_data_low    /                           \
 *     \_______/   B    \______/    D    \__________________________/   DHT duration_data_high    \__
 *
 *
 *  Initializing communications with the DHT requires four 'phases' as follows:
 *
 *  Phase A - MCU pulls signal low for at least 18000 us
 *  Phase B - MCU allows signal to float back up and waits 20-40us for DHT to pull it low
 *  Phase C - DHT pulls signal low for ~80us
 *  Phase D - DHT lets signal float back up for ~80us
 *
 *  After this, the DHT transmits its first bit by holding the signal low for 50us
 *  and then letting it float back high for a period of time that depends on the data bit.
 *  duration_data_high is shorter than 50us for a logic '0' and longer than 50us for logic '1'.
 *
 *  There are a total of 40 data bits transmitted sequentially. These bits are read into a byte array
 *  of length 5.  The first and third bytes are humidity (%) and temperature (C), respectively.  Bytes 2 and 4
 *  are zero-filled and the fifth is a checksum such that:
 *
 *  byte_5 == (byte_1 + byte_2 + byte_3 + byte_4) & 0xFF
 *
 */

static const char *TAG = "dht";

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL() portENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL() portEXIT_CRITICAL(&mux)

#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)
#define CHECK(x) do { \
        esp_err_t __ = (x); \
        if (__ != ESP_OK) \
            return __; \
    } while (0)

static bool dht_type_valid(dht_sensor_type_t type)
{
    return type == DHT_TYPE_DHT11 || type == DHT_TYPE_AM2301 || type == DHT_TYPE_SI7021;
}


/**
 * Wait for the pin to go to a specified state, but no longer than 'timeout'
 * microseconds. The time that has passed before the state was seen is
 * returned in 'duration' if it is not NULL.
 *
 * The direction of the pin must be set to the input before the call.
 */
static esp_err_t dht_await_pin_state(gpio_num_t pin, uint32_t timeout,
                                     int expected_pin_state, uint32_t *duration)
{
    int64_t start = esp_timer_get_time();

    for (;;)
    {
        int64_t elapsed;

        // a sample takes a couple of microseconds, so wait first and only then
        // look at the line: this skips the jitter of the edge itself
        esp_rom_delay_us(DHT_TIMER_INTERVAL);
        elapsed = esp_timer_get_time() - start;

        if (gpio_get_level(pin) == expected_pin_state)
        {
            if (duration)
                *duration = (uint32_t)elapsed;
            return ESP_OK;
        }

        if ((uint32_t)elapsed >= timeout)
            return ESP_ERR_TIMEOUT;
    }
}

/**
 * Request data from DHT and read raw bit stream.
 * The function call should be protected from task switching.
 * On a timeout, 'failed_at' points to the name of the place where the line
 * did not change in time; the caller reports it after leaving the critical
 * section, because logging is not allowed there.
 */
static inline esp_err_t dht_fetch_data(dht_sensor_type_t sensor_type, gpio_num_t pin, uint8_t data[DHT_DATA_BYTES],
                                       const char **failed_at)
{
    esp_err_t err;
    uint32_t low_duration;
    uint32_t high_duration;

    // Phase 'A' pulling signal low to initiate read sequence
    gpio_set_level(pin, 0);
    esp_rom_delay_us(sensor_type == DHT_TYPE_SI7021 ? 500 : 20000);
    gpio_set_level(pin, 1);

    // the sensor drives the line from now on, so the direction is switched
    // once instead of being switched on every phase and every bit
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    // Step through Phase 'B', 40us
    if ((err = dht_await_pin_state(pin, 40, 0, NULL)) != ESP_OK)
    {
        *failed_at = "phase 'B'";
        return err;
    }
    // Step through Phase 'C', 88us
    if ((err = dht_await_pin_state(pin, 88, 1, NULL)) != ESP_OK)
    {
        *failed_at = "phase 'C'";
        return err;
    }
    // Step through Phase 'D', 88us
    if ((err = dht_await_pin_state(pin, 88, 0, NULL)) != ESP_OK)
    {
        *failed_at = "phase 'D'";
        return err;
    }

    // Read in each of the 40 bits of data...
    for (int i = 0; i < DHT_DATA_BITS; i++)
    {
        if ((err = dht_await_pin_state(pin, 65, 1, &low_duration)) != ESP_OK)
        {
            *failed_at = "the low part of a bit";
            return err;
        }
        if ((err = dht_await_pin_state(pin, 90, 0, &high_duration)) != ESP_OK)
        {
            *failed_at = "the high part of a bit";
            return err;
        }

        uint8_t b = i / 8;
        uint8_t m = i % 8;
        if (!m)
            data[b] = 0;

        data[b] |= (high_duration > low_duration) << (7 - m);
    }

    return ESP_OK;
}

/**
 * Pack two data bytes into single value and take into account sign bit.
 */
static inline int16_t dht_convert_data(dht_sensor_type_t sensor_type, uint8_t msb, uint8_t lsb)
{
    int16_t data;

    if (sensor_type == DHT_TYPE_DHT11)
    {
        data = msb * 10 + lsb;
    }
    else
    {
        data = msb & 0x7F;
        data <<= 8;
        data |= lsb;
        if (msb & BIT(7))
            data = -data;       // convert it to negative
    }

    return data;
}

esp_err_t dht_read_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
                        int16_t *humidity, int16_t *temperature)
{
    CHECK_ARG(humidity || temperature);
    CHECK_ARG(GPIO_IS_VALID_OUTPUT_GPIO(pin));
    CHECK_ARG(dht_type_valid(sensor_type));

    uint8_t data[DHT_DATA_BYTES] = { 0 };

    CHECK(gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD));
    CHECK(gpio_set_level(pin, 1));

    const char *failed_at = NULL;

    PORT_ENTER_CRITICAL();
    esp_err_t result = dht_fetch_data(sensor_type, pin, data, &failed_at);
    PORT_EXIT_CRITICAL();

    // logging takes a lock of the C library, so it must not happen while
    // the interrupts are disabled
    if (result != ESP_OK)
        ESP_LOGE(TAG, "Timeout at %s", failed_at);

    /* restore GPIO direction because, after calling dht_fetch_data(), the
     * GPIO direction mode changes */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 1);

    if (result != ESP_OK)
        return result;

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        ESP_LOGE(TAG, "Checksum failed, invalid data received from sensor");
        return ESP_ERR_INVALID_CRC;
    }

    if (humidity)
        *humidity = dht_convert_data(sensor_type, data[0], data[1]);
    if (temperature)
        *temperature = dht_convert_data(sensor_type, data[2], data[3]);

    return ESP_OK;
}

esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
                              float *humidity, float *temperature)
{
    CHECK_ARG(humidity || temperature);

    int16_t i_humidity, i_temp;

    esp_err_t res = dht_read_data(sensor_type, pin, humidity ? &i_humidity : NULL, temperature ? &i_temp : NULL);
    if (res != ESP_OK)
        return res;

    if (humidity)
        *humidity = i_humidity / 10.0;
    if (temperature)
        *temperature = i_temp / 10.0;

    return ESP_OK;
}
