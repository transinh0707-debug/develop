/*
 *  File Name: common.utils.h
 *  Created on: Mar 27, 2026
 *      Author: SinhT
 */

#ifndef COMMON_UTILS_H_
#define COMMON_UTILS_H_

/* Generic headers */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hal_data.h"

/* SEGGER RTT and error related headers*/
#include "SEGGER_RTT/SEGGER_RTT.h"

#define BIT_SHIFT_8             (8U)
#define SIZE_64                 (64U)

#define LVL_ERR                 (1U)        /* Error conditions */

#define VERSION                 ("6.5.0 rc0")
#define MODULE_NAME             "r_gpt"
#define BANNER_1                "\r\n******************************************************************"
#define BANNER_2                "\r\n* FSP Project for "MODULE_NAME" module                           *"
#define BANNER_3                "\r\n* FSP Project Version %s                                         *"
#define BANNER_4                "\r\n* Flexible Software Package Version %d.%d.%d  *"
#define BANNER_5                "\r\n******************************************************************"
#define BANNER_6                "\r\n Refer to readme.txt file for more details on FSP project and "\
                                "\r\n FSP User's manual for more information about "MODULE_NAME" driver \r\n"

#define SEGGER_INDEX            (0U)

#define APP_PRINT(fn_, ...)     (SEGGER_RTT_printf (SEGGER_INDEX, (fn_), ##__VA_ARGS__))

#define APP_ERR_PRINT(fn_, ...) ({\
                                if (LVL_ERR)\
                                SEGGER_RTT_printf (SEGGER_INDEX, "[ERR] in Function: %s(), %s",\
                                __FUNCTION__, (fn_), ##__VA_ARGS__);\
                                })

#define APP_ERR_TRAP(err)       ({\
                             if ((err)) {\
                             SEGGER_RTT_printf(SEGGER_INDEX, "\r\n Returned Error Code: 0x%x \r\n", (err));\
                             __asm("BKPT #0\n");} /* trap upon the error */\
                             })

#define APP_READ(read_data)     (SEGGER_RTT_Read(SEGGER_INDEX, read_data, sizeof(read_data)))

#define APP_CHECK_DATA          (SEGGER_RTT_HasKey())

#endif /* COMMON_UTILS_H_ */
