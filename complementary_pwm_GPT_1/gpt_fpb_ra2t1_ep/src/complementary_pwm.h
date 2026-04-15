/***********************************************************************************************************************
 * File Name    : complementary_pwm.h
 * Description  : Contains Macros and function declarations for Complementary PWM modes.
 *                Implements support for GPT Complementary PWM Modes 1, 2, 3, 4 per RA2T1
 *                User's Manual sections 20.3.3.7 and 20.3.3.8.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/

#ifndef COMPLEMENTARY_PWM_H_
#define COMPLEMENTARY_PWM_H_

#include "common_utils.h"

/***********************************************************************************************************************
 * Complementary PWM Mode Definitions
 * Per RA2T1 Manual Table 20.35 / Table 20.40
 * GTCR.MD[3:0] bit encoding:
 *   0xC = Complementary PWM Mode 1 (transfer at crest)
 *   0xD = Complementary PWM Mode 2 (transfer at trough)
 *   0xE = Complementary PWM Mode 3 (transfer at crest and trough)
 *   0xF = Complementary PWM Mode 4 (immediate transfer)
 **********************************************************************************************************************/

/* Menu option indices for complementary PWM modes */
#define COMP_PWM_MODE1_TIMER            (1U)    /* Menu option 4: Complementary PWM Mode 1 */
#define COMP_PWM_MODE2_TIMER            (2U)    /* Menu option 5: Complementary PWM Mode 2 */
#define COMP_PWM_MODE3_TIMER            (3U)    /* Menu option 6: Complementary PWM Mode 3 */
#define COMP_PWM_MODE4_TIMER            (4U)    /* Menu option 7: Complementary PWM Mode 4 */

/* Timer open state values for complementary modes */
#define COMP_PWM_MODE1                  (4U)
#define COMP_PWM_MODE2                  (5U)
#define COMP_PWM_MODE3                  (6U)
#define COMP_PWM_MODE4                  (7U)

/* Dead time constraints for FPB-RA2T1 (16-bit GPT timer) */
#define COMP_PWM_MAX_DEAD_TIME          (0x0FFFU)   /* Maximum dead time value for 16-bit timer */
#define COMP_PWM_MIN_DEAD_TIME          (0x0001U)   /* Minimum dead time value (must be > 0) */
#define GPT0_MASTER_GTDVU_ADDR          (0x4008908C)
/* Duty cycle range for complementary PWM (0% to 100%) */
#define COMP_PWM_MAX_DUTY               (100U)
#define COMP_PWM_MIN_DUTY               (0U)

/* 3-Phase output channel indices */
#define COMP_PWM_PHASE_U                (0U)    /* U-phase: Master channel (ch0) */
#define COMP_PWM_PHASE_V                (1U)    /* V-phase: Slave channel 1 (ch1) */
#define COMP_PWM_PHASE_W                (2U)    /* W-phase: Slave channel 2 (ch2) */
#define COMP_PWM_NUM_PHASES             (3U)    /* Total number of phases */

/* Default dead time in timer counts (adjust per application requirement) */
#define COMP_PWM_DEFAULT_DEAD_TIME      (0x0100U)

/* Default PWM period for complementary mode (triangle wave cycle) */
#define COMP_PWM_DEFAULT_PERIOD         (0x2000U)

/***********************************************************************************************************************
 * Complementary PWM configuration structure
 * Holds runtime parameters for all 4 complementary PWM modes
 **********************************************************************************************************************/
typedef struct st_comp_pwm_cfg
{
    timer_mode_t  mode;                  /* Active complementary PWM mode (1-4) */
    uint32_t dead_time;             /* Dead time value in timer counts (GTDVU) */
    uint32_t period;                /* PWM cycle period (GTPR) */
    uint32_t duty_u;                /* U-phase duty (compare match value GTCCRA ch0) */
    uint32_t duty_v;                /* V-phase duty (compare match value GTCCRA ch1) */
    uint32_t duty_w;                /* W-phase duty (compare match value GTCCRA ch2) */
    bool     double_buffer;         /* Enable double buffer (Mode 3 only: GTBER2.CP3DB) */
    bool     is_running;            /* Flag: complementary PWM is currently running */
} comp_pwm_cfg_t;

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
 * @brief  Set dead time value for complementary PWM
 * @param[in] dead_time_counts  Dead time value in timer counts
 * @retval FSP_SUCCESS on successful dead time configuration
 */
fsp_err_t comp_pwm_set_dead_time(uint32_t dead_time_counts);

/**
 * @brief  Set 3-phase duty cycles for U, V, W phases
 * @param[in] duty_u  U-phase duty cycle percentage (0-100)
 * @param[in] duty_v  V-phase duty cycle percentage (0-100)
 * @param[in] duty_w  W-phase duty cycle percentage (0-100)
 * @retval FSP_SUCCESS on successful duty cycle update
 */
fsp_err_t comp_pwm_set_duty_3phase(uint8_t duty_u, uint8_t duty_v, uint8_t duty_w);

/**
 * @brief  Get current complementary PWM configuration
 * @return Pointer to the current configuration structure
 */
const comp_pwm_cfg_t * comp_pwm_get_config(void);

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
