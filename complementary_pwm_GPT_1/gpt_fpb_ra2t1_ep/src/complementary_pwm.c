/***********************************************************************************************************************
 * File Name    : complementary_pwm.c
 * Description  : Implementation of GPT Complementary PWM Modes 1, 2, 3, 4 for FPB-RA2T1.
 *                Based on RA2T1 User's Manual sections 20.3.3.7 and 20.3.3.8.
 *
 *                Complementary PWM uses 3 consecutive GPT channels (ch0=master, ch1=slave1, ch2=slave2)
 *                to generate three-phase PWM with dead time for motor control applications.
 *
 *                Mode 1: Buffer transfer at crest   (GTCR.MD = 0xC)
 *                Mode 2: Buffer transfer at trough  (GTCR.MD = 0xD)
 *                Mode 3: Buffer transfer at crest and trough (GTCR.MD = 0xE)
 *                Mode 4: Immediate transfer         (GTCR.MD = 0xF)
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/

#include "common_utils.h"
#include "gpt_timer.h"
#include "complementary_pwm.h"

/*******************************************************************************************************************//**
 * @addtogroup r_gpt_ep
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/* Complementary PWM runtime configuration */
static comp_pwm_cfg_t g_comp_pwm_cfg =
{
    .mode          = (timer_mode_t)RESET_VALUE,
    .dead_time     = COMP_PWM_DEFAULT_DEAD_TIME,
    .period        = COMP_PWM_DEFAULT_PERIOD,
    .duty_u        = 0U,
    .duty_v        = 0U,
    .duty_w        = 0U,
    .double_buffer = false,
    .is_running    = false,
};

/* Store open state (extern from gpt_timer.c) */
static timer_mode_t g_timer_mode;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_open_channels(uint8_t comp_mode);
static fsp_err_t comp_pwm_configure_dead_time(uint32_t dead_time_counts);
static fsp_err_t comp_pwm_set_compare_match(uint32_t duty_u, uint32_t duty_v, uint32_t duty_w);
static uint32_t  comp_pwm_duty_to_counts(uint32_t duty_percent, uint32_t period);
static void      comp_pwm_close_all_channels(void);

/***********************************************************************************************************************
 * Public function implementations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * @brief  Initialize GPT channels for complementary PWM operation
 *
 * Initialization sequence per RA2T1 Manual Table 20.35 (Modes 1-3) / Table 20.40 (Mode 4):
 *   Step 1:  Set operating mode (GTCR.MD) on master channel
 *   Step 2:  Select count clock (GTCR.TPCS) on master channel
 *   Step 3:  Set cycle (GTPR, GTPBR, GTPDBR) on master channel
 *   Step 4:  Set GTIOCnm pin function (GTIOR) on all 3 channels
 *   Step 5:  Enable GTCPPOn pin output (GTIOR.PSYE) on master channel
 *   Step 6:  Enable pin output (GTIOR.OAE/OBE) on all 3 channels
 *   Step 7:  Set buffer operation (GTBER2.CP3DB) — Mode 3 only
 *   Step 8:  Set compare match value (GTCCRA) on all 3 channels
 *   Step 9:  Set buffer value (GTCCRD/GTCCRF) on all 3 channels
 *   Step 10: Set dead time value (GTDVU) on master channel
 *
 * @param[in] comp_mode  Menu selection: COMP_PWM_MODE1_TIMER to COMP_PWM_MODE4_TIMER
 * @retval FSP_SUCCESS          Initialization successful
 * @retval FSP_ERR_ALREADY_OPEN Timer already open
 * @retval other                Error from FSP API
 **********************************************************************************************************************/
