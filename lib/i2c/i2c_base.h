#ifndef _I2C_BASE_H_
#define _I2C_BASE_H_

#include <inttypes.h>
#include <jescore.h>
#include "syserr.h"
#include <driver/i2c.h>

#define I2C_BASE_SCL    21
#define I2C_BASE_SDA    20
#define I2C_BASE_NUM    I2C_NUM_0
#define I2C_BASE_SPEED  100000
#define I2C_BASE_BUS_TXRX_TIMEOUT pdMS_TO_TICKS(1000)
#define I2C_BASE_BUS_LOCK_TIMEOUT pdMS_TO_TICKS(1000)

e_syserr_t i2c_base_init(uint8_t scl, uint8_t sda, uint32_t speed);

e_syserr_t i2c_base_init_default(void);

e_syserr_t i2c_base_transmit(uint8_t addr, uint8_t *tx_buf, uint32_t len, TickType_t timeout);

e_syserr_t i2c_base_receive(uint8_t addr, uint8_t *rx_buf, uint32_t len, TickType_t timeout);

#endif // _I2C_BASE_H_