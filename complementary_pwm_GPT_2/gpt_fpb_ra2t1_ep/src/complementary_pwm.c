/***********************************************************************************************************************
 * File Name    : complementary_pwm.c
 * Description  : Implementation of GPT Complementary PWM Modes 1, 2, 3, 4
 *
 *                Mode 1: Buffer transfer at crest   (GTCR.MD = 0xC)
 *                Mode 2: Buffer transfer at trough  (GTCR.MD = 0xD)
 *                Mode 3: Buffer transfer at crest and trough (GTCR.MD = 0xE)
 *                Mode 4: Immediate transfer         (GTCR.MD = 0xF)
 **********************************************************************************************************************/

#include "common_utils.h"
#include "gpt_timer.h"
#include "complementary_pwm.h"

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/* Store open state (extern from gpt_timer.c) */
static timer_mode_t g_timer_mode;

timer_cfg_t g_timer_comp_master_cfg_test;
timer_cfg_t g_timer_comp_slave1_cfg_test;
timer_cfg_t g_timer_comp_slave2_cfg_test;
gpt_extended_cfg_t * p_extend;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_set_compare_match(uint32_t duty_u, uint32_t duty_v, uint32_t duty_w);
static uint32_t  comp_pwm_duty_to_counts(uint32_t duty_percent, uint32_t period);
static void      comp_pwm_close_all_channels(void);

/***********************************************************************************************************************
 * Public function implementations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * @brief  Initialize GPT channels for complementary PWM operation
 *
 * @param[in] comp_mode  Menu selection: TIMER_MODE_COMPLEMENTARY_PWM_MODE1_TIMER to TIMER_MODE_COMPLEMENTARY_PWM_MODE4
 * @retval FSP_SUCCESS          Initialization successful
 * @retval FSP_ERR_ALREADY_OPEN Timer already open
 * @retval other                Error from FSP API
 **********************************************************************************************************************/
fsp_err_t comp_pwm_init(uint8_t comp_mode)
{
    fsp_err_t err = FSP_SUCCESS;


    /* Map menu selection to internal mode identifier */
    switch (comp_mode)
    {
        case COMP_PWM_MODE1_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE1;
            APP_PRINT("\r\n--- Complementary PWM Mode 1 (Transfer at Crest) ---\r\n");
            break;
        case COMP_PWM_MODE2_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE2;
            APP_PRINT("\r\n--- Complementary PWM Mode 2 (Transfer at Trough) ---\r\n");
            break;
        case COMP_PWM_MODE3_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE3;
            APP_PRINT("\r\n--- Complementary PWM Mode 3 (Transfer at Crest & Trough) ---\r\n");
            break;
        case COMP_PWM_MODE4_TIMER:
            g_timer_mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE4;
            APP_PRINT("\r\n--- Complementary PWM Mode 4 (Immediate Transfer) ---\r\n");
            break;
        default:
            APP_ERR_PRINT("\r\n** Invalid Complementary PWM mode **\r\n");
            return FSP_ERR_INVALID_ARGUMENT;
    }

    /* Open and configure all 3 GPT channels*/
       g_timer_comp_master_cfg_test.mode        = g_timer_mode;

       /* Open master channel (ch0) */
       err = R_GPT_Open(&g_timer_comp_master_ctrl, &g_timer_comp_master_cfg_test);
       if (FSP_SUCCESS != err)
       {
           APP_ERR_PRINT("\r\n** Master channel (ch0) R_GPT_Open FAILED **\r\n");
           return err;
       }
       APP_PRINT("\nMaster channel 0 opened\r\n");

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
       APP_PRINT("\n Complementary PWM channel open SUCCESS \r\n");

    /* Set initial compare match values (GTCCRA) and buffer values (GTCCRD) */
    err = comp_pwm_set_compare_match(g_timer_comp_master_cfg.duty_cycle_counts, g_timer_comp_slave1_cfg.duty_cycle_counts, g_timer_comp_slave2_cfg.duty_cycle_counts);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Compare match set FAILED **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }
    else
    {
         APP_PRINT("Duty updated:\nvalue_u=%d\nvalue_v=%d\nvalue_w=%d\r\n", g_timer_comp_master_cfg.duty_cycle_counts, g_timer_comp_slave1_cfg.duty_cycle_counts, g_timer_comp_slave2_cfg.duty_cycle_counts);
    }

    APP_PRINT("Complementary PWM Mode %d initialized successfully\r\n", g_timer_comp_master_cfg_test.mode - 11);
    APP_PRINT("  Period: 0x%04X counts\r\n", g_timer_comp_master_cfg.period_counts);
    APP_PRINT("  Dead Time: 0x%04X counts\r\n", p_extend->p_pwm_cfg->dead_time_count_up);

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Start complementary PWM output on all 3 channels
 *
 * @retval FSP_SUCCESS  Start successful
 * @retval other        Error from FSP API
 **********************************************************************************************************************/
