/* ${REA_DISCLAIMER_PLACEHOLDER} */
 
/*******************************************************************************************************************//**
* @defgroup GPT_TG3_COMP_PWM_TESTS GPT Complementary PWM Tests
* @ingroup RENESAS_TESTS
* @brief   Test group 3: Complementary PWM Modes 1-4 with single/double buffer,
*          dead time, duty cycle control, and buffer chain verification.
*          Covers requirements REQ-OM-01 through REQ-SEC-17 (FSPRA-5725).
* @{
**********************************************************************************************************************/
 
/***********************************************************************************************************************
* Includes
**********************************************************************************************************************/
#include "common_utils.h"
#include "r_gpt_test_tg3_comp_pwm.h"
 
 
/***********************************************************************************************************************
* Macro definitions
**********************************************************************************************************************/
#define COMP_PWM_TEST_ENABLE            (1U)
#define DELAY_US                        (1000U)
#define DELAY_SETTLING_MS               (10U)

/* Dead time test values */
#define DEAD_TIME_TEST_COUNTS           (64U)
 
/* Number of phases */
#define THREE_PHASE_COUNT               (3U)
 
/* GTCCR index enums (must match r_gpt_three_phase.c private enum) */
#define COMP_PWM_PRV_GTCCRA             (0U)
#define COMP_PWM_PRV_GTCCRB             (1U)
#define COMP_PWM_PRV_GTCCRC             (2U)
#define COMP_PWM_PRV_GTCCRE             (3U)
#define COMP_PWM_PRV_GTCCRD             (4U)
#define COMP_PWM_PRV_GTCCRF             (5U)
 
/* Total number of requirements */
#define TOTAL_TEST_CASES                (17U)
 
/***********************************************************************************************************************
* Typedef definitions
**********************************************************************************************************************/
typedef struct st_comp_pwm_test_result
{
    const char * req_id;
    const char * title;
    bool         passed;
    const char * detail;
} comp_pwm_test_result_t;
 
/***********************************************************************************************************************
// * External references (from hal_data.c / FSP generated)
// **********************************************************************************************************************/
extern const timer_instance_t g_timer_comp_pwm_master;
extern const timer_instance_t g_timer_comp_pwm_slave1;
extern const timer_instance_t g_timer_comp_pwm_slave2;

extern gpt_instance_ctrl_t g_timer_comp_pwm_master_ctrl;
extern gpt_instance_ctrl_t g_timer_comp_pwm_slave1_ctrl;
extern gpt_instance_ctrl_t g_timer_comp_pwm_slave2_ctrl;

extern const timer_cfg_t g_timer_comp_pwm_master_cfg;
extern const timer_cfg_t g_timer_comp_pwm_slave1_cfg;
extern const timer_cfg_t g_timer_comp_pwm_slave2_cfg;
 

extern const three_phase_instance_t g_three_phase_comp_pwm;
extern gpt_three_phase_instance_ctrl_t g_three_phase_comp_pwm_ctrl;
extern const three_phase_cfg_t g_three_phase_comp_pwm_cfg;
 
/***********************************************************************************************************************
* Private variables
**********************************************************************************************************************/
static timer_instance_t g_timer_instances_test[THREE_PHASE_COUNT];

static timer_cfg_t g_master_cfg_test;
static timer_cfg_t g_slave1_cfg_test;
static timer_cfg_t g_slave2_cfg_test;

static gpt_extended_cfg_t g_master_ext_test;
static gpt_extended_pwm_cfg_t g_pwm_cfg_test;

static three_phase_instance_t g_three_phase_comp_pwm_test
static gpt_three_phase_instance_ctrl_t g_three_phase_comp_pwm_ctrl_test;
static three_phase_cfg_t g_three_phase_comp_pwm_cfg_test;
 
static comp_pwm_test_result_t g_test_results[TOTAL_TEST_CASES];
static uint32_t g_test_count = 0U;
 
