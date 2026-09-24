#pragma once

#include "esp_err.h"
#include <stdint.h>

// Largest block a single Read Input Registers request may ask for.
#define MODBUS_MAX_READ_REGISTERS 125

void modbus_uart_init(void);

// Reads count consecutive input registers (function code 0x04) starting at
// start_addr into out. Returns ESP_OK only for a well-formed, CRC-checked reply.
esp_err_t modbus_read_input_registers(uint16_t start_addr, uint16_t count, uint16_t *out);

// Bring-up helpers for verifying the physical RS-485 link independent of
// Modbus framing. Both start a task that never returns.

// Holds the driver enabled and repeatedly transmits a single byte.
void modbus_start_tx_test(void);

// Holds the receiver enabled and logs every byte that arrives, one at a time.
void modbus_start_rx_test(void);

// Transmits a byte and immediately listens for it. With TXD jumpered straight
// to RXD this isolates the ESP32's own UART RX path from the transceiver and
// the bus; left wired normally it is an echo test of the whole loop.
void modbus_start_loopback_test(void);