fsp_err_t comp_pwm_start(void)
{
    fsp_err_t err = FSP_SUCCESS;

    err = R_GPT_Start(&g_timer_comp_master_ctrl);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM START failed **\r\n");
        return err;
    }

    APP_PRINT("Complementary PWM started (3-phase output active)\r\n");

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Stop complementary PWM and close all channels
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
     comp_pwm_close_all_channels();

    APP_PRINT("Complementary PWM stopped and all channels closed\r\n");
}


/***********************************************************************************************************************
 * @brief  Set 3-phase duty cycles
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

    /* Convert percentages to compare match counts */
    g_timer_comp_master_cfg_test.duty_cycle_counts = comp_pwm_duty_to_counts(duty_u, g_timer_comp_master_cfg.period_counts);
    g_timer_comp_slave1_cfg_test.duty_cycle_counts = comp_pwm_duty_to_counts(duty_v, g_timer_comp_master_cfg.period_counts);
    g_timer_comp_slave2_cfg_test.duty_cycle_counts = comp_pwm_duty_to_counts(duty_w, g_timer_comp_master_cfg.period_counts);

    /* Update compare match values.*/
    err = comp_pwm_set_compare_match(g_timer_comp_master_cfg_test.duty_cycle_counts, g_timer_comp_slave1_cfg_test.duty_cycle_counts, g_timer_comp_slave2_cfg_test.duty_cycle_counts);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Duty cycle update FAILED **\r\n");
        return err;
    }

    APP_PRINT("Duty updated:\nU=%d%%\tvalue_u=%d \nV=%d%%\tvalue_v=%d \nW=%d%%\tvalue_w=%d\r\n", duty_u, g_timer_comp_master_cfg_test.duty_cycle_counts, duty_v, g_timer_comp_slave1_cfg_test.duty_cycle_counts, duty_w, g_timer_comp_slave2_cfg_test.duty_cycle_counts);

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * @brief  Print the complementary PWM submenu to RTT Viewer
 **********************************************************************************************************************/
void print_comp_pwm_menu(void)
{
    APP_PRINT("\r\n=== Complementary PWM Sub-Menu ===\r\n");
    APP_PRINT("Enter 1 to set init\r\n");
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
                    fsp_err_t err = comp_pwm_start();
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_PRINT("Complementary PWM start failed\r\n");
                    }
                    break;
                }

                case 5U:  /* Stop */
                {
                    comp_pwm_stop();
                    break;
                }

                case 6U:  /* Show status */
                {
                    APP_PRINT("\r\n--- Complementary PWM Status ---\r\n");
                    APP_PRINT("  Mode: %d \r\n", g_timer_comp_master_cfg_test.mode - 11);
                    APP_PRINT("  Period: 0x%04X  Dead Time: 0x%04X\r\n", g_timer_comp_master_cfg_test.period_counts, p_extend->p_pwm_cfg->dead_time_count_up);
                    APP_PRINT("  Duty Counts: U=%d  V=%d  W=%d\r\n", g_timer_comp_master_cfg_test.duty_cycle_counts, g_timer_comp_slave1_cfg_test.duty_cycle_counts, g_timer_comp_slave2_cfg_test.duty_cycle_counts);
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



/***********************************************************************************************************************
 * @brief  Set compare match values for all 3 phases
 *
 * @param[in] duty_u  U-phase compare match count
 * @param[in] duty_v  V-phase compare match count
 * @param[in] duty_w  W-phase compare match count
 * @retval FSP_SUCCESS on success
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_set_compare_match(uint32_t duty_u, uint32_t duty_v, uint32_t duty_w)
{
    fsp_err_t err = FSP_SUCCESS;

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