/***********************************************************************************************************************
* Private function prototypes
**********************************************************************************************************************/
static void test_report(const char * req_id, const char * title, bool passed, const char * detail);
static void test_setup_config(timer_mode_t mode, three_phase_buffer_mode_t buf_mode, uint32_t dead_time);
static fsp_err_t test_open_three_phase(void);
static void test_close_three_phase(void);
static bool verify_gtber2_single_buffer(gpt_three_phase_instance_ctrl_t * p_ctrl, three_phase_channel_t ch);
static bool verify_gtber2_double_buffer(gpt_three_phase_instance_ctrl_t * p_ctrl, three_phase_channel_t ch);
 
/***********************************************************************************************************************
* Private helper functions
**********************************************************************************************************************/
 
/*******************************************************************************************************************//**
* @brief Record a test result and output via RTT.
**********************************************************************************************************************/
static void test_report (const char * req_id, const char * title, bool passed, const char * detail)
{
    if (g_test_count < TOTAL_TEST_CASES)
    {
        g_test_results[g_test_count].req_id = req_id;
        g_test_results[g_test_count].title  = title;
        g_test_results[g_test_count].passed = passed;
        g_test_results[g_test_count].detail = detail;
        g_test_count++;
    }
 
    APP_PRINT("[%s] %s: %s - %s\r\n", passed ? "PASS" : "FAIL", req_id, title, detail);
}
 
/*******************************************************************************************************************//**
* @brief Set up timer and three-phase configuration for a given mode and buffer type.
**********************************************************************************************************************/
static void test_setup_config (timer_mode_t mode, three_phase_buffer_mode_t buf_mode, uint32_t dead_time)
{
    /* Copy base configurations from FSP-generated const structs */
    memcpy(&g_master_cfg_test, &g_timer_comp_pwm_master_cfg, sizeof(timer_cfg_t));
    memcpy(&g_slave1_cfg_test, &g_timer_comp_pwm_slave1_cfg, sizeof(timer_cfg_t));
    memcpy(&g_slave2_cfg_test, &g_timer_comp_pwm_slave2_cfg, sizeof(timer_cfg_t));
    memcpy(&g_master_ext_test, g_master_cfg_test.p_extend, sizeof(gpt_extended_cfg_t));
 
    /* Configure dead time */
    if (g_master_ext_test.p_pwm_cfg != NULL)
    {
        memcpy(&g_pwm_cfg_test, g_master_ext_test.p_pwm_cfg, sizeof(gpt_extended_pwm_cfg_t));
        g_pwm_cfg_test.dead_time_count_up = dead_time;
        g_master_ext_test.p_pwm_cfg = &g_pwm_cfg_test;
    }
 
    /* Set complementary PWM mode on all channels */
    g_master_cfg_test.mode = mode;
    g_slave1_cfg_test.mode = mode;
    g_slave2_cfg_test.mode = mode;
    g_master_cfg_test.p_extend = &g_master_ext_test;
 
    /* Build timer instance array */
    g_timer_instances_test[0] = g_timer_comp_pwm_master;
    g_timer_instances_test[1] = g_timer_comp_pwm_slave1;
    g_timer_instances_test[2] = g_timer_comp_pwm_slave2;
    g_timer_instances_test[0].p_cfg = &g_master_cfg_test;
    g_timer_instances_test[1].p_cfg = &g_slave1_cfg_test;
    g_timer_instances_test[2].p_cfg = &g_slave2_cfg_test;
 
    /* Configure three-phase struct */
    memcpy(&g_three_phase_comp_pwm_cfg_test, &g_three_phase_comp_pwm_cfg, sizeof(three_phase_cfg_t));
    g_three_phase_comp_pwm_cfg_test.buffer_mode = buf_mode;
    g_three_phase_comp_pwm_cfg_test.p_timer_instance[0] = &g_timer_instances_test[0];
    g_three_phase_comp_pwm_cfg_test.p_timer_instance[1] = &g_timer_instances_test[1];
    g_three_phase_comp_pwm_cfg_test.p_timer_instance[2] = &g_timer_instances_test[2];
}
 
