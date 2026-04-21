/***********************************************************************************************************************
 * File Name    : complementary_pwm.h
 * Description  : Contains Macros and function declarations for Complementary PWM modes.
 *                Implements support for GPT Complementary PWM Modes 1, 2, 3, 4
 **********************************************************************************************************************/

#ifndef COMPLEMENTARY_PWM_H_
#define COMPLEMENTARY_PWM_H_

#include "common_utils.h"

/***********************************************************************************************************************
 * Complementary PWM Mode Definitions
 **********************************************************************************************************************/

/* Menu option indices for complementary PWM modes */
#define COMP_PWM_MODE1_TIMER            (1U)    /* Menu option 4: Complementary PWM Mode 1 */
#define COMP_PWM_MODE2_TIMER            (2U)    /* Menu option 5: Complementary PWM Mode 2 */
#define COMP_PWM_MODE3_TIMER            (3U)    /* Menu option 6: Complementary PWM Mode 3 */
#define COMP_PWM_MODE4_TIMER            (4U)    /* Menu option 7: Complementary PWM Mode 4 */

/* Dead time constraints for FPB-RA2T1 (16-bit GPT timer) */
#define COMP_PWM_MAX_DEAD_TIME          (0x0FFFU)   /* Maximum dead time value for 16-bit timer */
#define COMP_PWM_MIN_DEAD_TIME          (0x0001U)   /* Minimum dead time value (must be > 0) */
#define GPT0_MASTER_GTDVU_ADDR          (0x4008908C)

/* Duty cycle range for complementary PWM (0% to 100%) */
#define COMP_PWM_MAX_DUTY               (100U)
#define COMP_PWM_MIN_DUTY               (0U)

/* Default dead time in timer counts (adjust per application requirement) */
#define COMP_PWM_DEFAULT_DEAD_TIME      (0x0100U)

/* Default PWM period for complementary mode (triangle wave cycle) */
#define COMP_PWM_DEFAULT_PERIOD         (0xA000U)

/***********************************************************************************************************************
 * Function declarations
 **********************************************************************************************************************/

/**
 * @brief  Initialize GPT channels for complementary PWM operation
 * @param[in] comp_mode  Complementary PWM mode (COMP_PWM_MODE1_TIMER to COMP_PWM_MODE4_TIMER)
 * @retval FSP_SUCCESS on successful initialization
 */
fsp_err_t comp_pwm_init(uint8_t comp_mode);

/**
 * @brief  Start complementary PWM output on all 3 channels
 * @retval FSP_SUCCESS on successful start
 */
fsp_err_t comp_pwm_start(void);

/**
 * @brief  Stop complementary PWM output and close all channels
 * @retval None
 */
void comp_pwm_stop(void);

/**
 * @brief  Set 3-phase duty cycles for U, V, W phases
 * @param[in] duty_u  U-phase duty cycle percentage (0-100)
 * @param[in] duty_v  V-phase duty cycle percentage (0-100)
 * @param[in] duty_w  W-phase duty cycle percentage (0-100)
 * @retval FSP_SUCCESS on successful duty cycle update
 */
fsp_err_t comp_pwm_set_duty_3phase(uint8_t duty_u, uint8_t duty_v, uint8_t duty_w);

/**
 * @brief  Print the complementary PWM submenu
 */
void print_comp_pwm_menu(void);
void print_comp_pwm_mode_menu(void);

/**
 * @brief  Process user input for complementary PWM mode selection and parameters
 */
void comp_pwm_process_input(void);

#endif /* COMPLEMENTARY_PWM_H_ */