fsp_err_t comp_pwm_init(uint8_t comp_mode)
{
    fsp_err_t err = FSP_SUCCESS;

    g_comp_pwm_cfg.mode = g_timer_comp_master_cfg.mode;
    g_comp_pwm_cfg.period = g_timer_comp_master_cfg.period_counts;
    g_comp_pwm_cfg.duty_u = g_timer_comp_master_cfg.duty_cycle_counts;
    g_comp_pwm_cfg.duty_v = g_timer_comp_slave1_cfg.duty_cycle_counts;
    g_comp_pwm_cfg.duty_w = g_timer_comp_slave2_cfg.duty_cycle_counts;


    gpt_extended_cfg_t * p_extend = (gpt_extended_cfg_t *) g_timer_comp_master_cfg.p_extend;
    g_comp_pwm_cfg.dead_time = p_extend->p_pwm_cfg->dead_time_count_up;

    /* If complementary PWM is already running, stop it first */
    if (true == g_comp_pwm_cfg.is_running)
    {
        comp_pwm_stop();
    }

    /* Map menu selection to internal mode identifier */
    switch (comp_mode)
    {
        case COMP_PWM_MODE1_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE1;
            APP_PRINT("\r\n--- Complementary PWM Mode 1 (Transfer at Crest) ---\r\n");
            break;
        case COMP_PWM_MODE2_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE2;
            APP_PRINT("\r\n--- Complementary PWM Mode 2 (Transfer at Trough) ---\r\n");
            break;
        case COMP_PWM_MODE3_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE3;
            APP_PRINT("\r\n--- Complementary PWM Mode 3 (Transfer at Crest & Trough) ---\r\n");
            break;
        case COMP_PWM_MODE4_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE4;
            APP_PRINT("\r\n--- Complementary PWM Mode 4 (Immediate Transfer) ---\r\n");
            break;
        default:
            APP_ERR_PRINT("\r\n** Invalid Complementary PWM mode **\r\n");
            return FSP_ERR_INVALID_ARGUMENT;
    }

    /*
     * Steps 1-6: Open and configure all 3 GPT channels
     * The FSP R_GPT_Open API handles:
     *   - Setting GTCR.MD (operating mode)
     *   - Setting GTCR.TPCS (clock prescaler)
     *   - Setting GTPR (period/cycle)
     *   - Configuring GTIOR (pin functions, output enable)
     *
     * Note: In complementary PWM mode, only the master channel (ch0) GTCR.MD
     * setting controls all 3 channels. Slave channels follow the master.
     */
    err = comp_pwm_open_channels(comp_mode);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM channel open FAILED **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }
    else
    {
        APP_ERR_PRINT("\r\n** Complementary PWM channel open SUCCESS **\r\n");   
    }

    /*
     * Step 10: Set dead time value (GTDVU register of master channel)
     * The dead time creates a non-overlapping guard between positive-phase
     * and negative-phase outputs to prevent shoot-through in H-bridge drivers.
     *
     * Per manual: "Do not perform buffer operation for the GTDVU register
     * in complementary PWM mode."
     */
    err = comp_pwm_configure_dead_time(g_comp_pwm_cfg.dead_time);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Dead time configuration FAILED **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }

    /*
     * Steps 8-9: Set initial compare match values (GTCCRA) and buffer values (GTCCRD)
     * Initial duty is 50% on all three phases (centered PWM)
     */
    err = comp_pwm_set_compare_match(g_comp_pwm_cfg.duty_u, g_comp_pwm_cfg.duty_v, g_comp_pwm_cfg.duty_w);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Compare match set FAILED **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }
    else
    {
         APP_PRINT("Duty updated:\nvalue_u=%d\nvalue_v=%d\nvalue_w=%d\r\n", g_comp_pwm_cfg.duty_u , g_comp_pwm_cfg.duty_v, g_comp_pwm_cfg.duty_w);
    }

    APP_PRINT("Complementary PWM Mode %d initialized successfully\r\n", g_comp_pwm_cfg.mode - 11);
    APP_PRINT("  Period: 0x%04X counts\r\n", g_comp_pwm_cfg.period);
    APP_PRINT("  Dead Time: 0x%04X counts\r\n", g_comp_pwm_cfg.dead_time);

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Start complementary PWM output on all 3 channels
 *
 * Step 11 from manual: Set GTCR.CST of master channel to 1.
 * In complementary PWM mode, writing to the CSTRTn bit of the master channel
 * is the only valid way to start — slave channels follow automatically.
 *
 * @retval FSP_SUCCESS  Start successful
 * @retval other        Error from FSP API
 **********************************************************************************************************************/