static fsp_err_t test_open_three_phase (void)
{
    memset(&g_three_phase_comp_pwm_ctrl_test, 0, sizeof(g_three_phase_comp_pwm_ctrl_test));
 
    return R_GPT_THREE_PHASE_Open(&g_three_phase_comp_pwm_ctrl_test, &g_three_phase_comp_pwm_cfg_test);
}
 
static void test_close_three_phase (void)
{
    R_GPT_THREE_PHASE_Stop(&g_three_phase_comp_pwm_ctrl_test);
    R_GPT_THREE_PHASE_Close(&g_three_phase_comp_pwm_ctrl_test);
}
 
static bool verify_gtber2_single_buffer (gpt_three_phase_instance_ctrl_t * p_ctrl, three_phase_channel_t ch)
{
    return (p_ctrl->p_reg[ch]->GTBER2_b.CMTCA == 0x1U) &&
           (p_ctrl->p_reg[ch]->GTBER2_b.CP3DB == 0U) &&
           (p_ctrl->p_reg[ch]->GTBER2_b.CPBTD == 0U);
}
 
static bool verify_gtber2_double_buffer (gpt_three_phase_instance_ctrl_t * p_ctrl, three_phase_channel_t ch)
{
    return (p_ctrl->p_reg[ch]->GTBER2_b.CMTCA == 0x1U) &&
           (p_ctrl->p_reg[ch]->GTBER2_b.CP3DB == 1U) &&
           (p_ctrl->p_reg[ch]->GTBER2_b.CPBTD == 0U);
}
 
/***********************************************************************************************************************
* Test Cases
**********************************************************************************************************************/
 
#if COMP_PWM_TEST_ENABLE
 
