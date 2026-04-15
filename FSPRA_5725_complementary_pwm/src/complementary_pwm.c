/*
 * File name : complementary_pwm.c
 * Description : Implementation of GPT Complementary PWM mode 1, 2, 3, 4 for FPB-RA2T1
 * Based on RA2T1 User's Manual sections 20.3.3.7 and 20.3.3.8
 *
 * Complementary use 3 consecutive GPT channels (ch0=master, ch1=slave1, ch2=slave2)
 * to generate three-phase PWM with dead time for motor control applications.
 *
 * Mode: Buffer transfer at crest (GTCR.MD = 0xC)
 * Mode: Buffer transfer at trough (GTCR.MD = 0xD)
 * Mode: Buffer transfer at crest and trough (GTCR.MD = 0xE)
 * Mode: Immediate transfer (GTCR.MD = 0xF)
 */

#include "complementary_pwm.h"

/***********************************************************************************************************************
 * Private global variable declarations
 **********************************************************************************************************************/


/* Runtime configuration structure for complementary PWM */
static comp_pwm_cfg_t g_comp_pwm_cfg =
{
 .mode          = RESET_VALUE,
 .dead_time     = COMP_PWM_DEFAULT_DEAD_TIME,
 .period        = COMP_PWM_DEFAULT_PERIOD,
 .duty_u        = 0U,
 .duty_v        = 0U,
 .duty_w        = 0U,
 .double_buffer = false,
 .is_running    = false,
};

/**********************************************************************
 * Function prototypes
 *********************************************************************/
static fsp_err_t comp_pwm_open_channels(void);
static fsp_err_t comp_pwm_configure_dead_time(uint16_t dead_time_counts);
static fsp_err_t comp_pwm_set_compare_match(uint16_t duty_u, uint16_t duty_v, uint16_t duty_w);
static uint16_t  comp_pwm_convert_duty_to_counts(uint8_t duty_percent, uint16_t period);
static void comp_pwm_close_all_channels(void);

static uint32_t process_input_data(void);

/**********************************************************************
 * Function implementations
 *********************************************************************/

/**********************************************************************
 * @brief Initialize GPT channel for complementary PWM operation
 *
 * Step 1: Set operating mode (GTCR.MD) on Master channel
 * Step 2: Select count clock (GTCR.TPCS) on Master channel
 * Step 3: Set cycle (GTPR, GTPBR, GTPDBR) on Master channel
 * Step 4: Set GTIOCnm pin function (GTIOR) on all 3 channels
 * Step 5: Enable GTCPPOn pin output (GTIOR.PSYE) on Master channel
 * Step 6: Enable pin output (GTIOR.OAE/OBE) on all 3 channels
 * Step 7: Set buffer operation (GTBER2.CP3DB) - Mode 3 only
 * Step 8: Set compare match value (GTCCRA) on all 3 channels
 * Step 9: Set buffer value (GTCCRD/GTCCRF) on all 3 channes
 * Step 10: Set dead time value (GTDVU) on master channel
 *
 * @param[in] comp_mode Menu selection: COMP_PWM_MODE1_TIMER to COMP_PWM_MODE4_TIMER
 * @retval FSP_SUCCESS Initialization successful
 * @retval FSP_ERR_ALREADY_OPEN Timer already open
 * @retval other Error from FSP API
 *********************************************************************/