fsp_err_t comp_pwm_start(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /*
     * Per RA2T1 Manual section 20.3.3.7:
     * "In complementary PWM mode, writing to the CSTRTn bit of master channel
     *  is only valid. The bit on slave channels reflects the master setting."
     *
     * Therefore we only start the master channel (ch0). The FSP R_GPT_Start
     * API sets GTCR.CST = 1 which triggers all 3 channels to begin counting.
     */
    err = R_GPT_Start(&g_timer_comp_master_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM START failed **\r\n");
        return err;
    }

    g_comp_pwm_cfg.is_running = true;
    APP_PRINT("Complementary PWM started (3-phase output active)\r\n");

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Stop complementary PWM and close all channels
 *
 * Per manual: "In complementary PWM mode, writing to the CSTOPn bit of master
 * channel is only valid." Stopping the master channel stops all slaves.
 **********************************************************************************************************************/
void comp_pwm_stop(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Stop master channel — slaves follow automatically */
    err = R_GPT_Stop(&g_timer_comp_master_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM STOP failed **\r\n");
    }

    /* Close all 3 channels */
    // comp_pwm_close_all_channels();

    g_comp_pwm_cfg.is_running = false;

    APP_PRINT("Complementary PWM stopped and all channels closed\r\n");
}

/***********************************************************************************************************************
 * @brief  Set dead time value
 *
 * Dead time is written to the GTDVU register of the master channel.
 * This value defines the non-overlapping guard time between positive-phase
 * and negative-phase outputs.
 *
 * Per manual: "Set the dead time value in GTDVU of GPT16n channel."
 * Also: "Do not perform buffer operation for the GTDVU register in
 * complementary PWM mode."
 *
 * @param[in] dead_time_counts  Dead time value in timer counts
 * @retval FSP_SUCCESS          Dead time set successfully
 * @retval FSP_ERR_INVALID_ARGUMENT  Dead time out of range
 **********************************************************************************************************************/
fsp_err_t comp_pwm_set_dead_time(uint32_t dead_time_counts)
{
    /*
    * In complementary PWM mode, set the GTDVU register to meet the following conditions:
    * ● GTDVU > 0
    * ● GTDVU < GTPR/2
    * ● GTDVU + GTPR ≤ UINT16_MAX (GPT16) or UINT32_MAX (GPT32)
     */

    /* Validate dead time range */
    if ((dead_time_counts < COMP_PWM_MIN_DEAD_TIME) || 
        (dead_time_counts >= (g_comp_pwm_cfg.period / 2)))
    {
        APP_ERR_PRINT(
            "\r\n** Invalid dead time (0x%08lX): must be > 0 and < period/2 = (0x%08lX) **\r\n",
            dead_time_counts,
            (g_comp_pwm_cfg.period / 2)
        );
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if ((dead_time_counts + g_comp_pwm_cfg.period) > UINT16_MAX)
    {
        APP_ERR_PRINT(
            "\r\n** FOR RA2T1: Invalid dead time (0x%08lX): dead_time + period (0x%08lX) exceeds UINT16_MAX **\r\n",
            dead_time_counts,
            g_comp_pwm_cfg.period
        );
        return FSP_ERR_INVALID_ARGUMENT;
    }
    g_comp_pwm_cfg.dead_time = dead_time_counts;

    /* update the dead time register after start */
    if (false == g_comp_pwm_cfg.is_running)
    {
        fsp_err_t err = comp_pwm_configure_dead_time(dead_time_counts);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
        APP_PRINT("Dead time updated to 0x%04X counts\r\n", dead_time_counts);
    }

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Set 3-phase duty cycles
 *
 * Converts duty cycle percentages to compare match counts and updates the
 * GTCCRA registers of all 3 channels.
 *
 * For Modes 1-3: Updates are written to GTCCRD buffer registers and transferred
 *                at crest/trough per the mode's buffer transfer timing.
 * For Mode 4:    Updates are written to GTCCRD and immediately transferred to
 *                GTCCRA via the bypass path.
 *
 * Per manual Step 12: "Finally, make settings for the GPT16n+2.GTCCRD register
 * (data is transferred to the temporary register)."
 *
 * @param[in] duty_u  U-phase duty cycle (0-100%)
 * @param[in] duty_v  V-phase duty cycle (0-100%)
 * @param[in] duty_w  W-phase duty cycle (0-100%)
 * @retval FSP_SUCCESS  Duty cycle update successful
 **********************************************************************************************************************/
fsp_err_t comp_pwm_set_duty_3phase(uint8_t duty_u, uint8_t duty_v, uint8_t duty_w)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Validate inputs */
    if ((duty_u > COMP_PWM_MAX_DUTY) || (duty_v > COMP_PWM_MAX_DUTY) || (duty_w > COMP_PWM_MAX_DUTY))
    {
        APP_ERR_PRINT("\r\n** Duty cycle out of range (max 100%%) **\r\n");
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* Store new duty values */
    g_comp_pwm_cfg.duty_u = duty_u;
    g_comp_pwm_cfg.duty_v = duty_v;
    g_comp_pwm_cfg.duty_w = duty_w;

    /* Convert percentages to compare match counts */
    uint32_t counts_u = comp_pwm_duty_to_counts(duty_u, g_comp_pwm_cfg.period);
    uint32_t counts_v = comp_pwm_duty_to_counts(duty_v, g_comp_pwm_cfg.period);
    uint32_t counts_w = comp_pwm_duty_to_counts(duty_w, g_comp_pwm_cfg.period);

    /*
     * Update compare match values.
     *
     * For Modes 1-3: Write to GTCCRD buffer registers. The data flows through
     *   the buffer chain: GTCCRD → TempA → GTCCRC → GTCCRA
     *   Transfer timing depends on the mode (crest / trough / both).
     *
     * For Mode 4: Write to GTCCRD with immediate transfer path:
     *   GTCCRD → GTCCRA (immediate) + GTCCRD → TempA → GTCCRC (normal chain)
     *   Per manual: "the value written to the GTCCRD and GTCCRF registers is
     *   immediately applied to compare match operation."
     *
     * Critical: Per manual Step 12, slave channel 2 (GPT16n+2) GTCCRD must
     * be written LAST because writing to it triggers the simultaneous transfer
     * to temporary registers across all 3 channels.
     */
    err = comp_pwm_set_compare_match(counts_u, counts_v, counts_w);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Duty cycle update FAILED **\r\n");
        return err;
    }

    APP_PRINT("Duty updated:\nU=%d%%\tvalue_u=%d \nV=%d%%\tvalue_v=%d \nW=%d%%\tvalue_w=%d\r\n", duty_u, counts_u, duty_v, counts_v, duty_w, counts_w);

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Get current complementary PWM configuration
 * @return Pointer to the configuration structure (read-only)
 **********************************************************************************************************************/
const comp_pwm_cfg_t * comp_pwm_get_config(void)
{
    return &g_comp_pwm_cfg;
}

/***********************************************************************************************************************
 * @brief  Print the complementary PWM submenu to RTT Viewer
 **********************************************************************************************************************/
void print_comp_pwm_menu(void)
{
    APP_PRINT("\r\n=== Complementary PWM Sub-Menu ===\r\n");
    APP_PRINT("Enter 1 to set init\r\n");
    APP_PRINT("Enter 2 to set Dead Time\r\n");
    APP_PRINT("Enter 3 to set 3-Phase Duty (U, V, W)\r\n");
    APP_PRINT("Enter 4 to Start Complementary PWM\r\n");
    APP_PRINT("Enter 5 to Stop  Complementary PWM\r\n");
    APP_PRINT("Enter 6 to Show  Current Status\r\n");
    APP_PRINT("Enter 0 to Return to Main Menu\r\n");
    APP_PRINT("Comp PWM Input:  ");
}

void print_comp_pwm_mode_menu(void)
{
    APP_PRINT("\r\n=== Enter 1 to Set Complementary PWM MODE 1 ===\r\n");
    APP_PRINT("\r\n=== Enter 2 to Set Complementary PWM MODE 2 ===\r\n");
    APP_PRINT("\r\n=== Enter 3 to Set Complementary PWM MODE 3 ===\r\n");
    APP_PRINT("\r\n=== Enter 4 to Set Complementary PWM MODE 4 ===\r\n");
}
/***********************************************************************************************************************
 * @brief  Process user input for complementary PWM submenu
 *
 * This function implements the interactive submenu for configuring and
 * controlling complementary PWM operation via J-Link RTT Viewer.
 **********************************************************************************************************************/
void comp_pwm_process_input(void)
{
    uint32_t sub_input = RESET_VALUE;

    print_comp_pwm_menu();

    while (true)
    {
        if (APP_CHECK_DATA)
        {
            unsigned char buf[BUF_SIZE] = {INITIAL_VALUE};
            APP_READ(buf);
            sub_input = (uint32_t)(atoi((char *)buf));

            switch (sub_input)
            {
                case 1U:  /* Set init */
                {
                    print_comp_pwm_mode_menu();
                    APP_PRINT("\r\nChoose mode for PWM(1 - 3):\r\n");
                    uint32_t dt = process_input_data();

                    fsp_err_t err = comp_pwm_init((uint8_t)dt);
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("Init failed\r\n");
                    }
                    break;
                }

                case 2U:  /* Set dead time */
                {
                    APP_PRINT("\r\nEnter dead time in hex counts:");
                    uint32_t dt = process_input_data();
                    fsp_err_t err = comp_pwm_set_dead_time((uint32_t)dt);
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("Dead time set failed\r\n");
                    }
                    break;
                }

                case 3U:  /* Set 3-phase duty */
                {
                    APP_PRINT("\r\nEnter U-phase duty (0-100%%):\r\n");
                    uint32_t du = process_input_data();

                    APP_PRINT("Enter V-phase duty (0-100%%):\r\n");
                    uint32_t dv = process_input_data();

                    APP_PRINT("Enter W-phase duty (0-100%%):\r\n");
                    uint32_t dw = process_input_data();

                    fsp_err_t err = comp_pwm_set_duty_3phase((uint8_t)du, (uint8_t)dv, (uint8_t)dw);
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("Duty cycle set failed\r\n");
                    }
                    break;
                }

                case 4U:  /* Start */
                {
                    if (false == g_comp_pwm_cfg.is_running)
                    {
                        fsp_err_t err = comp_pwm_start();
                        if (FSP_SUCCESS != err)
                        {
                            APP_ERR_PRINT("Complementary PWM start failed\r\n");
                        }
                    }
                    else
                    {
                        APP_PRINT("Complementary PWM already running\r\n");
                    }
                    break;
                }

                case 5U:  /* Stop */
                {
                    if (true == g_comp_pwm_cfg.is_running)
                    {
                        comp_pwm_stop();
                    }
                    else
                    {
                        APP_PRINT("Complementary PWM not running\r\n");
                    }
                    break;
                }

                case 6U:  /* Show status */
                {
                    const comp_pwm_cfg_t *cfg = comp_pwm_get_config();
                    APP_PRINT("\r\n--- Complementary PWM Status ---\r\n");
                    APP_PRINT("  Mode: %d  Running: %s\r\n", cfg->mode - 3,
                              cfg->is_running ? "YES" : "NO");
                    APP_PRINT("  Period: 0x%04X  Dead Time: 0x%04X\r\n", cfg->period, cfg->dead_time);
                    APP_PRINT("  Duty: U=%d%%  V=%d%%  W=%d%%\r\n", cfg->duty_u, cfg->duty_v, cfg->duty_w);
                    APP_PRINT("  Double Buffer: %s\r\n", cfg->double_buffer ? "ON" : "OFF");
                    break;
                }

                case 0U:  /* Return to main menu */
                {
                    APP_PRINT("\r\nReturning to main menu...\r\n");
                    comp_pwm_close_all_channels();
                    break;
                }

                default:
                {
                    APP_PRINT("\r\nInvalid sub-menu option\r\n");
                    break;
                }
            }

            print_comp_pwm_menu();
        }
    }
}