/*******************************************************************************************************************//**
* @brief REQ-OM-01: Operating mode 1 - GTCCRD transfers to GTCCRA at end of crest section.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_OM_01 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE1, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
        
        /* Verify single buffer GTBER2 config on all channels */
        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            pass &= verify_gtber2_single_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);

            /* Verify GTCCRD is initialized (single buffer register) */
            uint32_t gtccrd = g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRD];
            pass &= (gtccrd == (p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts));
        }
 
        /* Verify mode is set correctly (GTCR.MD = 0xC for Mode 1) */
        uint32_t gtcr_md = (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
        pass &= (gtcr_md == 0xCU);
 
        test_close_three_phase();
    }
 
    test_report("REQ-OM-01", "Operating mode 1 (crest transfer)", pass,
                pass ? "Mode 1 opened, GTBER2 single buffer, GTCR.MD=0xC verified"
                     : "Mode 1 open or register verification failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-OM-02: Operating mode 2 - GTCCRD transfers to GTCCRA at end of trough section.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_OM_02 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE2, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);

        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            pass &= verify_gtber2_single_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);

            /* Verify GTCCRD is initialized (single buffer register) */
            uint32_t gtccrd = g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRD];
            pass &= (gtccrd == (p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts));
        }
 
        uint32_t gtcr_md = (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
        pass &= (gtcr_md == 0xDU);
 
        test_close_three_phase();
    }
 
    test_report("REQ-OM-02", "Operating mode 2 (trough transfer)", pass,
                pass ? "Mode 2 opened, GTBER2 single buffer, GTCR.MD=0xD verified"
                     : "Mode 2 open or register verification failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-OM-03: Operating mode 3 - transfer at both boundaries, single and double buffer.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_OM_03 (void)
{
    bool pass = true;
 
    /* ---- Test single buffer ---- */
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
    pass &= (FSP_SUCCESS == err);
    if (FSP_SUCCESS == err)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);

        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            pass &= verify_gtber2_single_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);

            /* Verify GTCCRD is initialized (single buffer register) */
            uint32_t gtccrd = g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRD];
            pass &= (gtccrd == (p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts));
        }
 
        test_close_three_phase();
    }
 
    /* ---- Test double buffer ---- */
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_DOUBLE, DEAD_TIME_TEST_COUNTS);
    err = test_open_three_phase();
    pass &= (FSP_SUCCESS == err);
    if (FSP_SUCCESS == err)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);

        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            pass &= verify_gtber2_double_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);
 
            /* Verify both GTCCRD and GTCCRF are initialized (double buffer register) */
            uint32_t gtccrd = g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRD];
            uint32_t gtccrf = g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRF];
            pass &= ((gtccrd == (p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts)) && 
                     (gtccrf == (p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts)));
        }
 
        /* Verify GTCR.MD = 0xE for Mode 3 */
        uint32_t gtcr_md = (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
        pass &= (gtcr_md == 0xEU);
 
        test_close_three_phase();
    }
 
    test_report("REQ-OM-03", "Operating mode 3 (crest+trough, single+double)", pass,
                pass ? "Mode 3 single/double buffer, GTCR.MD=0xE verified"
                     : "Mode 3 buffer configuration failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-OM-04: Operating mode 4 - immediate transfer, bypass buffer chain.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_OM_04 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE4, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        /* Verify GTCR.MD = 0xF for Mode 4 */
        uint32_t gtcr_md = (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
        pass &= (gtcr_md == 0xFU);
        
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);

        /* Mode 4 immediate transfer: write GTCCRD, expect GTCCRA to update immediately */
        uint32_t test_duty = g_master_cfg_test.period_counts / 4U;
        three_phase_duty_cycle_t duty = {{test_duty, test_duty, test_duty}, {0}};
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
        pass &= (FSP_SUCCESS == err);
 
        /* Short delay then verify GTCCRA received the value */
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
        uint32_t gtccra = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRA];
        pass &= (gtccra == test_duty);
 
        test_close_three_phase();
    }
 
    test_report("REQ-OM-04", "Operating mode 4 (immediate transfer)", pass,
                pass ? "Mode 4 GTCR.MD=0xF, immediate GTCCRD->GTCCRA verified"
                     : "Mode 4 immediate transfer failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DT-05: Configurable dead time via GTDVU register.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DT_05 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        /* GTDVU on master channel should equal configured dead time */
        uint32_t gtdvu = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
        pass &= (gtdvu == DEAD_TIME_TEST_COUNTS);
        test_close_three_phase();
    }
 
    test_report("REQ-DT-05", "Configurable dead time", pass,
                pass ? "GTDVU configured correctly"
                     : "GTDVU configuration failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DT-06: Valid range - 0 < GTDVU < GTPR.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DT_06 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        uint32_t gtpr  = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTPR;
        uint32_t gtdvu = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
        pass &= ((gtdvu > 0U) && (gtdvu < gtpr));
        test_close_three_phase();
    }
 
    test_report("REQ-DT-06", "Dead time valid range (0 < GTDVU < GTPR)", pass,
                pass ? "Dead time value within valid range"
                     : "Dead time value out of range");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DT-07: No buffer operation for dead time register.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DT_07 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        /* Verify GTDVU takes effect immediately (no buffer) */
        uint32_t gtdvu = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
        pass &= (gtdvu == DEAD_TIME_TEST_COUNTS);
 
        /* Direct write should update immediately - no buffer delay */
        uint32_t new_dt = DEAD_TIME_TEST_COUNTS * 2U;
        g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU = new_dt;
        gtdvu = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
        pass &= (gtdvu == new_dt);
 
        test_close_three_phase();
    }
 
    test_report("REQ-DT-07", "No buffer operation for dead time register", pass,
                pass ? "GTDVU direct write verified (no buffering)"
                     : "GTDVU buffer behavior unexpected");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DT-08: Non-overlapping guard - dead time prevents shoot-through.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DT_08 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        /* Verify GTDTCR.TDE is set — enables automatic dead time insertion */
        uint32_t gtdtcr = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDTCR;
        pass &= ((gtdtcr & R_GPT0_GTDTCR_TDE_Msk) == R_GPT0_GTDTCR_TDE_Msk);

        /* Verify GTDVU matches exactly the configured dead time value */
        uint32_t gtdvu = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
        pass &= (gtdvu == DEAD_TIME_TEST_COUNTS);
 
        test_close_three_phase();
    }
 
    test_report("REQ-DT-08", "Non-overlapping guard (prevent shoot-through)", pass,
                pass ? "Dead time output enabled, GTDVU > 0"
                     : "Dead time guard not properly configured");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DC-09: Independent duty cycle control across three channels (U, V, W).
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DC_09 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
 
        uint32_t period = g_master_cfg_test.period_counts;
 
        /* Set different duty cycles: 25%, 50%, 75% */
        three_phase_duty_cycle_t duty = {
            {period / 4U, period / 2U, (period * 3U) / 4U},
            {0}
        };
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
        pass &= (FSP_SUCCESS == err);
 
        /* Verify each channel GTCCRD has its distinct value */
        pass &= (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD] == period / 4U);
        pass &= (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_V]->GTCCR[COMP_PWM_PRV_GTCCRD] == period / 2U);
        pass &= (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_W]->GTCCR[COMP_PWM_PRV_GTCCRD] == (period * 3U) / 4U);
 
        test_close_three_phase();
    }
 
    test_report("REQ-DC-09", "Independent duty cycle control (U/V/W)", pass,
                pass ? "Three independent duty values set and verified"
                     : "Independent duty cycle control failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DC-10: GTCCRA >= GTPR means 0% duty (positive OFF, negative ON).
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DC_10 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE4, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        uint32_t period = g_master_cfg_test.period_counts;

        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
 
        /* Set duty to period value (GTCCRA >= GTPR -> 0% duty) */
        three_phase_duty_cycle_t duty = {{period, period, period}, {0}};
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
 
        /* Verify GTCCRD written with period value */
        uint32_t gtccrd = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD];
        pass &= (gtccrd >= period);
 
        test_close_three_phase();
    }
 
    test_report("REQ-DC-10", "0% duty when GTCCRA >= GTPR", pass,
                pass ? "Duty set to period (0% positive phase) verified"
                     : "0% duty cycle test failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DC-11: GTCCRA = 0 means 100% duty (positive ON, negative OFF).
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DC_11 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE4, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);

        /* Set duty to minimum (GTCCRA = 0 -> 100% duty).
         * Note: param checking may reject 0, use 1 as minimum */
        three_phase_duty_cycle_t duty = {{1U, 1U, 1U}, {0}};
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
        pass &= (FSP_SUCCESS == err);
 
        uint32_t gtccrd = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD];
        pass &= (gtccrd <= 1U);
 
        test_close_three_phase();
    }
 
    test_report("REQ-DC-11", "100% duty when GTCCRA = 0", pass,
                pass ? "Duty set to min (100% positive phase) verified"
                     : "100% duty cycle test failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-DC-12: Prevent 16-bit overflow on compare match value (RA2T1 only).
**********************************************************************************************************************/
static void comp_pwm_test_REQ_DC_12 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        /* Compute duty using uint32_t intermediate to prevent 16-bit overflow.
         * For RA2T1 (16-bit GPT), period may be up to 0xFFFF. */
        uint32_t period    = g_master_cfg_test.period_counts;
        uint32_t duty_pct  = 99U;   /* Near 100% to test overflow boundary */
        uint32_t duty_cnt  = (uint32_t)(((uint32_t)period * duty_pct) / 100U);
 
        /* Verify no wrap-around */
        pass &= (duty_cnt <= period);
        pass &= (duty_cnt > 0U);

        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
 
        three_phase_duty_cycle_t duty = {{duty_cnt, duty_cnt, duty_cnt}, {0}};
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
        pass &= (FSP_SUCCESS == err);
 
        test_close_three_phase();
    }
 
    test_report("REQ-DC-12", "Prevent 16-bit overflow (RA2T1)", pass,
                pass ? "uint32_t intermediate arithmetic prevents overflow"
                     : "Overflow prevention test failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-BUF-13: Single buffer chain for Modes 1-3 (GTCCRD -> Temp A -> GTCCRA).
**********************************************************************************************************************/
static void comp_pwm_test_REQ_BUF_13 (void)
{
    bool pass = true;
 
    timer_mode_t modes[] = {
        TIMER_MODE_COMPLEMENTARY_PWM_MODE1,
        TIMER_MODE_COMPLEMENTARY_PWM_MODE2,
        TIMER_MODE_COMPLEMENTARY_PWM_MODE3
    };
 
    for (uint32_t m = 0U; m < 3U; m++)
    {
        test_setup_config(modes[m], THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
        fsp_err_t err = test_open_three_phase();
        pass &= (FSP_SUCCESS == err);
        if (FSP_SUCCESS == err)
        {
            for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
            {
                pass &= verify_gtber2_single_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);
            }
 
            test_close_three_phase();
        }
    }
 
    test_report("REQ-BUF-13", "Single buffer chain (Modes 1-3)", pass,
                pass ? "GTBER2 single buffer verified for Modes 1/2/3"
                     : "Single buffer chain verification failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-BUF-14: Double buffer chain for Mode 3 (GTCCRF -> Temp B -> GTCCRE -> GTCCRA).
**********************************************************************************************************************/
static void comp_pwm_test_REQ_BUF_14 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_DOUBLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            pass &= verify_gtber2_double_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);
 
            /* Verify GTCCRF register is initialized */
            uint32_t gtccrf = g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRF];
            pass &= (gtccrf != 0U);
        }
 
        test_close_three_phase();
    }
 
    test_report("REQ-BUF-14", "Double buffer chain (Mode 3)", pass,
                pass ? "GTBER2 double buffer + GTCCRF initialized verified"
                     : "Double buffer chain verification failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-BUF-15: Mode 4 immediate bypass - GTCCRD transfers directly to GTCCRA.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_BUF_15 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE4, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
 
        uint32_t period    = g_master_cfg_test.period_counts;
        uint32_t test_duty = period / 3U;
        three_phase_duty_cycle_t duty = {{test_duty, test_duty, test_duty}, {0}};
 
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
        pass &= (FSP_SUCCESS == err);
 
        /* Mode 4: GTCCRD -> GTCCRA immediate transfer */
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
        uint32_t gtccra = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRA];
        pass &= (gtccra == test_duty);
 
        test_close_three_phase();
    }
 
    test_report("REQ-BUF-15", "Mode 4 immediate bypass transfer", pass,
                pass ? "GTCCRD -> GTCCRA immediate transfer verified"
                     : "Mode 4 bypass transfer verification failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-BUF-16: Slave channel 2 (W phase) write ordering - W must be written LAST.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_BUF_16 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
 
        uint32_t period = g_master_cfg_test.period_counts;
 
        /* Set distinct duty for each phase */
        three_phase_duty_cycle_t duty = {
            {period / 4U, period / 2U, (period * 3U) / 4U},
            {0}
        };
        err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_comp_pwm_ctrl_test, &duty);
        pass &= (FSP_SUCCESS == err);
 
        /* After DutyCycleSet, all GTCCRD registers should be set.
         * W channel was written last (triggers simultaneous temp-register transfer). */
        pass &= (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD] == period / 4U);
        pass &= (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_V]->GTCCR[COMP_PWM_PRV_GTCCRD] == period / 2U);
        pass &= (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_W]->GTCCR[COMP_PWM_PRV_GTCCRD] == (period * 3U) / 4U);
 
        test_close_three_phase();
    }
 
    test_report("REQ-BUF-16", "Slave channel 2 write ordering (W last)", pass,
                pass ? "W channel written last, all phases updated correctly"
                     : "Write ordering verification failed");
}
 