fsp_err_t comp_pwm_init(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint8_t comp_mode = RESET_VALUE;
    print_comp_pwm_menu();

    /* Map menu selection to internal mode identifier */
    comp_mode = (uint8_t)process_input_data();
    switch (comp_mode)
    {
        case COMP_PWM_MODE1_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE1;
            APP_PRINT("\r\n--- Complementary PWM Mode 1 (Transfer at crest) ---\r\n");
            break;
        case COMP_PWM_MODE2_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE2;
            APP_PRINT("\r\n--- Complementary PWM Mode 2 (Transfer at trough) ---\r\n");
            break;
        case COMP_PWM_MODE3_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE3;
            APP_PRINT("\r\n--- Complementary PWM Mode 3 (Transfer at crest & trough) ---\r\n");
            break;
        case COMP_PWM_MODE4_TIMER:
            g_comp_pwm_cfg.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE4;
            APP_PRINT("\r\n--- Complementary PWM Mode 4 (Immediate transfer) ---\r\n");
            break;
        default:
            APP_ERR_PRINT("\r\n** Invalid Complementary PWM Mode **\r\n");
            return FSP_ERR_INVALID_ARGUMENT;
    }

    /* If complementary PWM is already running, stop it first */
    if (true == g_comp_pwm_cfg.is_running)
    {
        comp_pwm_stop();
    }

    /* Steps 1-6: Open and configure all 3 GPT channels */
    err = comp_pwm_open_channels();
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM channel open FAILED **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }

    /* Steps 8-9: Set initial compare match values (GTCCRA) and buffer values (GTCCRD) */
    g_comp_pwm_cfg.duty_u = 50U;
    g_comp_pwm_cfg.duty_v = 50U;
    g_comp_pwm_cfg.duty_w = 50U;

    /* convert percentages to compare match counts*/
    uint16_t counts_u = comp_pwm_convert_duty_to_counts((uint8_t)g_comp_pwm_cfg.duty_u, g_comp_pwm_cfg.period);
    uint16_t counts_v = comp_pwm_convert_duty_to_counts((uint8_t)g_comp_pwm_cfg.duty_v, g_comp_pwm_cfg.period);
    uint16_t counts_w = comp_pwm_convert_duty_to_counts((uint8_t)g_comp_pwm_cfg.duty_w, g_comp_pwm_cfg.period);

    err = comp_pwm_set_compare_match(counts_u, counts_v, counts_w);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n**Compare match set failed **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }

    APP_PRINT(" Complementary PWM mode %d initialized successfully \r\n", g_comp_pwm_cfg.mode - 3);
    APP_PRINT(" Period: 0x%04X counts \r\n", g_comp_pwm_cfg.period);
    APP_PRINT(" Dead Time: 0x%04X counts \r\n", g_comp_pwm_cfg.dead_time);
    APP_PRINT(" Initial duty: U=%d%% V=%d%% W=%d%% \r\n", g_comp_pwm_cfg.duty_u, g_comp_pwm_cfg.duty_v, g_comp_pwm_cfg.duty_w);

    return FSP_SUCCESS;
}

/**********************************************************************
 * @brief Start Complementary PWM output on all 3 channels
 *
 * @retval      FSP_SUCCESS          Start successful
 * @retval      other          Error from FSP API
 *********************************************************************/
fsp_err_t comp_pwm_start(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* only start the master channel (ch0), which triggers all 3 channels to begin counting.*/
    err = R_GPT_Start(&g_timer_master_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM START failed **\r\n");
        return err;
    }

    g_comp_pwm_cfg.is_running = true;
    APP_PRINT(" Complementary PWM started (three-phase output is active) \r\n");

    return FSP_SUCCESS;
}

/**********************************************************************
 * @brief Open all 3 GPT channels for complementary PWM
 *
 * @param[in] NONE
 * @retval FSP_SUCCESS on success
 *********************************************************************/
static fsp_err_t comp_pwm_open_channels(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Open master channel (ch0)*/
    err = R_GPT_Open(&g_timer_master_ctrl, &g_timer_master_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Master channel (ch0) R_GPT_Open failed **\r\n");
        return err;
    }
    APP_PRINT(" Master channel (ch0) opened \r\n");

    /* Open slave channel 1 (ch1)*/
    err = R_GPT_Open(&g_timer_slave1_ctrl, &g_timer_slave1_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** slave channel1 (ch1) R_GPT_Open failed **\r\n");
        return err;
    }
    APP_PRINT(" slave channel (ch1) opened \r\n");

    /* Open slave channel 2 (ch2)*/
    err = R_GPT_Open(&g_timer_slave2_ctrl, &g_timer_slave2_cfg);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** slave channel2 (ch2) R_GPT_Open failed **\r\n");
        return err;
    }
    APP_PRINT(" slave channel 2 (ch2) opened \r\n");

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Stop complementary PWM and close all channels
 **********************************************************************************************************************/