/***********************************************************************************************************************
 * Private function implementations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * @brief  Open all 3 GPT channels for complementary PWM
 *
 * Opens master channel (ch0), slave channel 1 (ch1), and slave channel 2 (ch2)
 * using the FSP R_GPT_Open API. The timer_cfg structures should be configured
 * in the FSP Configurator with the appropriate complementary PWM settings.
 *
 * @param[in] comp_mode  Complementary PWM mode selection
 * @retval FSP_SUCCESS on success
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_open_channels(uint8_t comp_mode)
{
    fsp_err_t err = FSP_SUCCESS;

    /*
     * Note: In a production implementation, the FSP Configurator would generate
     * separate timer configuration structures for each complementary PWM mode:
     *   - g_timer_comp_pwm_master_cfg  (ch0 with GTCR.MD = 0xC/0xD/0xE/0xF)
     *   - g_timer_comp_pwm_slave1_cfg  (ch1 — follows master)
     *   - g_timer_comp_pwm_slave2_cfg  (ch2 — follows master)
     *
     * For this example, we reuse the existing periodic timer configuration
     * as a base. In your e2studio project, you would add new GPT timer
     * stacks in the FSP Configurator with:
     *   - Mode: "Complementary PWM Mode X"
     *   - Channel: 0 (master), 1 (slave1), 2 (slave2)
     *   - Period: as needed for your PWM frequency
     *   - Output pins: GTIOCnA (positive), GTIOCnB (negative) for each channel
     */

    /* Update global state */
    switch (comp_mode)
    {
        case COMP_PWM_MODE1_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE1;
            break;
        case COMP_PWM_MODE2_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE2;
            break;
        case COMP_PWM_MODE3_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE3;
            break;
        case COMP_PWM_MODE4_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE4;
            break;
        default:
            break;
    }
    timer_cfg_t g_timer_comp_master_cfg_test = g_timer_comp_master_cfg;
    g_timer_comp_master_cfg_test.mode        = g_timer_mode;


    /* Open slave channel 1 (ch1) */
    err = R_GPT_Open(&g_timer_comp_slave1_ctrl, &g_timer_comp_slave1_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Slave1 channel (ch1) R_GPT_Open FAILED **\r\n");
        return err;
    }
    APP_PRINT("\nSlave channel 1 opened\r\n");

    /* Open slave channel 2 (ch2) */
    err = R_GPT_Open(&g_timer_comp_slave2_ctrl, &g_timer_comp_slave2_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Slave2 channel (ch2) R_GPT_Open FAILED **\r\n");
        return err;
    }
    APP_PRINT("\nSlave channel 2 opened\r\n");

    /* Open master channel (ch0) */
    err = R_GPT_Open(&g_timer_comp_master_ctrl, &g_timer_comp_master_cfg_test);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Master channel (ch0) R_GPT_Open FAILED **\r\n");
        return err;
    }
    APP_PRINT("\nMaster channel 0 opened\r\n");

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Configure dead time register (GTDVU) on master channel
 *
 * Per RA2T1 Manual: The dead time value is set in the GTDVU register of the
 * master channel only. The dead time creates a non-overlapping section between
 * positive-phase OFF and negative-phase ON transitions.
 *
 * Counter sections defined by dead time:
 *   - Trough section: master GTCNT ≤ GTDVU
 *   - Crest section:  slave1 GTCNT > GTPR
 *   - Middle section: between trough and crest
 *
 * @param[in] dead_time_counts  Dead time in timer counts
 * @retval FSP_SUCCESS on success
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_configure_dead_time(uint32_t dead_time_counts)
{
    fsp_err_t err = FSP_SUCCESS;

    /*
 
     * Setting Deadtime direct register access on RA2T1 for master channel of Complementary PWM Mode:
     *   Base address GPT160 = 0x40089000 (Channel0)
     *   GTDVU offset = 0x8C
     *   R_GPT0->GTDVU = (uint32_t) dead_time_counts;
     *
     */

    if (g_comp_pwm_cfg.is_running ==false)
    {
        *((volatile uint32_t *)GPT0_MASTER_GTDVU_ADDR) = dead_time_counts;
        APP_PRINT("Dead time configured: 0x%08lX counts\r\n", dead_time_counts);
    }
    else
    {
        APP_PRINT("Please stop PWM before setting dead time\r\n");
    }

    return err;
}