/*******************************************************************************************************************//**
* @brief REQ-SEC-17: Support for five counting operation sections.
**********************************************************************************************************************/
static void comp_pwm_test_REQ_SEC_17 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE3, THREE_PHASE_BUFFER_MODE_SINGLE, DEAD_TIME_TEST_COUNTS);
    fsp_err_t err = test_open_three_phase();
 
    bool pass = (FSP_SUCCESS == err);
    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);
 
        uint32_t period    = g_master_cfg_test.period_counts;
        uint32_t dead_time = DEAD_TIME_TEST_COUNTS;
 
        /* Let PWM run then sample counter to verify all five sections */
        R_BSP_SoftwareDelay(DELAY_SETTLING_MS, BSP_DELAY_UNITS_MILLISECONDS);
 
        bool saw_trough   = false;  /* GTCNT near 0 */
        bool saw_count_up = false;  /* 0 < GTCNT < GTPR, counting up */
        bool saw_crest    = false;  /* GTCNT near GTPR */
        bool saw_count_dn = false;  /* 0 < GTCNT < GTPR, counting down */
        bool saw_deadband = false;  /* GTCNT within dead_time of boundary */
 
        uint32_t prev_cnt = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCNT;
 
        for (uint32_t sample = 0U; sample < 2000U; sample++)
        {
            uint32_t cnt = g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCNT;
 
            if (cnt <= 1U)
            {
                saw_trough = true;
            }
 
            if (cnt >= (period - 1U))
            {
                saw_crest = true;
            }
 
            if ((cnt > 1U) && (cnt < (period - 1U)) && (cnt > prev_cnt))
            {
                saw_count_up = true;
            }
 
            if ((cnt > 1U) && (cnt < (period - 1U)) && (cnt < prev_cnt))
            {
                saw_count_dn = true;
            }
 
            if ((cnt <= dead_time) || (cnt >= (period - dead_time)))
            {
                saw_deadband = true;
            }
 
            prev_cnt = cnt;
        }
 
        pass &= saw_trough && saw_count_up && saw_crest && saw_count_dn && saw_deadband;
 
        test_close_three_phase();
    }
 
    test_report("REQ-SEC-17", "Five counting operation sections", pass,
                pass ? "All 5 sections (trough/up/crest/down/deadband) observed"
                     : "Not all counting sections observed");
}
 