/* Stopping the master channel stops all slaves */
void comp_pwm_stop(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Stop Master channel - Slave channels follow automatically */
    err = R_GPT_Stop(&g_timer_master_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM STOP failed **\r\n");
    }

    /* Close all 3 channels */
    comp_pwm_close_all_channels();

    g_comp_pwm_cfg.is_running     = false;
    g_comp_pwm_cfg.mode           = RESET_VALUE;

    APP_PRINT("Complementary PWM stopped and all channels closed \r\n");
}

/**********************************************************************
 * @brief Close all 3 GPT channels used for complementary PWM
 *********************************************************************/
static void comp_pwm_close_all_channels(void)
{
    /*Close in reverse order: slave2, slave1, master */
    R_GPT_Close(&g_timer_slave2_ctrl);
    R_GPT_Close(&g_timer_slave1_ctrl);
    R_GPT_Close(&g_timer_master_ctrl);

}

/**********************************************************************
 * @brief Set dead time value
 *
 * @param[in]   dead_time_counts            Dead time value in timer counts
 * @retval      FSP_SUCCESS                 Dead time set successfully
 * @return      FSP_ERR_INVALID_ARGUMENT    Dead time is out of range
 *********************************************************************/

fsp_err_t comp_pwm_set_dead_time(uint16_t dead_time_counts)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Validate dead time range */
    if ((dead_time_counts < COMP_PWM_MIN_DEAD_TIME) || (dead_time_counts > COMP_PWM_MAX_DEAD_TIME))
    {
        APP_ERR_PRINT("\r\n** Dead time value is out of range (0x%04X) **\r\n", dead_time_counts);
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* Dead time must be less than period: GTDVU < GTPR */
    if (dead_time_counts >= g_comp_pwm_cfg.period)
    {
        APP_ERR_PRINT("\r\n** Dead time must be less than period (0x%04X) **\r\n", g_comp_pwm_cfg.period);
    }

    g_comp_pwm_cfg.dead_time = dead_time_counts;

    /* If already running, update the dead time register directly */
    if (true == g_comp_pwm_cfg.is_running)
    {
        err = comp_pwm_configure_dead_time(dead_time_counts);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
        APP_PRINT("Dead time updated to 0x%04X counts \r\n", dead_time_counts);
    }
    return FSP_SUCCESS;
}

/**********************************************************************
 * @brief Configure dead time register (GTDVU) on master channel
 *
 * @param[in]   duty_percent    Duty cycle percentage (0-100)
 * @retval      period          Timer period (GTPR value)
 *********************************************************************/
static fsp_err_t comp_pwm_configure_dead_time(uint16_t dead_time_counts)
{
    (void)dead_time_counts; /* Suppress unused warning in skeleton */

    return FSP_SUCCESS;
}

/**********************************************************************
 * @brief Set three-phase duty cycles
 *
 * @param[in]   duty_u            U-phase duty cycle (0-100%)
 * @param[in]   duty_v            V-phase duty cycle (0-100%)
 * @param[in]   duty_w            W-phase duty cycle (0-100%)
 * @retval      FSP_SUCCESS                 Duty cycle updated successfully
 *********************************************************************/
fsp_err_t comp_pwm_set_duty_3phase(uint8_t duty_u, uint8_t duty_v, uint8_t duty_w)
{
    fsp_err_t err = FSP_SUCCESS;
    /* Validate inputs */
    if ((duty_u > COMP_PWM_MAX_DUTY) || (duty_v > COMP_PWM_MAX_DUTY) || (duty_w > COMP_PWM_MAX_DUTY))
    {
        APP_ERR_PRINT("\r\n** Duty cycle is out of range (max 100%) **\r\n");
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* Store new duty values */
    g_comp_pwm_cfg.duty_u = duty_u;
    g_comp_pwm_cfg.duty_v = duty_v;
    g_comp_pwm_cfg.duty_w = duty_w;

    /* Convert percentages to compare match counts*/
    uint16_t counts_u = comp_pwm_convert_duty_to_counts(duty_u, g_comp_pwm_cfg.period);
    uint16_t counts_v = comp_pwm_convert_duty_to_counts(duty_v, g_comp_pwm_cfg.period);
    uint16_t counts_w = comp_pwm_convert_duty_to_counts(duty_w, g_comp_pwm_cfg.period);

    /* Update compare match values */
    err = comp_pwm_set_compare_match(counts_u, counts_v, counts_w);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Duty cycle UPDATE failed **");
        return err;
    }

    APP_PRINT("Duty updated: U=%d%% V=%d%% W=%d%%\r\n", duty_u, duty_v, duty_w);

    return FSP_SUCCESS;
}