/***********************************************************************************************************************
 * @brief  Set compare match values for all 3 phases
 *
 * In complementary PWM mode, the compare match value in GTCCRA determines
 * where the positive-phase and negative-phase output transitions occur.
 *
 * Per manual Table 20.34:
 *   - In middle sections: compare match between master GTCNT and GTCCRA
 *     controls positive-phase; slave1 GTCNT and GTCCRA controls negative-phase.
 *   - In crest/trough sections: slave2 GTCNT with GTCCRC/GTCCRE is used
 *     for linearity at 0% and 100% duty.
 *
 * Per manual Step 12: Slave channel 2 GTCCRD must be written LAST because
 * this triggers simultaneous transfer to temporary registers across all channels.
 *
 * @param[in] duty_u  U-phase compare match count
 * @param[in] duty_v  V-phase compare match count
 * @param[in] duty_w  W-phase compare match count
 * @retval FSP_SUCCESS on success
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_set_compare_match(uint32_t duty_u, uint32_t duty_v, uint32_t duty_w)
{
    fsp_err_t err = FSP_SUCCESS;

    /*
     * For runtime duty cycle updates (Step 12), write to the buffer registers
     * in this specific order:
     *
     * 1. Write master channel (ch0) GTCCRD ← duty_u
     * 2. Write slave1 channel (ch1) GTCCRD ← duty_v
     * 3. Write slave2 channel (ch2) GTCCRD ← duty_w  *** MUST BE LAST ***
     *
     * Writing to ch2's GTCCRD triggers simultaneous transfer of all 3 channels'
     * GTCCRD → temporary register A.
     *
     * For Mode 3 double buffer: also write GTCCRF registers before GTCCRD.
     * For Mode 4: the GTCCRD write also triggers immediate GTCCRD → GTCCRA transfer.
     */

    /* Set compare match on master channel (U-phase) */
    err = R_GPT_DutyCycleSet(&g_timer_comp_master_ctrl, duty_u, GPT_IO_PIN_GTIOCA);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** U-phase DutyCycleSet FAILED **\r\n");
        return err;
    }

    /* Set compare match on slave1 channel (V-phase) */
    err = R_GPT_DutyCycleSet(&g_timer_comp_slave1_ctrl, duty_v, GPT_IO_PIN_GTIOCA);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** V-phase DutyCycleSet FAILED **\r\n");
        return err;
    }

    /* Set compare match on slave2 channel (W-phase) — MUST BE LAST */
    err = R_GPT_DutyCycleSet(&g_timer_comp_slave2_ctrl, duty_w, GPT_IO_PIN_GTIOCA);
    if (FSP_SUCCESS != err)
    {

        APP_ERR_PRINT("\r\n** W-phase DutyCycleSet FAILED **\r\n");
        return err;
    }

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Convert duty cycle percentage to compare match timer counts
 *
 * In complementary PWM mode:
 *   - When compare match value ≥ GTPR: duty = 0% (positive-phase OFF)
 *   - When compare match value = 0:    duty = 100% (positive-phase ON)
 *
 * Therefore: counts = GTPR - (duty_percent × GTPR / 100)
 * At 0%:   counts = GTPR (positive OFF, negative ON)
 * At 100%: counts = 0    (positive ON, negative OFF)
 *
 * @param[in] duty_percent  Duty cycle percentage (0-100)
 * @param[in] period        Timer period (GTPR value)
 * @return Compare match count value
 **********************************************************************************************************************/
static uint32_t comp_pwm_duty_to_counts(uint32_t duty_percent, uint32_t period)
{
    if (duty_percent >= COMP_PWM_MAX_DUTY)
    {
        return 0U;  /* 100% duty = compare match at 0 (always ON) */
    }
    if (duty_percent == COMP_PWM_MIN_DUTY)
    {
        return period;  /* 0% duty = compare match at GTPR (always OFF) */
    }

    /* Use uint32_t intermediate to prevent overflow on 16-bit multiplication */
    uint32_t counts = (uint32_t)period - (((uint32_t)period * (uint32_t)duty_percent) / COMP_PWM_MAX_DUTY);

    return (uint32_t)counts;
}

/***********************************************************************************************************************
 * @brief  Close all 3 GPT channels used for complementary PWM
 **********************************************************************************************************************/
static void comp_pwm_close_all_channels(void)
{
    /* Close in reverse order: slave2, slave1, master */
    R_GPT_Close(&g_timer_comp_master_ctrl);
    R_GPT_Close(&g_timer_comp_slave1_ctrl);
    R_GPT_Close(&g_timer_comp_slave2_ctrl);
}

/*******************************************************************************************************************//**
 * @} (end addtogroup r_gpt_ep)
 **********************************************************************************************************************/