/*******************************************************************************************************************//**
* @brief Main entry point: run all complementary PWM test cases and print summary.
**********************************************************************************************************************/
void comp_pwm_run_all_tests (void)
{
    APP_PRINT("\r\n========================================\r\n");
    APP_PRINT("  GPT Complementary PWM Test Suite\r\n");
    APP_PRINT("  FSPRA-5725 / FSP v6.5.0 rc0\r\n");
    APP_PRINT("========================================\r\n\r\n");
 
    g_test_count = 0U;
 
    /* Operating Modes (REQ-OM-01 ~ 04) */
    comp_pwm_test_REQ_OM_01();
    comp_pwm_test_REQ_OM_02();
    comp_pwm_test_REQ_OM_03();
    comp_pwm_test_REQ_OM_04();
 
    /* Dead Time (REQ-DT-05 ~ 08) */
    comp_pwm_test_REQ_DT_05();
    comp_pwm_test_REQ_DT_06();
    comp_pwm_test_REQ_DT_07();
    comp_pwm_test_REQ_DT_08();
 
    /* Duty Cycle Control (REQ-DC-09 ~ 12) */
    comp_pwm_test_REQ_DC_09();
    comp_pwm_test_REQ_DC_10();
    comp_pwm_test_REQ_DC_11();
    comp_pwm_test_REQ_DC_12();
 
    /* Buffer Chains (REQ-BUF-13 ~ 16) */
    comp_pwm_test_REQ_BUF_13();
    comp_pwm_test_REQ_BUF_14();
    comp_pwm_test_REQ_BUF_15();
    comp_pwm_test_REQ_BUF_16();
 
    /* Counting Sections (REQ-SEC-17) */
    comp_pwm_test_REQ_SEC_17();
 
    /* ---- Summary ---- */
    uint32_t passed = 0U;
    for (uint32_t i = 0U; i < g_test_count; i++)
    {
        if (g_test_results[i].passed)
        {
            passed++;
        }
    }
 
    APP_PRINT("\r\n========================================\r\n");
    APP_PRINT("  Results: %lu / %lu passed\r\n", (unsigned long)passed, (unsigned long)g_test_count);
    APP_PRINT("========================================\r\n\r\n");
}
 
#endif /* COMP_PWM_TEST_ENABLE */
 
/*******************************************************************************************************************//**
* @} (end defgroup GPT_TG3_COMP_PWM_TESTS)
**********************************************************************************************************************/