/**********************************************************************
 * @brief Convert percentages to compare match timer counts
 *
 * In complementary PWM mode:
 *   - when compare match value ≥ GTPR: duty 0% (positive-phase OFF)
 *   - when compare match value = 0: duty 100% (positive-phase ON)
 *
 * @param[in]   duty_percent    Duty cycle percentage (0-100)
 * @retval      period          Timer period (GTPR value)
 * @return                      compare match count value
 *********************************************************************/
static uint16_t comp_pwm_convert_duty_to_counts(uint8_t duty_percent, uint16_t period)
{
    if (duty_percent >= COMP_PWM_MAX_DUTY)
    {
        return 0U; /* 100% duty = compare match at 0 (always ON)*/
    }
    if (duty_percent == COMP_PWM_MIN_DUTY)
    {
        return period; /* 0% duty = compare match at GTPR (always OFF)*/
    }

     /* Use uint32_t immediate to prevent overflow on 16-bit multiplication*/
    uint32_t counts = (uint32_t)period - (((uint32_t)period * (uint32_t)duty_percent)/COMP_PWM_MAX_DUTY);

    return (uint16_t)counts;
}

/**********************************************************************
 * @brief Set compare match values for all 3 phases
 *
 * @param[in]  duty_u U-phase compare match count
 * @param[in]  duty_v V-phase compare match count
 * @param[in]  duty_w W-phase compare match count
 * @retval FSP_SUCCESS on success
 *********************************************************************/
static fsp_err_t comp_pwm_set_compare_match(uint16_t duty_u, uint16_t duty_v, uint16_t duty_w)
{
    fsp_err_t err = FSP_SUCCESS;

    /* set compare match on master channel (U-phase) */
    err = R_GPT_DutyCycleSet(&g_timer_master_ctrl, (uint32_t)duty_u, TIMER_PIN);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** U-phase DutyCycleSet failed **\r\n");
        return err;
    }

    /* set compare match on slave channel1 (V-phase) */
    err = R_GPT_DutyCycleSet(&g_timer_slave1_ctrl, (uint32_t)duty_v, TIMER_PIN);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** V-phase DutyCycleSet failed **\r\n");
        return err;
    }

    /* set compare match on slave channel2 (W-phase) */
    err = R_GPT_DutyCycleSet(&g_timer_slave2_ctrl, (uint32_t)duty_w, TIMER_PIN);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** W-phase DutyCycleSet failed **\r\n");
        return err;
    }

    return FSP_SUCCESS;
}



/**********************************************************************
 * @brief process user implementary PWM sub-menu
 *
 * This function implements the interactive sub-menu for configuring and
 * controlling complementary PWM operation via J-Link RTT viewer.
 *********************************************************************/
