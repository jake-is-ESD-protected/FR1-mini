#ifndef _UIO_H_
#define _UIO_H_

#include "syserr.h"
#include <inttypes.h>

#define SSD1306_LCDWIDTH 64
#define SSD1306_LCDHEIGHT 48
#define OLED_RESET -1
#define OLED_I2C_ADDRESS 0x3D

#define OLED_VISUAL_DB_MAX  105
#define OLED_VISUAL_DB_MIN  15
#define OLED_VISUAL_DEG_MAX 159
#define OLED_VISUAL_DEG_MIN 21
#define OLED_VISUAL_VUM_RAD 21
#define OLED_VISUAL_VUM_X   32
#define OLED_VISUAL_VUM_Y   46   
#define OLED_VISUAL_TXT_UPD 6

#define UIO_OLED_WGT_BATT_X 56
#define UIO_OLED_WGT_BATT_Y 2
#define UIO_OLED_WGT_BATT_W 6
#define UIO_OLED_WGT_BATT_H 4

#define UIO_LED_PIN 5

#define UIO_JOB_NAME "uio"

e_syserr_t uio_init(void);

void uio_oled_init(void);

void uio_led_on(void);

void uio_led_off(void);

void uio_led_toggle(void);

void uio_led_level(uint8_t lvl);

void uio_oled_rotate_show(void);

void uio_oled_title_screen(void);

void uio_oled_idle_screen(void);

void uio_oled_update_db(int16_t val);

void uio_oled_update_db_text(int16_t val);

void uio_oled_update_db_vu(int16_t val);

uint8_t uio_oled_db_to_deg(int16_t db);

void uio_job(void* p);

#endif // _UIO_H_