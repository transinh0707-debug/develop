/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @defgroup GPT_TESTS GPT Tests
 * @ingroup RENESAS_TESTS
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "common_utils.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define TIMER_TEST_COMPLEMENTARY_PWM (1U)
#define DELAY_VAL           (1000U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
timer_cfg_t g_timer_comp_pwm_master_test;
timer_cfg_t g_timer_comp_pwm_slave1_cfg_test;
timer_cfg_t g_timer_comp_pwm_slave2_cfg_test;
gpt_extended_cfg_t * p_extend;
gpt_three_phase_instance_ctrl_t * p_instance_ctrl_test;

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/

#if TIMER_TEST_COMPLEMENTARY_PWM                    // Complementary PWM testing needs to be updated. See FSPRA-5725

/* TC verify Operating mode 1: GTCCRC transfers to GTCCRA at the end of the crest section|Must have|Transfer timing at crest boundary */
void comp_pwm_test_mode1(three_phase_ctrl_t p_ctrl_test, three_phase_cfg_t p_cfg_test)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Open and configure all 3 GPT channels*/
    g_timer_comp_pwm_master_test.mode = TIMER_MODE_COMPLEMENTARY_PWM_MODE1;

    err = R_GPT_THREE_PHASE_Open();
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n[INOFR] Complementary PWM channel open SUCCESS \r\n");
        return err;
    }

    /* Set initial compare match values (GTCCRA) and buffer values (GTCCRD) */
    err = R_GPT_THREE_PHASE_DutyCycleSet();
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Compare match set FAILED **\r\n");
        comp_pwm_close_all_channels();
        return err;
    }

    APP_PRINT("Complementary PWM Mode %d initialized successfully\r\n", g_timer_comp_pwm_master_test.mode - 11);
    APP_PRINT("  Period: 0x%04X counts\r\n", g_timer_comp_pwm_master_cfg.period_counts);
    APP_PRINT("  Dead Time: 0x%04X counts\r\n", p_extend->p_pwm_cfg->dead_time_count_up);
    APP_PRINT("Duty updated:\nvalue_u=%d\nvalue_v=%d\nvalue_w=%d\r\n", g_timer_comp_pwm_master_cfg.duty_cycle_counts, g_timer_comp_pwm_slave1_cfg.duty_cycle_counts, g_timer_comp_pwm_slave2_cfg.duty_cycle_counts);

    /* Start complementary PWM output on all 3 channels */
    err = R_GPT_THREE_PHASE_Start(&p_instance_ctrl_test->p_cfg->p_timer_instance[0]);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM START failed **\r\n");
        return err;
    }

    R_BSP_SoftwareDelay((uint32_t) DELAY_VAL, BSP_DELAY_UNITS_MICROSECONDS);

    err = R_GPT_THREE_PHASE_Stop(&p_instance_ctrl_test);
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n** Complementary PWM STOP failed **\r\n");
    }

}
#endif