void comp_pwm_process_input(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint32_t sub_input = RESET_VALUE;
    print_comp_pwm_submenu();

    while (true)
    {
        if (APP_CHECK_DATA)
        {
            unsigned char buf[BUF_SIZE] = {0U};
            APP_READ(buf);
            sub_input = (uint32_t)(atoi((char *)buf));

            switch(sub_input)
            {
                case 1U: /* Set dead time */
                {
                    uint32_t dt = process_input_data();
                    err = comp_pwm_set_dead_time((uint16_t)dt);
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("Dead time set failed \r\n");
                    }
                    break;
                }

                case 2U: /* Set three-phase duty */
                {
                    uint32_t du = process_input_data();
                    uint32_t dv = process_input_data();
                    uint32_t dw = process_input_data();

                    err = comp_pwm_set_duty_3phase((uint8_t)du, (uint8_t)dv, (uint8_t)dw);
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("Duty cycle set failed \r\n");
                    }
                    break;
                }

                case 3U: /* Start complementary PWM */
                {
                    if (false == g_comp_pwm_cfg.is_running)
                    {
                        err = comp_pwm_start();
                        if (FSP_SUCCESS != err)
                        {
                            APP_ERR_PRINT("Complementary PWM START failed \r\n");
                        }
                    }
                    else
                    {
                        APP_PRINT("Complementary PWM is already running \r\n");
                    }
                    break;
                }

                case 4U: /* Stop complementary PWM */
                {
                    if (true == g_comp_pwm_cfg.is_running)
                    {
                        comp_pwm_stop();
                    }
                    else
                    {
                        APP_PRINT("Complementary PWM is already not running \r\n");
                    }
                    break;
                }
                case 5U: /* Show the current status of the complementary PWM */
                {
                    const comp_pwm_cfg_t * cfg = comp_pwm_get_config();
                    APP_PRINT("\r\n** Complementary PWM status **");
                    APP_PRINT("\r\n Mode: %d Running: %s \r\n", cfg->mode - 3, cfg->is_running ? "RUNNING" : "STOPPED"); // Ternary operator Running: YES nếu == true
                    APP_PRINT("\r\n Period: 0x%04X Dead Time: 0x%04X \r\n", cfg->period, cfg->dead_time);
                    APP_PRINT("\r\n Duty: U=%d%% V=%d%% W=%d%% \r\n", cfg->duty_u, cfg->duty_v, cfg->duty_w);
                    APP_PRINT("\r\n Double Buffer: %s \r\n", cfg->double_buffer ? "ON" : "OFF");
                    break;
                }

                case 0U: /* Return to main menu */
                {
                    APP_PRINT("Return to main menu \r\n");
                    return;
                }

                default:
                {
                    APP_PRINT("\r\n Invalid sub-menu option \r\n");
                    break;
                }
            }
            print_comp_pwm_submenu();
        }
    }
}

/**********************************************************************
 * @brief Print the complementary PWM submenu to RTT viewer
 *********************************************************************/
void print_comp_pwm_submenu(void)
{
    APP_PRINT("\r\n** Complementary PWM sub-menu **\r\n");
    APP_PRINT(" a. Enter 1 to set Dead Time \r\n");
    APP_PRINT(" b. Enter 2 to set three-phase duty (U, V, W) \r\n");
    APP_PRINT(" c. Enter 3 to start complementary PWM \r\n");
    APP_PRINT(" d. Enter 4 to stop complementary PWM \r\n");
    APP_PRINT(" e. Enter 5 to show current state \r\n");
    APP_PRINT(" f. Enter 0 to return to main sub-menu \r\n");
    APP_PRINT(" Comp PWM Input: ");
}

/**********************************************************************
 * @brief Print the complementary PWM submenu to RTT viewer
 *********************************************************************/
void print_comp_pwm_menu(void)
{
    APP_PRINT("\r\n** Complementary PWM menu **\r\n");
    APP_PRINT(" a. Enter 4 to set complementary mode 1 \r\n");
    APP_PRINT(" b. Enter 5 to set complementary mode 2 (U, V, W) \r\n");
    APP_PRINT(" c. Enter 6 to set complementary mode 3 \r\n");
    APP_PRINT(" d. Enter 7 to set complementary mode 4 \r\n");
    APP_PRINT(" f. Other to return invalid mode \r\n");
    APP_PRINT(" Comp PWM Input: ");
}

/***********************************************************************************************************************
 * @brief       Parse input string to integer value
 * @param[in]   None
 * @retval      Integer value of input string
 **********************************************************************************************************************/
static uint32_t process_input_data(void)
{
    unsigned char buf[BUF_SIZE] = {0U};
    uint32_t num_bytes          = RESET_VALUE;
    uint32_t value              = RESET_VALUE;

    while (RESET_VALUE == num_bytes)
    {
        if (APP_CHECK_DATA)
        {
            num_bytes = APP_READ(buf);
            if (RESET_VALUE == num_bytes)
            {
                /* Input string is invalid */
                APP_PRINT("\r\ninput string is invalid");
            }
        }
    }

    /* Conversion from input string to integer value */
    value = (uint32_t) (atoi((char *)buf));

    return value;
}

/**********************************************************************
 * @brief Get complementary PWM configuration
 *********************************************************************/
const comp_pwm_cfg_t * comp_pwm_get_config(void)
{
    return &g_comp_pwm_cfg;
}

