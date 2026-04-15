/* generated configuration header file - do not edit */
#ifndef BSP_PIN_CFG_H_
#define BSP_PIN_CFG_H_
#include "r_ioport.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#define ARDUINO_A0 (BSP_IO_PORT_00_PIN_00)
#define ARDUINO_A1 (BSP_IO_PORT_00_PIN_01)
#define ARDUINO_A2 (BSP_IO_PORT_00_PIN_02)
#define ARDUINO_D4 (BSP_IO_PORT_00_PIN_12)
#define ARDUINO_A3 (BSP_IO_PORT_00_PIN_13)
#define ARDUINO_A4 (BSP_IO_PORT_00_PIN_14)
#define PMOD1_INT_ARDUINO_A5 (BSP_IO_PORT_00_PIN_15)
#define PMOD1_MISO_RXD_ARDUINO_MISO (BSP_IO_PORT_01_PIN_00)
#define PMOD1_MOSI_TXD_ARDUINO_MOSI (BSP_IO_PORT_01_PIN_01)
#define PMOD1_SCK_ARDUINO_SCK (BSP_IO_PORT_01_PIN_02)
#define PMOD1_CS_CTS_ARDUINO_SS (BSP_IO_PORT_01_PIN_03)
#define ARDUINO_D8 (BSP_IO_PORT_01_PIN_04)
#define VCOM_TXD (BSP_IO_PORT_01_PIN_09)
#define VCOM_RXD (BSP_IO_PORT_01_PIN_10)
#define ARDUINO_D3 (BSP_IO_PORT_01_PIN_11)
#define ARDUINO_D9 (BSP_IO_PORT_01_PIN_12)
#define SW (BSP_IO_PORT_02_PIN_00)
#define PMOD2_GPIO9_ARDUINO_D7 (BSP_IO_PORT_02_PIN_06)
#define PMOD2_GPIO8 (BSP_IO_PORT_02_PIN_07)
#define PMOD2_GPIO7 (BSP_IO_PORT_02_PIN_08)
#define PMOD2_RESET (BSP_IO_PORT_02_PIN_12)
#define LED1 (BSP_IO_PORT_02_PIN_13) /* GREEN */
#define ARDUINO_RX (BSP_IO_PORT_03_PIN_01) /* PMOD2_GPIO10 */
#define ARDUINO_TX (BSP_IO_PORT_03_PIN_02)
#define PMOD2_SCL_ARDUINO_SCL (BSP_IO_PORT_04_PIN_00) /* PMOD1_SCL */
#define PMOD2_SDA_ARDUINO_SDA (BSP_IO_PORT_04_PIN_01) /* PMOD1_SDA */
#define PMOD2_INT (BSP_IO_PORT_04_PIN_08)
#define ARDUINO_D2 (BSP_IO_PORT_04_PIN_09)
#define ARDUINO_D6 (BSP_IO_PORT_05_PIN_00)
#define PMOD1_GPIO10_ARDUINO_D5 (BSP_IO_PORT_09_PIN_13)
#define LED2 (BSP_IO_PORT_09_PIN_14) /* GREEN_PMOD1_GPIO9 */
#define PMOD1_RESET (BSP_IO_PORT_09_PIN_15)

extern const ioport_cfg_t g_bsp_pin_cfg; /* RA2T1 FPB */

void BSP_PinConfigSecurityInit();

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER
#endif /* BSP_PIN_CFG_H_ */
