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
#include "r_gpt_test_data.h"
#include "../../!test/timer/r_timer_test_data.h"
#include "../../!test/timer/r_timer_test_port.h"
#include "r_ioport.h"
#if BSP_PERIPHERAL_ADC_PRESENT
 #include "r_adc.h"
#endif
#include "r_cgc.h"
#include "r_elc.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define TIMER_TEST_PRCR_KEY                        (0xA500U)
#define TIMER_TEST_PRCR_UNLOCK                     ((TIMER_TEST_PRCR_KEY) | 0x3U)
#define TIMER_TEST_PRCR_LOCK                       ((TIMER_TEST_PRCR_KEY) | 0x0U)
#define TIMER_TEST_UPDATE_GPTCKDIVCR(value)    {                       \
        volatile uint8_t * gptckdivcr = &R_SYSTEM->GPTCKDIVCR;         \
        volatile uint8_t * gptckcr    = &R_SYSTEM->GPTCKCR;            \
        *gptckcr |= (uint8_t) R_SYSTEM_GPTCKCR_GPTCKSREQ_Msk;          \
        FSP_HARDWARE_REGISTER_WAIT(R_SYSTEM->GPTCKCR_b.GPTCKSRDY, 1U); \
        *gptckdivcr = value;                                           \
        *gptckcr   &= (uint8_t) ~R_SYSTEM_GPTCKCR_GPTCKSREQ_Msk;       \
        FSP_HARDWARE_REGISTER_WAIT(R_SYSTEM->GPTCKCR_b.GPTCKSRDY, 0U); \
}

/* RA8 first stopped using the GPTE/H nomenclature, but current features overlap so RA8x1 has been tagged with as
 * having these 'feature-sets'. This should be re-evaluated if feature-sets are updated. */
#define TIMER_TEST_GPTE_GPTEH_SUPPORTED            ((BSP_FEATURE_GPT_GPTEH_SUPPORTED | \
                                                     BSP_FEATURE_GPT_GPTE_SUPPORTED))
#define TIMER_TEST_GPTE_GPTEH_CHANNEL_MASK         ((BSP_FEATURE_GPT_GPTEH_CHANNEL_MASK | \
                                                     BSP_FEATURE_GPT_GPTE_CHANNEL_MASK))
#define TIMER_TEST_INTERRUPT_SKIP_TODO             (BSP_MCU_GROUP_RA6T2 || BSP_MCU_GROUP_RA8D1 || BSP_MCU_GROUP_RA8P1 || \
                                                    BSP_MCU_GROUP_RA8M2)                        // Temporarily disable for these boards
#define TIMER_TEST_COMPLEMENTARY_MODE_TODO         (BSP_MCU_GROUP_RA6T2)                        // Temporarily disable for RA6T2

#define TIMER_TEST_ADC_TODO                        (BSP_MCU_GROUP_RA6T2 || BSP_MCU_GROUP_RA8D1 || BSP_MCU_GROUP_RA8P1 || \
                                                    BSP_MCU_GROUP_RA8M2 || BSP_MCU_GROUP_RA2T1) // Temporarily disable for these boards
#define TIMER_TEST_OUTPUT_ENABLE_TODO              (BSP_MCU_GROUP_RA4L1)
#define TIMER_TEST_ADC                             (TIMER_TEST_GPTE_GPTEH_SUPPORTED && !TIMER_TEST_ADC_TODO)
#define TIMER_TEST_ADC_DIRECT_START                (TIMER_TEST_ADC &&                   \
                                                    (1U << TIMER_TEST_OUTPUT_CHANNEL) & \
                                                    (BSP_FEATURE_GPT_AD_DIRECT_START_CHANNEL_MASK))
#define TIMER_TEST_ADC_WITH_ELC                    (TIMER_TEST_ADC &&                       \
                                                    ((1U << TIMER_TEST_OUTPUT_CHANNEL) &    \
                                                     TIMER_TEST_GPTE_GPTEH_CHANNEL_MASK) && \
                                                    !TIMER_TEST_ADC_DIRECT_START) // ELC is the 'standard' way of GPT-ADC trigger. RA8 first added 'direct start'.

/* Reduce it from 100 to 10 so GPT16E channel will not overflow */
#define GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS    (10U)

#define GPT_TEST_PERIOD_COUNTS                     (2000)

#define TIMER_TEST_NUM_CAPTURES                    (11) // 10 steps plus fence-post.

/* Offset GTIOCA and GTIOCB to avoid simultaneous interrupts */
#define TIMER_TEST_DEFAULT_DUTY                    (GPT_TEST_PERIOD_COUNTS / 2)
#define TIMER_TEST_PWM_DUTY_INCREMENT              (TIMER_TEST_DEFAULT_DUTY / (TIMER_TEST_NUM_CAPTURES - 1))

#define TIMER_TEST_PWM_DEAD_TIME_UP                (50) // Only used by POEG Test case
#define TIMER_TEST_PWM_DEAD_TIME_DOWN              (20) // Only used by POEG Test case
#define TIMER_TEST_PRCR_UNLOCK_CGC_LPM             (0xA503)

#define GPT_TEST_ADC_COMPARE_MATCH_VALUE           (123)
#define GPT_TEST_INTERRUPT_SKIP_COUNT_TIME_MS      (20)

/*===== Configure GTIOCA/B for one-shot pulse ====
 * - This will duplicate timings specified in usage-notes example. See usage notes for details. */

/* GTIOCA (configured for active-high) first and second pulse will be:
 * - 1/2 period, offset by 3/8 period
 * - 1/4 period, offset by 1/4 period */
#define TIMER_TEST_FIRST_EDGE_PIN_A                ((g_timer_test_ram_cfg.period_counts * 3) / 8)
#define TIMER_TEST_SECOND_EDGE_PIN_A               ((g_timer_test_ram_cfg.period_counts * 7) / 8)
#define TIMER_TEST_THIRD_EDGE_PIN_A                ((g_timer_test_ram_cfg.period_counts * 1) / 4)
#define TIMER_TEST_FOURTH_EDGE_PIN_A               ((g_timer_test_ram_cfg.period_counts * 2) / 4)

/* GTIOCB (configured for active-low) first and second pulse will be:
 * - 1/2 period, offset by 1/4 period
 * - 1/4 period, offset by 1/2 period */
#define TIMER_TEST_FIRST_EDGE_PIN_B                ((g_timer_test_ram_cfg.period_counts * 1) / 4)
#define TIMER_TEST_SECOND_EDGE_PIN_B               ((g_timer_test_ram_cfg.period_counts * 3) / 4)
#define TIMER_TEST_THIRD_EDGE_PIN_B                ((g_timer_test_ram_cfg.period_counts * 2) / 4)
#define TIMER_TEST_FOURTH_EDGE_PIN_B               ((g_timer_test_ram_cfg.period_counts * 3) / 4)

/* Debug delay loop is used after configuring and starting PWM.
 *  - It permits timer callback while halting the test to allow viewing of waveform on oscilloscope.
 *  - Some delay is required to allow PWM to stabilize, even if not debugging . */
volatile bool g_timer_test_halted_for_debug = true;
#define TIMER_TEST_PWM_WAIT_DEBUG(halt)    {g_timer_test_halted_for_debug = halt; do {R_BSP_SoftwareDelay(10,                            \
                                                                                                          BSP_DELAY_UNITS_MILLISECONDS); \
                                            } while (g_timer_test_halted_for_debug);}

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
extern uint32_t g_timer_test_sckdivcr_saved;

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
void r_gpt_tests_restore_clock_settings(void);
void one_shot_pulse_preload_timings(void);

static void timer_test_one_shot_pulse_callback(timer_callback_args_t * p_args);
static void gpt_test_pwm_duty_set_callback(timer_callback_args_t * p_args);

static uint32_t            g_timer_test_limit_cycles = 0;
static gpt_instance_ctrl_t g_gpt_ctrl;

static timer_cfg_t                  g_timer_test_capture_cfg;
static gpt_extended_cfg_t           g_timer_test_capture_cfg_extend;
static gpt_extended_cfg_t           g_timer_test_capture_gpt_cfg_extend;
static gpt_extended_pwm_cfg_t       g_gpt_test_pwm_ram_cfg;
static const gpt_extended_pwm_cfg_t g_gpt_test_pwm_default_cfg =
{
    .trough_ipl             = 3,
    .trough_irq             = FSP_INVALID_VECTOR,
    .poeg_link              = GPT_POEG_LINK_POEG0,
    .output_disable         = GPT_OUTPUT_DISABLE_NONE,
    .adc_trigger            = GPT_ADC_TRIGGER_NONE,
    .dead_time_count_up     = 0,
    .dead_time_count_down   = 0,
    .adc_a_compare_match    = 0,
    .adc_b_compare_match    = 0,
    .interrupt_skip_source  = GPT_INTERRUPT_SKIP_SOURCE_NONE,
    .interrupt_skip_count   = GPT_INTERRUPT_SKIP_COUNT_0,
    .interrupt_skip_adc     = GPT_INTERRUPT_SKIP_ADC_NONE,
    .gtioca_disable_setting = GPT_GTIOC_DISABLE_PROHIBITED,
    .gtiocb_disable_setting = GPT_GTIOC_DISABLE_PROHIBITED,
};

#if TIMER_TEST_ADC                     // ADC testing needs to be updated. See FSPRA-2964
 #if TIMER_TEST_ADC_WITH_ELC
static elc_instance_ctrl_t g_gpt_test_elc_ctrl;
static elc_cfg_t           g_gpt_test_elc_cfg;
 #endif

static volatile uint32_t g_gpt_test_adc_callbacks = 0;
static void gpt_test_adc_callback (adc_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
    g_gpt_test_adc_callbacks++;
}

static adc_instance_ctrl_t g_adc0_ctrl;

const adc_extended_cfg_t g_adc0_cfg_extend =
{
    .clearing          = ADC_CLEAR_AFTER_READ_ON,
    .add_average_count = ADC_ADD_OFF,
    .trigger           = ADC_START_SOURCE_ELC_AD0,  // ADC does not use 'config.trigger'
    .trigger_group_b   = ADC_START_SOURCE_DISABLED, // ADC does not use 'config.trigger'
};
const adc_cfg_t g_adc0_cfg =
{
    .unit = 0,
    .mode = ADC_MODE_SINGLE_SCAN,
 #if TEST_ADC16
    .resolution     = ADC_RESOLUTION_16_BIT,
 #else
    .resolution     = ADC_RESOLUTION_12_BIT,
 #endif
    .alignment      = ADC_ALIGNMENT_RIGHT,
    .trigger        = (adc_trigger_t) 0xF, // Not used
    .p_callback     = gpt_test_adc_callback,
    .p_context      = NULL,
    .scan_end_ipl   = 2,
    .scan_end_irq   = BSP_VECTOR_ADC_SCAN_END,
    .scan_end_b_ipl = 2,
    .scan_end_b_irq = FSP_INVALID_VECTOR,
    .p_extend       = &g_adc0_cfg_extend,
};
const adc_channel_cfg_t g_adc0_channel_cfg =
{
    .scan_mask          = ADC_MASK_CHANNEL_1,

    /* Group B channel mask is right shifted by 32 at the end to form the proper mask */
    .scan_mask_group_b  = 0U,
    .add_mask           = (uint32_t) (0),
    .sample_hold_mask   = (uint32_t) (0),
    .sample_hold_states = 24,
    .priority_group_a   = ADC_GROUP_A_PRIORITY_OFF,
};
#endif

/* Define test groups. */
TEST_GROUP(R_GPT_TG2);

/*******************************************************************************************************************//**
 * Test setup function called before every test in this test group.
 **********************************************************************************************************************/
TEST_SETUP(R_GPT_TG2)
{
#if 2U != GPT_CFG_OUTPUT_SUPPORT_ENABLE
    TEST_IGNORE_MESSAGE("This test is only run if triangle-wave PWM output is enabled.");
#endif

    /* Configure pins. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_OUTPUT_PIN_A,
                                      ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN |
                                       TIMER_TEST_OUTPUT_PIN_CFG)));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_OUTPUT_PIN_B,
                                      ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN |
                                       TIMER_TEST_OUTPUT_PIN_CFG)));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_INPUT_PIN_A,
                                      ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN |
                                       (uint32_t) IOPORT_PERIPHERAL_GPT1)));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_INPUT_PIN_B,
                                      ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN |
                                       (uint32_t) IOPORT_PERIPHERAL_GPT1)));

    /* Initialize the input capture variables */
    g_timer_test_limit_cycles      = UINT32_MAX;
    g_timer_test_capture_counter   = 0U;
    g_timer_test_capture_b_counter = 0U;
    g_timer_test_cycle_end_counter = 0U;
    for (uint8_t i = 0; i < sizeof(g_timer_test_capture_values) / sizeof(g_timer_test_capture_values[0]); i++)
    {
        g_timer_test_capture_values[i].counter   = UINT32_MAX;
        g_timer_test_capture_values[i].overflows = 0U;
    }

#if TIMER_TEST_ADC_WITH_ELC

    /* Initialize the ELC for the ADC test */
    if (0U == g_gpt_test_elc_ctrl.open)
    {
        R_ELC_Open(&g_gpt_test_elc_ctrl, &g_gpt_test_elc_cfg);
        R_ELC_Enable(&g_gpt_test_elc_ctrl);
        R_ELC_LinkSet(&g_gpt_test_elc_ctrl, ELC_PERIPHERAL_ADC0, GPT_TEST_ADCA_EVENT_LINK);
    }
#endif

    g_timer_test_sckdivcr_saved = R_SYSTEM->SCKDIVCR;

    /* Unlock CGC and LPM protection registers. */

#if BSP_FEATURE_TZ_VERSION == 2 && BSP_TZ_NONSECURE_BUILD == 1
    R_SYSTEM->PRCR_NS = (uint16_t) TIMER_TEST_PRCR_UNLOCK;
#else
    R_SYSTEM->PRCR = (uint16_t) TIMER_TEST_PRCR_UNLOCK;
#endif

    cgc_divider_cfg_t dividers;
    dividers.sckdivcr_b.iclk_div = (cgc_sys_clock_div_t) R_SYSTEM->SCKDIVCR_b.ICK;
#if (defined(BSP_MCU_GROUP_RA4L1) && TEST_GPT)
    dividers.sckdivcr_b.fclk_div = CGC_SYS_CLOCK_DIV_2;
#else
    dividers.sckdivcr_b.fclk_div = CGC_SYS_CLOCK_DIV_32;
#endif
    dividers.sckdivcr_b.pclka_div = CGC_SYS_CLOCK_DIV_32;
    dividers.sckdivcr_b.pclkb_div = CGC_SYS_CLOCK_DIV_32;
    dividers.sckdivcr_b.pclkc_div = CGC_SYS_CLOCK_DIV_32;
    dividers.sckdivcr_b.pclkd_div = CGC_SYS_CLOCK_DIV_32;
    dividers.sckdivcr_b.bclk_div  = CGC_SYS_CLOCK_DIV_32;
    R_SYSTEM->PRCR                = TIMER_TEST_PRCR_UNLOCK_CGC_LPM;
    R_SYSTEM->SCKDIVCR            = dividers.sckdivcr_w;

    /* Set the ICLK higher than the GPT clock so that the capture happens smoothly without impacting the code
     * execution. */
#if (BSP_PERIPHERAL_GPT_GTCLK_PRESENT && TEST_GPT && !GPT_CFG_GPTCLK_BYPASS)
    TEST_ASSERT_GREATER_OR_EQUAL(0, TIMER_TEST_GPT_CKDIVCR_DFLT);
    TIMER_TEST_UPDATE_GPTCKDIVCR(TIMER_TEST_GPT_CKDIVCR_DFLT)
#endif

    /* Reinitialize test configuration structure to defaults. */
    g_timer_test_ram_cfg                   = g_timer_test_default_cfg;
    g_timer_test_ram_cfg.source_div        = TIMER_TEST_CLOCK_DIV;
    g_timer_test_ram_cfg.period_counts     = GPT_TEST_PERIOD_COUNTS;
    g_timer_test_ram_cfg.duty_cycle_counts = TIMER_TEST_DEFAULT_DUTY;
    g_timer_test_ram_cfg_extend            = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend          = &g_timer_test_ram_cfg_extend;
    g_gpt_test_pwm_ram_cfg                 = g_gpt_test_pwm_default_cfg;
    g_timer_test_ram_cfg_extend.p_pwm_cfg  = &g_gpt_test_pwm_ram_cfg;

    /* Create the configuration for GPT input capture */
    g_timer_test_capture_cfg            = g_timer_test_capture_default_cfg;
    g_timer_test_capture_cfg.source_div = TIMER_TEST_CLOCK_DIV;
    g_timer_test_capture_cfg_extend     = g_gpt_test_capture_default_extend;
    g_timer_test_capture_cfg.p_extend   = &g_timer_test_capture_cfg_extend;
}

/*******************************************************************************************************************//**
 * Helper functions
 **********************************************************************************************************************/

static void timer_test_one_shot_pulse_callback (timer_callback_args_t * p_args)
{
    (void) p_args;

    if (TIMER_EVENT_CYCLE_END == p_args->event)
    {
        g_timer_test_cycle_end_counter++;
        if (g_timer_test_cycle_end_counter == g_timer_test_limit_cycles)
        {
            gp_timer_test_api->stop(&g_gpt_ctrl);
        }
    }
}

/* Callback to update compare match register for asymmetric PWM.
 *  Note: Difference between Symmetric and Asymmetric mode is in HW behavior.
 *   - Symmetric GTCCRn buffer transfer at Crest (only)
 *   - Asymmetric GTCCRn buffer transfer at Crest and Trough
 *   - No difference in interrupts, only buffer transfer
 *
 *  Set up symmetric mode to 'fail' if misconfigured
 *   - Set new duty-cycle every time, based on current GTCCRn. Symmetric mode should have 1/2 the
 *     updates in the same time period (customer should never read registers directly)
 *   - Count Crests/Troughs for later verification  */
static void gpt_test_pwm_duty_set_callback (timer_callback_args_t * p_args)
{
    static volatile uint32_t trough_duty = TIMER_TEST_DEFAULT_DUTY;

/* Asymmetric Pattern: (Crest/Trough transfer values set in opposite event)
 *  TROUGH_VAL_A.....   /\`   /\    /\`   /\    /\`   /\    /\`   /\
 *  TROUGH_VAL_B.....  /  \  /  \` /  \  /  \` /  \  /  \` /  \  /  \`
 *  CREST_VAL........ /`   \/`   \/`   \/`   \/`   \/`   \/`   \/`   \
 *                     __    ___   __    ___   __    ___   __    ___
 *  GTIOCA.......... _|  |__|   |_|  |__|   |_|  |__|   |_|  |__|   |_
 *                   _    __     _    __     _    __     _    __     _
 *  GTIOCB..........  |__|  |___| |__|  |___| |__|  |___| |__|  |___|
 *  ASYM  PERIOD      |     |     |     |     |     |     |     |
 */
    if (TIMER_EVENT_CREST == p_args->event)
    {
        /* Set data to load in trough */
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_GPT_DutyCycleSet(&g_gpt_ctrl, g_timer_test_ram_cfg.duty_cycle_counts, GPT_IO_PIN_GTIOCA));
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_GPT_DutyCycleSet(&g_gpt_ctrl, g_timer_test_ram_cfg.duty_cycle_counts, GPT_IO_PIN_GTIOCB));
    }

    if (TIMER_EVENT_TROUGH == p_args->event)
    {
        /* #=== Ignored by hardware in symmetric mode ===#
         * This duty cycle will load at the next crest - Always load same data (this ensures asymmetric PWM)
         *  - Period start is always period_counts
         *  - Period end is between (period_counts/NUM_CAPTURES) and period_counts
         *  - Total period:
         *     - Should average
         *     - between (TIMER_TEST_DEFAULT_DUTY+period_counts/NUM_CAPTURES) and (TIMER_TEST_DEFAULT_DUTY+period_counts)  */
        const uint32_t default_trough_duty = TIMER_TEST_DEFAULT_DUTY - TIMER_TEST_PWM_DUTY_INCREMENT; // Offset so we can achieve a period of 'duty_cycle_counts', since we can't set duty cycle equal to 'period_counts'
        uint32_t       increment           = TIMER_TEST_PWM_DUTY_INCREMENT;
        trough_duty += increment;
        trough_duty  = (trough_duty >= GPT_TEST_PERIOD_COUNTS) ? default_trough_duty : trough_duty;
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, trough_duty, GPT_IO_PIN_GTIOCA));
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, trough_duty, GPT_IO_PIN_GTIOCB));
    }
}

/*******************************************************************************************************************//**
 * @brief Test tear down function called after every test in this test group.
 **********************************************************************************************************************/
TEST_TEAR_DOWN(R_GPT_TG2)
{
    if (0U != g_gpt_ctrl.open)
    {
        R_GPT_Close(&g_gpt_ctrl);
    }

    if (0U != g_timer_test_capture_ctrl.open)
    {
        R_GPT_Close(&g_timer_test_capture_ctrl);
    }

#if TIMER_TEST_ADC_WITH_ELC
    if (0U != g_gpt_test_elc_ctrl.open)
    {
        R_ELC_Close(&g_gpt_test_elc_ctrl);
    }
#else                                  /* TODO: TIMER_TEST_ADC_DIRECT_START */
#endif

    /* Restore the stored CGC dividers. */
    R_SYSTEM->SCKDIVCR = g_timer_test_sckdivcr_saved;

#if BSP_FEATURE_TZ_VERSION == 2 && BSP_TZ_NONSECURE_BUILD == 1
    R_SYSTEM->PRCR_NS = (uint16_t) TIMER_TEST_PRCR_LOCK;
#else
    R_SYSTEM->PRCR = (uint16_t) TIMER_TEST_PRCR_LOCK;
#endif
}

/**
 * @req{gpt_symmetric_pwm,SWFLEX-592,R_GPT_Open} The GPT shall support generating symmetric PWM waveforms.
 *
 * @verify{gpt_symmetric_pwm} The high level waveforms of GTIOC pins match expected values for symmetric
 * PWM when they are measured with an input capture timer.
 *
 * Notes to the developer:
 *  - R_GPT configuration for symmetric happens at compile-time (or in R_GPT_Open), configuration for
 *    asymmetric happens at run-time (or in callback).
 *
 * Symmetric PWM: is symmetric with respect to the center of the period. Symmetric PWM (SPWM) uses a
 * control number n to turn a pulse on n counts before a datum time, and off n counts after. The resulting
 * pulse has a width of 2n, but is centred on the datum time, regardless of pulse width.
 *
 * Symmetric Example: [all PWM waveforms are symmetric about period center]:
 *     ___      _     _____     _      ___
 *    |   |    | |   |     |   | |    |   |
 * ___|   |____| |___|     |___| |____|   |___
 *      |       |       |       |       |
 *    Datum   Datum   Datum   Datum   Datum
 *
 * Asymmetric PWM always has a PWM edge aligned with the PWM period, but does not have equal trough
 * duration. Asymetric PWM generates a pulse of n counts wide, usually turning on at the datum time and
 * off n counts later. The centre of the pulse is located n/2 pulses after the datum time, so moves about
 * with respect to the datum as n changes. Other things being equal, as the pulse width can change by 1
 * count rather than 2 as in symmetric, it has twice the resolution.
 *
 *  Asymmetric:[all PWM waveforms have edges on period center]:
 *   ___       _   _____       _     ___
 *  |   |     | | |     |     | |   |   |
 * _|   |_____| |_|     |_____| |___|   |___
 *      |       |       |       |       |
 *    Datum   Datum   Datum   Datum   Datum
 *
 * Further Reading:
 *  - https://www.ti.com/lit/an/spra278/spra278.pdf
 *  - https://electronics.stackexchange.com/questions/383360/symmetric-vs-asymmetric-pwm
 */
TEST(R_GPT_TG2, TC_SymmetricPwm)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Intermediate variables for readability */
    uint32_t             capture_count        = TIMER_TEST_NUM_CAPTURES;
    timer_cfg_t        * p_timer_cfg          = &g_timer_test_capture_cfg;
    timer_cfg_t        * p_timer_ram_cfg      = &g_timer_test_ram_cfg;
    gpt_extended_cfg_t * p_timer_extend_cfg   = &g_timer_test_capture_cfg_extend;
    gpt_extended_cfg_t * p_timer_extend_cfg_b = &g_timer_test_capture_gpt_cfg_extend;
    const timer_api_t  * p_timer_api          = gp_timer_test_input_capture_api;

    const uint32_t gtioca_expected_max = (uint32_t) (TIMER_TEST_DEFAULT_DUTY * 2);
    const uint32_t gtioca_expected_min = (uint32_t) (TIMER_TEST_DEFAULT_DUTY * 2);
    const uint32_t gtioca_expected_avg = (uint32_t) (TIMER_TEST_DEFAULT_DUTY * 2);

    const uint32_t gtiocb_expected_max = (uint32_t) (p_timer_ram_cfg->period_counts - TIMER_TEST_DEFAULT_DUTY) * 2;
    const uint32_t gtiocb_expected_min = (uint32_t) (p_timer_ram_cfg->period_counts - TIMER_TEST_DEFAULT_DUTY) * 2;
    const uint32_t gtiocb_expected_avg = (uint32_t) (p_timer_ram_cfg->period_counts - TIMER_TEST_DEFAULT_DUTY) * 2;

    /* Create a configuration for GPT symmetric PWM. */
    p_timer_ram_cfg->mode          = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    p_timer_ram_cfg->p_callback    = NULL;
    p_timer_ram_cfg->cycle_end_irq = FSP_INVALID_VECTOR;
    p_timer_ram_cfg->p_callback    = NULL;
    p_timer_ram_cfg->cycle_end_irq = FSP_INVALID_VECTOR;

    uint32_t capture = 0U;

    /* Opening the GPT to generate PWM. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, p_timer_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    for (uint32_t i = 1; i < 3; i++)
    {
        /* Set duty cycle and test. */
        uint32_t offset = (i - 1) * (TIMER_TEST_DEFAULT_DUTY / 2); // Offset will double pulse-width in symmetric mode.
        uint32_t duty   = TIMER_TEST_DEFAULT_DUTY + offset;
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, duty, GPT_IO_PIN_GTIOCA));
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, duty, GPT_IO_PIN_GTIOCB));

        TIMER_TEST_PWM_WAIT_DEBUG(false);

        /* ======================================================================
         * ===== GTIOCA Overall Period (active high) - Inverted from GTIOCB =====
         * ======================================================================
         * Update capture configuration to measure the ON cycle pulse width of GTIOCB. */
        g_timer_test_capture_cfg.p_extend = &g_timer_test_capture_cfg_extend;

        /* Verify PWM waveform period */
        g_timer_test_capture_counter = 0;
        timer_test_capture_cfg(p_timer_extend_cfg, TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCA);
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts * 2, capture); // Increased from 1 to 2. See FSPRA-2975

        /* Verify PWM waveform pulse-width average. */
        timer_test_capture_cfg(p_timer_extend_cfg, TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH, GPT_IO_PIN_GTIOCA);
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, gtioca_expected_avg / i, capture); // Increased from 1 to 2. See FSPRA-2975

        /* Verify PWM waveform pulse-width max. */
        g_timer_test_capture_counter = 0;
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_MAX_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, gtioca_expected_max / i, capture); // Increased from 1 to 2. See FSPRA-2975

        /* Verify PWM waveform pulse-width min. */
        g_timer_test_capture_counter = 0;
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_MIN_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, gtioca_expected_min / i, capture); // Increased from 1 to 2. See FSPRA-2975

        /* ======================================================================
         * ===== GTIOCB Overall Period (active low) - Inverted from GTIOCA  =====
         * ======================================================================
         * Update capture configuration to measure the ON cycle pulse width of GTIOCB. */
        g_timer_test_capture_cfg               = g_timer_test_capture_default_cfg;
        g_timer_test_capture_gpt_cfg_extend    = g_gpt_test_capture_default_extend;
        g_timer_test_capture_cfg.p_extend      = &g_timer_test_capture_gpt_cfg_extend;
        g_timer_test_capture_cfg.cycle_end_irq = BSP_VECTOR_GPT_INPUT_OVERFLOW;

        /* Verify PWM waveform period */
        g_timer_test_capture_counter = 0;

        timer_test_capture_cfg(p_timer_extend_cfg_b, TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCB);
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts * 2, capture); // Increased from 1 to 2. See FSPRA-2975

        /* Verify GTIOCB Pulse Average */
        g_timer_test_capture_counter = 0;
        timer_test_capture_cfg(p_timer_extend_cfg_b, TIMER_TEST_CAPTURE_PULSE_WIDTH_LOW, GPT_IO_PIN_GTIOCB);
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, gtiocb_expected_avg / i, capture); // Increased from 1 to 2. See FSPRA-2975

        /* Verify GTIOCB Pulse Maximum */
        g_timer_test_capture_counter = 0;
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_MAX_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, gtiocb_expected_max / i, capture); // Increased from 1 to 2. See FSPRA-2975

        /* Verify GTIOCB Pulse Minimum */
        g_timer_test_capture_counter = 0;
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_MIN_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, gtiocb_expected_min / i, capture); // Increased from 1 to 2. See FSPRA-2975
    }
}

/**
 * @req{gpt_asymmetric_pwm,SWFLEX-592,R_GPT_Open} The GPT shall support generating asymmetric PWM waveforms.
 *
 * @verify{gpt_asymmetric_pwm} The high level waveforms of GTIOC pins match expected values for asymmetric
 * PWM when they are measured with an input capture timer.
 */
TEST(R_GPT_TG2, TC_AsymmetricPwm)
{
    r_gpt_tests_restore_clock_settings();                                                              // Set good clock values

    timer_cfg_t        * p_timer_cfg          = &g_timer_test_capture_cfg;                             // Readability hack, short name to fix uncrustify newlines
    gpt_extended_cfg_t * p_timer_extend_cfg   = &g_timer_test_capture_cfg_extend;                      // Readability hack, short name to fix uncrustify newlines
    gpt_extended_cfg_t * p_timer_extend_cfg_b = &g_timer_test_capture_gpt_cfg_extend;                  // Readability hack, short name to fix uncrustify newlines
    const timer_api_t  * p_timer_api          = gp_timer_test_input_capture_api;                       // Readability hack, short name to fix uncrustify newlines

    const uint32_t gtioca_expected_max = (uint32_t) GPT_TEST_PERIOD_COUNTS;
    const uint32_t gtioca_expected_min = (uint32_t) TIMER_TEST_DEFAULT_DUTY;
    const uint32_t gtioca_expected_avg = (uint32_t) ((gtioca_expected_max + gtioca_expected_min) / 2); // Each measurement is off by 1 (by design)

    const uint32_t gtiocb_expected_max = (uint32_t) GPT_TEST_PERIOD_COUNTS;
    const uint32_t gtiocb_expected_min = (uint32_t) TIMER_TEST_DEFAULT_DUTY;
    const uint32_t gtiocb_expected_avg = (uint32_t) ((gtiocb_expected_max + gtiocb_expected_min) / 2); // Each measurement is off by 1 (by design)

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode              = TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM;
    g_timer_test_ram_cfg.period_counts     = GPT_TEST_PERIOD_COUNTS;
    g_timer_test_ram_cfg.duty_cycle_counts = TIMER_TEST_DEFAULT_DUTY + TIMER_TEST_PWM_DUTY_INCREMENT; // Offset so we can achieve a period of 'duty_cycle_counts', since we can't set duty cycle equal to 'period_counts'
    g_timer_test_ram_cfg.p_callback        = gpt_test_pwm_duty_set_callback;
    g_timer_test_ram_cfg.cycle_end_irq     = BSP_VECTOR_TIMER_OVERFLOW;
    g_gpt_test_pwm_ram_cfg.trough_irq      = BSP_VECTOR_GPT_UNDERFLOW;

    uint32_t capture = 0U;

    /* Opening the GPT to generate PWM.
     * Wait for the waveform to update since we are using the buffer register to update the duty cycle in the interrupt. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
    TIMER_TEST_PWM_WAIT_DEBUG(false);

    /* ======================================================================
     * ===== GTIOCA Overall Period (active high) - Inverted from GTIOCB =====
     * ======================================================================
     * Open the GPT input capture to measure the cycle period of the PWM waveform */
    g_timer_test_capture_counter = 0;
    timer_test_capture_cfg(p_timer_extend_cfg, TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCA);
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_AVG_VERIFY);
    TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts * 2, capture); // Increased from 1 to 2. See FSPRA-2975

    /* GTIOCA Pulse average */
    g_timer_test_capture_counter = 0;
    timer_test_capture_cfg(p_timer_extend_cfg, TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH, GPT_IO_PIN_GTIOCA);
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_AVG);
    TEST_ASSERT_UINT_WITHIN(2, gtioca_expected_avg, capture); // Increased from 1 to 2. See FSPRA-2975

    /* Verify Pulse Maximum */
    g_timer_test_capture_counter = 0;
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_MAX);
    TEST_ASSERT_UINT_WITHIN(2, gtioca_expected_max, capture); // Increased from 1 to 2. See FSPRA-2975

    /* Verify Pulse Minimum */
    g_timer_test_capture_counter = 0;
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_MIN);
    TEST_ASSERT_UINT_WITHIN(2, gtioca_expected_min, capture); // Increased from 1 to 2. See FSPRA-2975

    /* ======================================================================
     * ===== GTIOCB Overall Period (active low) - Inverted from GTIOCA  =====
     * ======================================================================
     * Open the GPT input capture to measure the cycle period of the PWM waveform */
    g_timer_test_capture_cfg               = g_timer_test_capture_default_cfg;
    g_timer_test_capture_gpt_cfg_extend    = g_gpt_test_capture_default_extend;
    g_timer_test_capture_cfg.p_extend      = &g_timer_test_capture_gpt_cfg_extend;
    g_timer_test_capture_cfg.cycle_end_irq = BSP_VECTOR_GPT_INPUT_OVERFLOW;

    /* GTIOCB Pulse Average */
    g_timer_test_capture_counter = 0;
    timer_test_capture_cfg(p_timer_extend_cfg_b, TIMER_TEST_CAPTURE_PULSE_WIDTH_LOW, GPT_IO_PIN_GTIOCB);
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_AVG);
    TEST_ASSERT_UINT_WITHIN(2, gtiocb_expected_avg, capture); // Increased from 1 to 2. See FSPRA-2975

    /* GTIOCB Pulse Maximum */
    g_timer_test_capture_counter = 0;
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_MAX);
    TEST_ASSERT_UINT_WITHIN(TIMER_TEST_NUM_CAPTURES, gtiocb_expected_max, capture);

    /* GTIOCB Pulse Minimum */
    g_timer_test_capture_counter = 0;
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, TIMER_TEST_NUM_CAPTURES, TIMER_TEST_CAPTURE_MIN);
    TEST_ASSERT_UINT_WITHIN(TIMER_TEST_NUM_CAPTURES, gtiocb_expected_min, capture);
}

/* Preload one-shot pulse register values. See "Example setting for saw-wave one-shot pulse mode" for more information*/
void one_shot_pulse_preload_timings (void)
{
    fsp_err_t err;
    uint32_t  test_GTCCRA;
    uint32_t  test_GTCCRB;

    uint32_t test_GTCCRC;
    uint32_t test_GTCCRD;
    uint32_t test_GTCCRE;
    uint32_t test_GTCCRF;

    /*========== Load first four transition count values ==========*/

// *UNCRUSTIFY-OFF*
    /* Set the first edge. */
    err = R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_FIRST_EDGE_PIN_A, GPT_IO_PIN_GTIOCA | GPT_IO_PIN_ONE_SHOT_LEADING_EDGE);
    err |= R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_FIRST_EDGE_PIN_B, GPT_IO_PIN_GTIOCB | GPT_IO_PIN_ONE_SHOT_LEADING_EDGE);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);

    /* Set the second edge
     * Also and shift into GTCCRA/B and TMP-A/B (first two compare-match sets for one-shot pulse)*/
    err = R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_SECOND_EDGE_PIN_A, GPT_IO_PIN_GTIOCA | GPT_IO_PIN_ONE_SHOT_TRAILING_EDGE);
    err |= R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_SECOND_EDGE_PIN_B, GPT_IO_PIN_GTIOCB | GPT_IO_PIN_ONE_SHOT_TRAILING_EDGE | GPT_BUFFER_FORCE_PUSH);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
// *UNCRUSTIFY-ON*

    /* Validate registers before they've been shifted - NOTE: this works because shifting from upper-most registers is non-descructive*/
    test_GTCCRC = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRC];
    test_GTCCRE = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRE];
    test_GTCCRD = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRD];
    test_GTCCRF = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRF];
    TEST_ASSERT_EQUAL((TIMER_TEST_FIRST_EDGE_PIN_A - 1), test_GTCCRC);
    TEST_ASSERT_EQUAL((TIMER_TEST_FIRST_EDGE_PIN_B - 1), test_GTCCRE);
    TEST_ASSERT_EQUAL((TIMER_TEST_SECOND_EDGE_PIN_A - 1), test_GTCCRD);
    TEST_ASSERT_EQUAL((TIMER_TEST_SECOND_EDGE_PIN_B - 1), test_GTCCRF);

    /* Shift our register test values to reflect force-push that just occurred */
    test_GTCCRA = test_GTCCRC;
    test_GTCCRB = test_GTCCRE;

    /*========== Load second four transition count values ==========*/

// *UNCRUSTIFY-OFF*
    /* Set the third edge */
    err = R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_THIRD_EDGE_PIN_A, GPT_IO_PIN_GTIOCA | GPT_IO_PIN_ONE_SHOT_LEADING_EDGE);
    err |= R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_THIRD_EDGE_PIN_B, GPT_IO_PIN_GTIOCB | GPT_IO_PIN_ONE_SHOT_LEADING_EDGE);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);

    /* Set the fourth edge
     * Do not force-push as this would overwrite active register data that was just pushed */
    err = R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_FOURTH_EDGE_PIN_A, GPT_IO_PIN_GTIOCA | GPT_IO_PIN_ONE_SHOT_TRAILING_EDGE);
    err |= R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_FOURTH_EDGE_PIN_B, GPT_IO_PIN_GTIOCB | GPT_IO_PIN_ONE_SHOT_TRAILING_EDGE);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
// *UNCRUSTIFY-ON*

    /* Validate registers before they've been shifted */
    test_GTCCRC = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRC];
    test_GTCCRD = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRD];
    test_GTCCRE = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRE];
    test_GTCCRF = g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRF];
    TEST_ASSERT_EQUAL((TIMER_TEST_THIRD_EDGE_PIN_A - 1), test_GTCCRC);
    TEST_ASSERT_EQUAL((TIMER_TEST_THIRD_EDGE_PIN_B - 1), test_GTCCRE);
    TEST_ASSERT_EQUAL((TIMER_TEST_FOURTH_EDGE_PIN_A - 1), test_GTCCRD);
    TEST_ASSERT_EQUAL((TIMER_TEST_FOURTH_EDGE_PIN_B - 1), test_GTCCRF);

    /* Verify first edge of first and second pulse are different */
    TEST_ASSERT_NOT_EQUAL(test_GTCCRA, test_GTCCRC);
    TEST_ASSERT_NOT_EQUAL(test_GTCCRB, test_GTCCRE);
}

/**
 * @req{gpt_one_shot_pulse_mode,SWFLEX-3910,R_GPT_Open} The GPT shall support configuring One-Shot Pulse Mode.
 *
 * @verify{gpt_one_shot_pulse_mode} Two pulses are generated while interrupts disabled.
 */
TEST(R_GPT_TG2, TC_OneShotPulseOutput)
{
#if !TIMER_CFG_OUTPUT_SUPPORT_ENABLE
    TEST_IGNORE_MESSAGE("This test is only run if timer output is enabled.");
#endif

    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Create a configuration for One-Shot Pulse mode using output extension and register a callback function to use in
     * the test. */
    g_timer_test_ram_cfg               = g_timer_test_default_cfg;
    g_timer_test_ram_cfg.mode          = TIMER_MODE_ONE_SHOT_PULSE;
    g_timer_test_ram_cfg.p_callback    = timer_test_one_shot_pulse_callback;
    g_timer_test_ram_cfg.period_counts = GPT_TEST_PERIOD_COUNTS;

    /* Call timer_api_t::open. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, gp_timer_test_api->open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /* Verify initial output levels. */
    g_timer_test_capture_counter = 0;
    TEST_ASSERT_EQUAL(TIMER_TEST_OUTPUT_PIN_A_START_LEVEL, TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A));
#ifdef TIMER_TEST_OUTPUT_PIN_B
    TEST_ASSERT_EQUAL(TIMER_TEST_OUTPUT_PIN_B_START_LEVEL, TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));
#endif

    /*=============================================================*
     *========= Verify Buffers are Sequencing as Expected =========*
     *=============================================================*/
    one_shot_pulse_preload_timings();

    /* Call timer_api_t::start.  After this call, the output waveform will be visible on the scope. */
    g_timer_test_cycle_end_counter = 0;
    g_timer_test_limit_cycles      = 2;            // Three to ensure register cycling completes
    TEST_ASSERT_EQUAL(0, g_gpt_ctrl.p_reg->GTCNT); // Ensure counter was reset correctly.

    TEST_ASSERT_EQUAL(FSP_SUCCESS, gp_timer_test_api->start(&g_gpt_ctrl));

    /* Verify output levels have not immediately toggled. */
    TEST_ASSERT_EQUAL(TIMER_TEST_OUTPUT_PIN_A_START_LEVEL, TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A));
#ifdef TIMER_TEST_OUTPUT_PIN_B
    TEST_ASSERT_EQUAL(TIMER_TEST_OUTPUT_PIN_B_START_LEVEL, TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));
#endif

/* Note about why the remainder of this test is compiled out:
 *  - One-shot pulse output is working correctly, as verified manually on the scope.
 *  - The issue is that "GPT Capture" does not appear to be working for anything but symmetric, periodic signals.
 *  - It seems that GTTCR isn't valid for some time after the capture interrupt. This means that the capture value provided by the driver is always for the previous capture. Also that one-shot capture is not possible.
 *  - Fix-it ticket Jira: https://jira.eng.renesas.com/browse/FSPRA-2933 */
#if 0
    uint32_t expected = 0;

    /* Wait for the waveform to complete. */
    volatile uint32_t timeout = TIMER_TEST_WAVEFORM_TIMEOUT;
    while (((timeout--) > 0) && (0 != g_timer_test_limit_cycles))
    {
        // Do nothing
    }

    /* Verify buffers are sequencing, at second cycle end GTCCRC is pushd to GTCCRA and GTCCRE is pushed to GTCCRB. */
    TEST_ASSERT_EQUAL(g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRC], g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRA]);
    TEST_ASSERT_EQUAL(g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRE], g_gpt_ctrl.p_reg->GTCCR[TIMER_TEST_PRV_GTCCRB]);

    /*============================================================*
     *=========           Verify GTIOCA Timing           =========*
     *============================================================*/
    g_timer_test_capture_cfg          = g_timer_test_capture_default_cfg;
    g_timer_test_capture_cfg_extend   = g_gpt_test_capture_default_extend;
    g_timer_test_capture_cfg.p_extend = &g_timer_test_capture_cfg_extend;

    uint32_t capture = 0U;

    /* Verify GTIOCA period - Note this is not the actual period due to behavior of one-shot */
    g_timer_test_skip_first_measurement = false;
    g_timer_test_limit_cycles           = 10;
    g_timer_test_cycle_end_counter      = 0;
    one_shot_pulse_preload_timings();
    timer_test_capture_cfg(&g_timer_test_capture_cfg_extend, TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCA); // Note pulse-period measures based on rising-edges

    /* Pre-open capture interface to avoid delay that causes waveform to be missed */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, gp_timer_test_api->start(&g_gpt_ctrl));
    capture = r_timer_capture_output(&g_timer_test_capture_cfg,
                                     gp_timer_test_input_capture_api,
                                     g_timer_test_limit_cycles,
                                     TIMER_TEST_CAPTURE_AVG_NO_WAIT);

    expected = (g_timer_test_ram_cfg.period_counts - TIMER_TEST_FIRST_EDGE_PIN_A) + TIMER_TEST_THIRD_EDGE_PIN_A;
    TEST_ASSERT_UINT_WITHIN(2, expected, capture);         // Increased from 1 to 2. See FSPRA-2975
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS); // Wait for waveform to complete
    TEST_ASSERT_EQUAL(g_timer_test_cycle_end_counter, g_timer_test_limit_cycles);

    /* Verify GTIOCA min high-pulse */
    g_timer_test_skip_first_measurement = false;
    g_timer_test_limit_cycles           = 2;
    g_timer_test_cycle_end_counter      = 0;
    one_shot_pulse_preload_timings();
    timer_test_capture_cfg(&g_timer_test_capture_cfg_extend, TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH, GPT_IO_PIN_GTIOCA);

    /* Pre-open capture interface to avoid delay that causes waveform to be missed */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, gp_timer_test_api->start(&g_gpt_ctrl));
    capture = r_timer_capture_output(&g_timer_test_capture_cfg,
                                     gp_timer_test_input_capture_api,
                                     g_timer_test_limit_cycles,
                                     TIMER_TEST_CAPTURE_MIN_NO_WAIT);

    expected = TIMER_TEST_SECOND_EDGE_PIN_A - TIMER_TEST_FIRST_EDGE_PIN_A;
    TEST_ASSERT_UINT_WITHIN(2, expected, capture);         // Increased from 1 to 2. See FSPRA-2975
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS); // Wait for waveform to complete
    TEST_ASSERT_EQUAL(g_timer_test_cycle_end_counter, g_timer_test_limit_cycles);

    /* Verify GTIOCA max high-pulse */
    g_timer_test_skip_first_measurement = false;
    g_timer_test_limit_cycles           = 2;
    g_timer_test_cycle_end_counter      = 0;
    one_shot_pulse_preload_timings();
    timer_test_capture_cfg(&g_timer_test_capture_cfg_extend, TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH, GPT_IO_PIN_GTIOCA);

    /* Pre-open capture interface to avoid delay that causes waveform to be missed */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, gp_timer_test_api->start(&g_gpt_ctrl));
    capture = r_timer_capture_output(&g_timer_test_capture_cfg,
                                     gp_timer_test_input_capture_api,
                                     g_timer_test_limit_cycles,
                                     TIMER_TEST_CAPTURE_MAX_NO_WAIT);

    expected = TIMER_TEST_FOURTH_EDGE_PIN_A - TIMER_TEST_THIRD_EDGE_PIN_A;
    TEST_ASSERT_UINT_WITHIN(2, expected, capture);         // Increased from 1 to 2. See FSPRA-2975
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS); // Wait for waveform to complete
    TEST_ASSERT_EQUAL(g_timer_test_cycle_end_counter, g_timer_test_limit_cycles);

    /* Verify GTIOCB period - Note this is not the actual period due to behavior of one-shot */
    /* Verify GTIOCB min high-pulse */
    /* Verify GTIOCB max high-pulse */

    // uint32_t one_shot_test_on_cycle = ((g_timer_test_ram_cfg.period_counts / 2) - (g_timer_test_ram_cfg.period_counts / 4));
    // TEST_ASSERT_UINT_WITHIN(2, one_shot_test_on_cycle, capture);
//
    /// * Open the GPT input capture to measure the OFF cycle pulse width of the waveform */
    // timer_test_capture_cfg(&g_timer_test_capture_cfg_extend, TIMER_TEST_CAPTURE_PULSE_WIDTH_LOW, GPT_IO_PIN_GTIOCA);
//
    // uint32_t off_capture = r_timer_capture_output(&g_timer_test_capture_cfg,
    // gp_timer_test_input_capture_api,
    // TIMER_TEST_NUM_CAPTURES,
    // TIMER_TEST_CAPTURE_AVG_VERIFY | TIMER_TEST_CAPTURE_NO_STABILIZE);
    // uint32_t period_value            = g_timer_test_ram_cfg.period_counts;
    // uint32_t one_shot_test_off_cycle = period_value - one_shot_test_on_cycle;
    // TEST_ASSERT_UINT_WITHIN(2, one_shot_test_off_cycle, off_capture);
#endif
}

/**
 * @req{gpt_pwm_dead_time,SWFLEX-592,R_GPT_Open} The GPT shall support adding dead time in the PWM waveforms generated on GTIOCA and GTIOCB.
 *
 * @verify{gpt_pwm_dead_time} The high level and low level waveforms of GTIOC pins match expected values for asymmetric
 * PWM with dead time when they are measured with an input capture timer.
 *
 * Confirm dead time by:
 * - Reading GIOTCA period
 * - Reading GIOTCA pulse (active-high) time
 * - Reading GIOTCB period
 * - Reading GIOTCB pulse (active-low) time
 * - Compare
 *
 * GIOTCB pulse time should be GIOTCA + dead time.
 * Dead time is
 * - 2 * GTDVU for timer units that don't support GTDVD (for non-GPTE/H)
 * - GTDVU + GTDVD for timer units that do support GTDVD (for GPTE/H)
 * - N/A for timer units that don't support GTDVU or GTDVD (fail test in this case - all MCUs have at least one timer unit with support.)
 */
TEST(R_GPT_TG2, TC_DeadTime)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Validate Test Configuration */
#define DEAD_TIME_CHANNEL_MASK    (BSP_FEATURE_GPT_GTDVU_CHANNEL_MASK | TIMER_TEST_GPTE_GPTEH_CHANNEL_MASK)
    const char * error_message     = "Test configuration error; must select timer channel with dead-time support";
    uint32_t     test_channel_mask = (1 << g_timer_test_ram_cfg.channel);
    UNITY_TEST_ASSERT_BITS(DEAD_TIME_CHANNEL_MASK, test_channel_mask, test_channel_mask, __LINE__, error_message);

    /* Intermediate variables for readability */
    uint32_t             capture_count        = TIMER_TEST_NUM_CAPTURES;
    uint32_t             capture              = 0;
    uint32_t             expected             = 0;
    timer_cfg_t        * p_timer_cfg          = &g_timer_test_capture_cfg;
    gpt_extended_cfg_t * p_timer_extend_cfg   = &g_timer_test_capture_cfg_extend;
    gpt_extended_cfg_t * p_timer_extend_cfg_b = &g_timer_test_capture_gpt_cfg_extend;
    const timer_api_t  * p_timer_api          = gp_timer_test_input_capture_api;

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode                   = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.duty_cycle_counts      = TIMER_TEST_DEFAULT_DUTY;
    g_timer_test_ram_cfg.p_callback             = NULL;
    g_timer_test_ram_cfg.cycle_end_irq          = FSP_INVALID_VECTOR;
    g_gpt_test_pwm_ram_cfg.dead_time_count_up   = TIMER_TEST_PWM_DEAD_TIME_UP;
    g_gpt_test_pwm_ram_cfg.dead_time_count_down = TIMER_TEST_PWM_DEAD_TIME_DOWN;

    /* Opening the GPT to generate PWM. */
    (void) R_GPT_Close(&g_gpt_ctrl);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Verify GTIOCA period  */
    expected = g_timer_test_ram_cfg.period_counts * 2;
    timer_test_capture_cfg(p_timer_extend_cfg, TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCA);
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
    TEST_ASSERT_UINT_WITHIN(2, expected, capture); // Increased from 1 to 2. See FSPRA-2975

    /* Verify GTIOCA active (high) pulse time */
    expected = (g_timer_test_ram_cfg.period_counts - g_timer_test_ram_cfg.duty_cycle_counts) * 2;
    timer_test_capture_cfg(p_timer_extend_cfg, TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH, GPT_IO_PIN_GTIOCA);
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
    TEST_ASSERT_UINT_WITHIN(2, expected, capture); // Increased from 1 to 2. See FSPRA-2975

    /* Update global capture configuration for GTIOCB (active-low) */
    g_timer_test_capture_cfg               = g_timer_test_capture_default_cfg;
    g_timer_test_capture_gpt_cfg_extend    = g_gpt_test_capture_default_extend;
    g_timer_test_capture_cfg.p_extend      = &g_timer_test_capture_gpt_cfg_extend;
    g_timer_test_capture_cfg.cycle_end_irq = BSP_VECTOR_GPT_INPUT_OVERFLOW;

    /* Verify GTIOCB active (low) pulse time */
    expected = g_timer_test_ram_cfg.period_counts * 2;
    timer_test_capture_cfg(p_timer_extend_cfg_b, TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCB);
    capture = r_timer_capture_output(p_timer_cfg, &g_timer_on_gpt, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
    TEST_ASSERT_UINT_WITHIN(2, expected, capture); // Increased from 1 to 2. See FSPRA-2975

    /* Verify GTIOCB active (low) pulse time */
    bool     gtdvd_present        = (TIMER_TEST_GPTE_GPTEH_CHANNEL_MASK & test_channel_mask);
    uint32_t dead_time_count_down = gtdvd_present ? TIMER_TEST_PWM_DEAD_TIME_DOWN : TIMER_TEST_PWM_DEAD_TIME_UP;
    expected = g_timer_test_ram_cfg.duty_cycle_counts * 2 - (TIMER_TEST_PWM_DEAD_TIME_UP + dead_time_count_down);
    timer_test_capture_cfg(p_timer_extend_cfg_b, TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH, GPT_IO_PIN_GTIOCB);
    capture = r_timer_capture_output(p_timer_cfg, &g_timer_on_gpt, capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
    TEST_ASSERT_UINT_WITHIN(2, expected, capture); // Increased from 1 to 2. See FSPRA-2975

#undef DEAD_TIME_CHANNEL_MASK
}

#if ((TIMER_TEST_GPTE_GPTEH_SUPPORTED && !TIMER_TEST_INTERRUPT_SKIP_TODO) || TIMER_TEST_ADC)

static volatile uint32_t g_gpt_test_crest_count  = 0U;
static volatile uint32_t g_gpt_test_trough_count = 0U;

static volatile bool g_int_skip_test_end = false;

/* Callback count interrupts. */
static void gpt_test_interrupt_skip_callback (timer_callback_args_t * p_args)
{
    if (TIMER_EVENT_CREST == p_args->event)
    {
        g_gpt_test_crest_count++;
    }

    if (TIMER_EVENT_TROUGH == p_args->event)
    {
        g_gpt_test_trough_count++;
    }
}

/* Interrupt skip test one-shot interrupt */
static void gpt_test_interrupt_skip_end_callback (timer_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);

    g_gpt_ctrl.p_reg->GTSTP = (1U << TIMER_TEST_OUTPUT_CHANNEL) | (1U << TIMER_TEST_UNUSED_CHANNEL);

    g_int_skip_test_end = true;
}

static timer_cfg_t g_timer_test_is_cfg =
{
    .channel           = TIMER_TEST_UNUSED_CHANNEL,
    .duty_cycle_counts = 0,
    .source_div        = TIMER_TEST_CLOCK_DIV,
    .mode              = TIMER_MODE_ONE_SHOT,
    .p_callback        = gpt_test_interrupt_skip_end_callback,
    .p_context         = 0,
    .p_extend          = &TIMER_TEST_CFG_EXTEND,
    .cycle_end_ipl     = 2,
    .cycle_end_irq     = BSP_VECTOR_UNUSED_TIMER_OVERFLOW,
};
static gpt_instance_ctrl_t g_gpt_is_ctrl;

/* Subroutine to test interrupt skipping. */
static void gpt_test_interrupt_skip_sub (gpt_interrupt_skip_source_t setting, gpt_interrupt_skip_count_t count)
{
    g_gpt_test_crest_count  = 0U;
    g_gpt_test_trough_count = 0U;

    g_gpt_test_pwm_ram_cfg.interrupt_skip_source = setting;
    g_gpt_test_pwm_ram_cfg.interrupt_skip_count  = count;

    /* Opening the GPT to generate PWM. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /* Open one-shot timer to stop the test after two periods */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_is_ctrl, &g_timer_test_is_cfg));

    /* Start both timers synchronously */
    g_gpt_ctrl.p_reg->GTSTR = (1U << TIMER_TEST_OUTPUT_CHANNEL) | (1U << TIMER_TEST_UNUSED_CHANNEL);

    /* Wait for one-shot timer interrupt (will also stop timers) */
    g_int_skip_test_end = false;
    while (!g_int_skip_test_end)
    {
        ;
    }

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_gpt_is_ctrl));

    if (!setting || !count)
    {
        /* No skipping */
        TEST_ASSERT_UINT_WITHIN(1U, GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS, g_gpt_test_crest_count);
        TEST_ASSERT_UINT_WITHIN(1U, GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS, g_gpt_test_trough_count);
    }
    else
    {
        /* Confirm that the specified number of interrupts were skipped */
        TEST_ASSERT_UINT_WITHIN(1U,
                                GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS / ((uint32_t) count + 1U),
                                g_gpt_test_crest_count);
        TEST_ASSERT_UINT_WITHIN(1U,
                                GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS / ((uint32_t) count + 1U),
                                g_gpt_test_trough_count);
    }
}

#endif

/**
 * @req{gpt_interrupt_skip,SWFLEX-592,R_GPT_Open} The GPT shall support skipping crest and/or trough interrupts.
 *
 * @verify{gpt_interrupt_skip} GPT interrupt count values match expected values when skipping is applied to crest,
 * trough, and both crest and trough.
 */
TEST(R_GPT_TG2, TC_InterruptSkip)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

#if TIMER_TEST_GPTE_GPTEH_SUPPORTED && !TIMER_TEST_INTERRUPT_SKIP_TODO

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode              = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.period_counts     = GPT_TEST_PERIOD_COUNTS / 5U;
    g_timer_test_ram_cfg.duty_cycle_counts = GPT_TEST_PERIOD_COUNTS / 10U;
    g_timer_test_ram_cfg.p_callback        = gpt_test_interrupt_skip_callback;
    g_timer_test_ram_cfg.cycle_end_irq     = BSP_VECTOR_TIMER_OVERFLOW;
    g_gpt_test_pwm_ram_cfg.trough_irq      = BSP_VECTOR_GPT_UNDERFLOW;

    /* Configure the one-shot timer to stop the GPT after 100 triangle wave periods of (GPT_TEST_PERIOD_COUNTS / 5) */
    g_timer_test_is_cfg.period_counts = GPT_TEST_PERIOD_COUNTS * GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS / 5U * 2U;

    /* Run all combinations of skip source and count */
    for (gpt_interrupt_skip_source_t source = GPT_INTERRUPT_SKIP_SOURCE_NONE;
         source <= GPT_INTERRUPT_SKIP_SOURCE_TROUGH;
         source++)
    {
        for (gpt_interrupt_skip_count_t count = GPT_INTERRUPT_SKIP_COUNT_0;
             count <= GPT_INTERRUPT_SKIP_COUNT_7;
             count++)
        {
            gpt_test_interrupt_skip_sub(source, count);
        }
    }

#else
    TEST_IGNORE_MESSAGE("This channel does not support interrupt skipping.");
#endif
}

/**
 * @req{gpt_adc_trigger,SWFLEX-592,R_GPT_Open} The GPT shall support triggering an ADC scan.
 *
 * @verify{gpt_adc_trigger} The ADC scan end interrupt count matches the expected value for triggering an ADC
 * interrupt with and without event (interrupt) skipping enabled.
 */
TEST(R_GPT_TG2, TC_AdcTrigger)
{
#if TIMER_TEST_ADC_TODO
    TEST_IGNORE_MESSAGE("This test is temporarily ignored for RA6T2, RA8D1, RA8M2, and RA8P1 pending further testing.");
    TEST_IGNORE_MESSAGE(
        "Tests with ADC dependency currently skipped for RA6T2, RA8D1, RA8M2, and RA8P1. These will be added in future.");
#elif TIMER_TEST_ADC
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode                  = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.period_counts         = TIMER_TEST_PERIOD_COUNTS / 2U;
    g_timer_test_ram_cfg.duty_cycle_counts     = TIMER_TEST_PERIOD_COUNTS / 8U;
    g_timer_test_ram_cfg.p_callback            = gpt_test_interrupt_skip_callback;
    g_timer_test_ram_cfg.cycle_end_irq         = BSP_VECTOR_TIMER_OVERFLOW;
    g_gpt_test_pwm_ram_cfg.trough_irq          = BSP_VECTOR_GPT_UNDERFLOW;
    g_gpt_test_pwm_ram_cfg.adc_a_compare_match = GPT_TEST_ADC_COMPARE_MATCH_VALUE;
    g_gpt_test_pwm_ram_cfg.adc_trigger         = GPT_ADC_TRIGGER_UP_COUNT_START_ADC_A;
    g_timer_test_is_cfg.period_counts          = TIMER_TEST_PERIOD_COUNTS * GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS;

    /* Configure the ADC to scan channel 0.  It doesn't matter what's connected for this test - this test is just checking for scan complete interrupts. */
    TEST_ASSERT_EQUAL_UINT32(FSP_SUCCESS, R_ADC_Open(&g_adc0_ctrl, &g_adc0_cfg));
    TEST_ASSERT_EQUAL_UINT32(FSP_SUCCESS, R_ADC_ScanCfg(&g_adc0_ctrl, &g_adc0_channel_cfg));
    TEST_ASSERT_EQUAL_UINT32(FSP_SUCCESS, R_ADC_ScanStart(&g_adc0_ctrl));

    /* Trigger the ADC on up counting. Skip ADC triggers except before each crest interrupt. */
    g_gpt_test_adc_callbacks = 0;
    gpt_test_interrupt_skip_sub(GPT_INTERRUPT_SKIP_SOURCE_NONE, GPT_INTERRUPT_SKIP_COUNT_0);

    /* Verify ADC callback count matches crest callback count (100). */
    uint32_t adc_callbacks   = g_gpt_test_adc_callbacks;
    uint32_t crest_callbacks = g_gpt_test_crest_count;
    TEST_ASSERT_UINT_WITHIN(1U, crest_callbacks, GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS);
    TEST_ASSERT_UINT_WITHIN(1U, crest_callbacks, adc_callbacks);

    /* Trigger the ADC on up counting. Skip ADC triggers except before each crest interrupt. */
    g_gpt_test_pwm_ram_cfg.interrupt_skip_adc = GPT_INTERRUPT_SKIP_ADC_A;
    g_gpt_test_adc_callbacks = 0;
    gpt_test_interrupt_skip_sub(GPT_INTERRUPT_SKIP_SOURCE_CREST, GPT_INTERRUPT_SKIP_COUNT_4);

    /* Verify ADC callback count matches crest callback count (100 / 5 == 20). */
    adc_callbacks   = g_gpt_test_adc_callbacks;
    crest_callbacks = g_gpt_test_crest_count;
    TEST_ASSERT_UINT_WITHIN(1U, crest_callbacks, GPT_TEST_NUM_INTERRUPT_SKIPPING_PERIODS / 5U);
    TEST_ASSERT_UINT_WITHIN(1U, crest_callbacks, adc_callbacks);
#else
    TEST_IGNORE_MESSAGE("This channel does not support ADC start requests.");
#endif
}

/**
 * @req{gpt_adc_trigger_update,SWFLEX-592,R_GPT_AdcTriggerSet} The GPT shall support updating the compare match value that triggers an ADC scan.
 *
 * @verify{gpt_adc_trigger_update} The ADC compare match register for events A and B match the expected value after updating them with the API.
 */
TEST(R_GPT_TG2, TC_AdcTriggerUpdate)
{
#if TIMER_TEST_ADC_TODO
    TEST_IGNORE_MESSAGE(
        "This test is temporarily ignored for RA6T2, RA8D1, RA8M2, and RA8P1 pending further testing. \n \
                         Tests with ADC dependency currently skipped for RA6T2, RA8D1, RA8M2, and RA8P1. These will be added in future."                        );
#elif TIMER_TEST_ADC
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode                  = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.duty_cycle_counts     = TIMER_TEST_DEFAULT_DUTY;
    g_timer_test_ram_cfg.cycle_end_irq         = BSP_VECTOR_TIMER_OVERFLOW;
    g_gpt_test_pwm_ram_cfg.adc_a_compare_match = GPT_TEST_ADC_COMPARE_MATCH_VALUE;
    g_gpt_test_pwm_ram_cfg.adc_b_compare_match = 3U;
    g_gpt_test_pwm_ram_cfg.adc_trigger         = GPT_ADC_TRIGGER_UP_COUNT_START_ADC_A;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /* Verify initial values. */
    TEST_ASSERT_EQUAL(GPT_TEST_ADC_COMPARE_MATCH_VALUE, g_gpt_ctrl.p_reg->GTADTRA);
    TEST_ASSERT_EQUAL(3U, g_gpt_ctrl.p_reg->GTADTRB);

    /* Update the ADC trigger values. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_AdcTriggerSet(&g_gpt_ctrl, GPT_ADC_COMPARE_MATCH_ADC_A, 5U));
    TEST_ASSERT_EQUAL(5U, g_gpt_ctrl.p_reg->GTADTRA);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_AdcTriggerSet(&g_gpt_ctrl, GPT_ADC_COMPARE_MATCH_ADC_B, 10U));
    TEST_ASSERT_EQUAL(10U, g_gpt_ctrl.p_reg->GTADTRB);
#else
    TEST_IGNORE_MESSAGE("This channel does not support ADC start requests.");
#endif
}

/**
 * @req{gpt_poeg_link,SWFLEX-592,R_GPT_Open} The GPT shall configuring output disable links to be used with POEG.
 *
 * @verify{gpt_poeg_link} GTIOC pins match their configured disabled level after output is disabled by POEG due to dead time error.
 */
TEST(R_GPT_TG2, TC_Poeg)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode              = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.duty_cycle_counts = TIMER_TEST_DEFAULT_DUTY;
    g_timer_test_ram_cfg.p_callback        = NULL;
    g_timer_test_ram_cfg.cycle_end_irq     = FSP_INVALID_VECTOR;
#if TIMER_TEST_GPTE_GPTEH_SUPPORTED
    g_gpt_test_pwm_ram_cfg.dead_time_count_up   = TIMER_TEST_PWM_DEAD_TIME_UP;
    g_gpt_test_pwm_ram_cfg.dead_time_count_down = TIMER_TEST_PWM_DEAD_TIME_DOWN;
    g_gpt_test_pwm_ram_cfg.output_disable       = GPT_OUTPUT_DISABLE_DEAD_TIME_ERROR;
#else
    g_gpt_test_pwm_ram_cfg.output_disable =
        (gpt_output_disable_t) ((uint32_t) GPT_OUTPUT_DISABLE_GTIOCA_GTIOCB_HIGH |
                                (uint32_t) GPT_OUTPUT_DISABLE_GTIOCA_GTIOCB_LOW);
#endif
    g_gpt_test_pwm_ram_cfg.poeg_link              = GPT_POEG_LINK_POEG1;
    g_gpt_test_pwm_ram_cfg.gtioca_disable_setting = GPT_GTIOC_DISABLE_LEVEL_LOW;
    g_gpt_test_pwm_ram_cfg.gtiocb_disable_setting = GPT_GTIOC_DISABLE_LEVEL_HIGH;

    /* Enable POEG1. */
    R_BSP_MODULE_START(FSP_IP_POEG, 1U);
    R_GPT_POEG1->POEGG = R_GPT_POEG0_POEGG_IOCE_Msk;

    /* Opening the GPT to generate PWM. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Capture outputs. */
    g_timer_test_capture_counter = 0;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_timer_test_capture_ctrl, &g_timer_test_capture_default_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_timer_test_capture_ctrl));
    r_timer_test_wait_for_captures(TIMER_TEST_NUM_CAPTURES, TIMER_TEST_WAVEFORM_TIMEOUT);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_timer_test_capture_ctrl));

    /* Output is enabled. */
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);

#if TIMER_TEST_GPTE_GPTEH_SUPPORTED

    /* Create a dead time error by setting the duty cycle less than the dead time. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_PWM_DEAD_TIME_UP - 5, GPT_IO_PIN_GTIOCA));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_PWM_DEAD_TIME_UP - 5, GPT_IO_PIN_GTIOCB));
#else
 #if GPT_CFG_WRITE_PROTECT_ENABLE
  #define GPT_PRV_GTWP_RESET_VALUE    (0xA500)
    g_gpt_ctrl.p_reg->GTWP = GPT_PRV_GTWP_RESET_VALUE;
 #endif

    /* Change configuration for GTIOCA to match GTIOCB so they output in phase instead of out of phase. */
    uint32_t gtiob = g_gpt_ctrl.p_reg->GTIOR_b.GTIOB;
    uint32_t gtior = g_gpt_ctrl.p_reg->GTIOR;
    g_gpt_ctrl.p_reg->GTIOR_b.GTIOA = gtiob & R_GPT0_GTIOR_GTIOA_Msk;

 #if GPT_CFG_WRITE_PROTECT_ENABLE
  #define GPT_PRV_GTWP_WRITE_PROTECT    (0xA501)
    g_gpt_ctrl.p_reg->GTWP = GPT_PRV_GTWP_WRITE_PROTECT;
 #endif

    /* GTIOR changes take effect after restarting. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Stop(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Reset(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
#endif

    /* Capture outputs. */
    g_timer_test_capture_counter = 0;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_timer_test_capture_ctrl, &g_timer_test_capture_default_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_timer_test_capture_ctrl));
    r_timer_test_wait_for_captures(TIMER_TEST_NUM_CAPTURES, TIMER_TEST_WAVEFORM_TIMEOUT);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_timer_test_capture_ctrl));

    /* Output is disabled. */
    TEST_ASSERT_EQUAL(1U, g_gpt_ctrl.p_reg->GTST_b.ODF);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);

#if TIMER_TEST_GPTE_GPTEH_SUPPORTED

    /* Restore duty cycle. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_DEFAULT_DUTY, GPT_IO_PIN_GTIOCA));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, TIMER_TEST_DEFAULT_DUTY, GPT_IO_PIN_GTIOCB));

    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

    R_GPT_POEG1->POEGG = 0U;
#else

    /* Stop timer to recover GTIOR. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Stop(&g_gpt_ctrl));

 #if GPT_CFG_WRITE_PROTECT_ENABLE
  #define GPT_PRV_GTWP_RESET_VALUE    (0xA500)
    g_gpt_ctrl.p_reg->GTWP = GPT_PRV_GTWP_RESET_VALUE;
 #endif

    /* Restore GTIOA polarity and disable settings. */
    g_gpt_ctrl.p_reg->GTIOR = gtior;

 #if GPT_CFG_WRITE_PROTECT_ENABLE
  #define GPT_PRV_GTWP_WRITE_PROTECT    (0xA501)
    g_gpt_ctrl.p_reg->GTWP = GPT_PRV_GTWP_WRITE_PROTECT;
 #endif

    R_GPT_POEG1->POEGG = 0U;

    /* Changes take effect after restarting. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Reset(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
#endif

    /* Capture outputs. */
    g_timer_test_capture_counter = 0;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_timer_test_capture_ctrl, &g_timer_test_capture_default_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_timer_test_capture_ctrl));
    r_timer_test_wait_for_captures(TIMER_TEST_NUM_CAPTURES, TIMER_TEST_WAVEFORM_TIMEOUT);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_timer_test_capture_ctrl));

    /* Output is enabled. */
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);
}

static void gpt_test_capture_period (gpt_io_pin_t pin)
{
    g_timer_test_capture_cfg            = g_timer_test_capture_default_cfg;
    g_timer_test_capture_gpt_cfg_extend = g_gpt_test_capture_default_extend;
    g_timer_test_capture_cfg.p_extend   = &g_timer_test_capture_gpt_cfg_extend;
    timer_test_capture_cfg(&g_timer_test_capture_gpt_cfg_extend, TIMER_TEST_CAPTURE_PULSE_PERIOD, pin);

    /* Capture outputs. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_timer_test_capture_ctrl, &g_timer_test_capture_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_timer_test_capture_ctrl));
    g_timer_test_capture_counter = 0;
    r_timer_test_wait_for_captures(TIMER_TEST_NUM_CAPTURES, TIMER_TEST_WAVEFORM_TIMEOUT);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Disable(&g_timer_test_capture_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_timer_test_capture_ctrl));
}

/**
 * @req{gpt_output_disable,SWFLEX-592,R_GPT_OutputDisable} The GPT shall support disabling output pins.
 *
 * @verify{gpt_output_disable} Input capture loopback detects a waveform on both GTIOCA and GTIOCB after open.
 * After disabling GTIOCA, input capture loopback detects a waveform on GTIOCB, but no captures on GTIOCA.
 * After disabling GTIOCB, no captures occur on either GTIOCA or GTIOCB.
 */
TEST(R_GPT_TG2, TC_OutputDisable)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode              = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.duty_cycle_counts = TIMER_TEST_DEFAULT_DUTY;
    g_timer_test_ram_cfg.p_callback        = NULL;
    g_timer_test_ram_cfg.cycle_end_irq     = FSP_INVALID_VECTOR;

    /* Opening the GPT to generate PWM. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Expect captures on both GTIOCA and GTIOCB. */
    gpt_test_capture_period(GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);
    gpt_test_capture_period(GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);

    /* Disable output on GTIOCA. Expect captures on GTIOCB, but not GTIOCA. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputDisable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA));
    gpt_test_capture_period(GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);
    gpt_test_capture_period(GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);

    /* Disable output on GTIOCB. Expect no captures on GTIOCA or GTIOCB. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputDisable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCB));
    gpt_test_capture_period(GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);
    gpt_test_capture_period(GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);
}

/**
 * @req{gpt_output_enable,SWFLEX-592,R_GPT_OutputEnable} The GPT shall support enabling output pins.
 *
 * @verify{gpt_output_enable} Start with output disabled.
 * After enabling GTIOCA, input capture loopback detects a waveform on GTIOCA, but no captures on GTIOCB.
 * After enabling GTIOCB, captures occur on both GTIOCA and GTIOCB.
 */
TEST(R_GPT_TG2, TC_OutputEnable)
{
#if TIMER_TEST_OUTPUT_ENABLE_TODO
    TEST_IGNORE_MESSAGE("This test is temporarily ignored for R44L1 pending further investigation. See FSPRA-3049");
#endif
    r_gpt_tests_restore_clock_settings();                                // Set good clock values

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode              = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.period_counts     = GPT_TEST_PERIOD_COUNTS / 2; // Test will not pass with longer period. Presumably due to floating input and/or cross-talk.
    g_timer_test_ram_cfg.duty_cycle_counts = GPT_TEST_PERIOD_COUNTS / 4;
    g_timer_test_ram_cfg.p_callback        = NULL;
    g_timer_test_ram_cfg.cycle_end_irq     = FSP_INVALID_VECTOR;
    g_timer_test_ram_cfg.source_div        = TIMER_SOURCE_DIV_1;

    /* Opening the GPT to generate PWM. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Disable output on GTIOCA and GTIOCB. Expect no captures on GTIOCA or GTIOCB. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputDisable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA_AND_GTIOCB));
    gpt_test_capture_period(GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);
    gpt_test_capture_period(GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);

    /* Enable output on GTIOCA. Expect captures on GTIOCA, but not GTIOCB. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputEnable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA));
    gpt_test_capture_period(GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);
    gpt_test_capture_period(GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(0U, g_timer_test_capture_counter);

    /* Enable output on GTIOCB. Expect captures on both GTIOCA and GTIOCB. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputEnable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCB));
    gpt_test_capture_period(GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);
    gpt_test_capture_period(GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(TIMER_TEST_NUM_CAPTURES, g_timer_test_capture_counter);
}

/**
 * @req{gpt_counter_set,SWFLEX-592,R_GPT_CounterSet} The GPT shall support updating the counter value.
 *
 * @verify{gpt_counter_set} The counter register value matches the expected value after updating it.
 */
TEST(R_GPT_TG2, TC_CounterSet)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /* Create a configuration for GPT symmetric PWM. */
    g_timer_test_ram_cfg.mode = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /* Verify initial countervalue is 0. */
    TEST_ASSERT_EQUAL(0U, g_gpt_ctrl.p_reg->GTCNT);

    /* Update the ADC trigger values. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_CounterSet(&g_gpt_ctrl, 5U));
    TEST_ASSERT_EQUAL(5U, g_gpt_ctrl.p_reg->GTCNT);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_CounterSet(&g_gpt_ctrl, 10U));
    TEST_ASSERT_EQUAL(10U, g_gpt_ctrl.p_reg->GTCNT);
}

/**
 * @req{gpt_custom_waveform,SWFLEX-2652,R_GPT_Open} The GPT shall support configuring custom GTIOR settings.
 *
 * @verify{gpt_custom_waveform} After configuring the GPT to use a custom GTIOR setting, the expected
 * waveform will be generated for GTIOA and GTIOB.
 *
 *  Configured Waveform:
 *          ___   ___   ___   ___   ___   ___
 * GTIOCA -    |_|   |_|   |_|   |_|   |_|   |_
 *            ___   ___   ___   ___   ___   ___
 * GTIOCB - _|   |_|   |_|   |_|   |_|   |_|   |
 * Period -      |     |     |     |     |     |
 */
TEST(R_GPT_TG2, TC_CustomWaveform)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    g_timer_test_ram_cfg.mode              = TIMER_MODE_PWM;
    g_timer_test_ram_cfg.duty_cycle_counts = 0;
    g_timer_test_ram_cfg.p_callback        = NULL;
    g_timer_test_ram_cfg.cycle_end_irq     = FSP_INVALID_VECTOR;

    /** Set custom waveform.
     *  - The GTIOA PWM pulse will start at the beginning of the timer period.
     *  - The GTIOB PWM pulse will end at the end of the timer period.
     */
    g_timer_test_ram_cfg_extend.gtior_setting.gtior_b.gtioa = (1U << 4U) | // Initial value high
                                                              (3U << 2U) | // Toggle at cycle end
                                                              (3U << 0U);  // Toggle at compare match
    g_timer_test_ram_cfg_extend.gtior_setting.gtior_b.gtiob = (0U << 4U) | // Initial value Low
                                                              (3U << 2U) | // Toggle at cycle end
                                                              (3U << 0U);  // Toggle at compare match

    /** Configure the PWM timer to use the custom waveform. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /** Set the duty cycle for GTIOCA so that the compare match occurs at the last quarter of the timer period. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, (GPT_TEST_PERIOD_COUNTS) -(GPT_TEST_PERIOD_COUNTS / 4),
                                         GPT_IO_PIN_GTIOCA));

    /** Set the duty cycle for GTIOB so that the compare match occurs at the beginning of the timer period. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, (GPT_TEST_PERIOD_COUNTS / 4), GPT_IO_PIN_GTIOCB));

    /** Enable the GTIOA/B outputs. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputEnable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputEnable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCB));

    /** Start the timer. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /** Configure the Input Capture timer to measure the time between the two pulses.
     * Note: Do not configure start or stop source for timer overflow, when both pins transition. */
    g_timer_test_capture_cfg_extend.start_source = GPT_SOURCE_GTIOCB_FALLING_WHILE_GTIOCA_LOW;
    g_timer_test_capture_cfg_extend.stop_source  = GPT_SOURCE_GTIOCA_RISING_WHILE_GTIOCB_LOW;
    g_timer_test_capture_cfg_extend.clear_source =
        (gpt_source_t) ((uint32_t) GPT_SOURCE_GTIOCB_FALLING_WHILE_GTIOCA_LOW |
                        (uint32_t) GPT_SOURCE_GTIOCA_RISING_WHILE_GTIOCB_HIGH);
    uint32_t capture = r_timer_capture_output(&g_timer_test_capture_cfg,
                                              gp_timer_test_input_capture_api,
                                              TIMER_TEST_NUM_CAPTURES,
                                              TIMER_TEST_CAPTURE_AVG_VERIFY);

    /** Both the GTIOA and GTIOB pulse widths should be 1 / 4 of the total timer period. The GTIOB pulse is at
     * the beginning of the timer period and GTIOA is at the end of the timer period. This means that the expected
     * time between the two pulses is (timer period) / 2. */
    TEST_ASSERT_UINT32_WITHIN(1, GPT_TEST_PERIOD_COUNTS / 2U, capture);
}

/**
 * @req{gpt_pwm_mode3,SWFLEX-2652,R_GPT_Open} The GPT shall support configuring Asymmetric Triangle Mode 3.
 *
 * @verify{gpt_pwm_mode3} Two asymmetric pulses are generated while interrupts disabled.
 */
TEST(R_GPT_TG2, TC_AsymmetricTriangleMode3)
{
    r_gpt_tests_restore_clock_settings(); // Set good clock values

    /** Configure the GPT timer to use Asymmetric triangle wave mode 3 with interrupts disabled. */
    g_timer_test_ram_cfg.mode                     = TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3;
    g_timer_test_ram_cfg.p_callback               = NULL;
    g_timer_test_ram_cfg.cycle_end_irq            = FSP_INVALID_VECTOR;
    g_gpt_test_pwm_ram_cfg.trough_irq             = FSP_INVALID_VECTOR;
    g_timer_test_ram_cfg_extend.gtioca.stop_level = GPT_PIN_LEVEL_LOW;
    g_timer_test_ram_cfg_extend.gtiocb.stop_level = GPT_PIN_LEVEL_LOW;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /** Configure two identical pulses that are out of phase by (3U * GPT_TEST_PERIOD_COUNTS / 5U) timer ticks. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, 4U * GPT_TEST_PERIOD_COUNTS / 5U,
                                         GPT_IO_PIN_GTIOCA | GPT_IO_PIN_TROUGH));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, GPT_TEST_PERIOD_COUNTS / 5U,
                                         GPT_IO_PIN_GTIOCA | GPT_IO_PIN_CREST));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, GPT_TEST_PERIOD_COUNTS / 5U,
                                         GPT_IO_PIN_GTIOCB | GPT_IO_PIN_TROUGH));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, 4U * GPT_TEST_PERIOD_COUNTS / 5U,
                                         GPT_IO_PIN_GTIOCB | GPT_IO_PIN_CREST));

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /** Configure the Input Capture timer to measure the time between the two pulses. */
    g_timer_test_capture_cfg_extend.start_source = GPT_SOURCE_GTIOCB_RISING_WHILE_GTIOCA_LOW;
    g_timer_test_capture_cfg_extend.stop_source  = GPT_SOURCE_GTIOCA_RISING_WHILE_GTIOCB_HIGH;
    g_timer_test_capture_cfg_extend.clear_source = GPT_SOURCE_GTIOCB_RISING_WHILE_GTIOCA_LOW;
    uint32_t capture = r_timer_capture_output(&g_timer_test_capture_cfg,
                                              gp_timer_test_input_capture_api,
                                              TIMER_TEST_NUM_CAPTURES,
                                              TIMER_TEST_CAPTURE_AVG_VERIFY);

    /** Verify that the time between GTIOA rising edge and GTIOB rising edge is (3U * GPT_TEST_PERIOD_COUNTS / 5U). */
    TEST_ASSERT_UINT32_WITHIN(1, 3U * GPT_TEST_PERIOD_COUNTS / 5U, capture);
}

/**
 * @verify{gpt_event_count_gtetrg} An error is returned when event counting is configured for a triangle PWM mode.
 */
TEST(R_GPT_TG2, TC_EventCountUnsupported_TrianglePWM)
{
#if !GPT_CFG_PARAM_CHECKING_ENABLE
    TEST_IGNORE_MESSAGE("This test is only run when parameter checking is enabled.");
#else

    /* Open timer in triangle PWM mode. */
    g_timer_test_ram_cfg.mode                   = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    g_timer_test_ram_cfg.p_callback             = NULL;
    g_timer_test_ram_cfg.cycle_end_irq          = FSP_INVALID_VECTOR;
    g_timer_test_ram_cfg_extend.count_up_source = TIMER_TEST_GPT_GTETRG_COUNT_UP;

    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_MODE, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
#endif
}

#define GPT_COMPLEMENTARY_TESTS_NUM_CFGS    (2)

/**
 * @req{gpt_pwm_complementary,SWFLEX-2873,R_GPT_Open} The GPT shall support setting complementary outputs simultaneously.
 *
 * @verify{gpt_pwm_complementary} Using GPT_IO_PIN_GTIOCA_AND_GTIOCB with both pins set complementary to each other retains their complementary status even at 0% and 100% duty.
 */
TEST(R_GPT_TG2, TC_ComplementaryPWM)
{
#if TIMER_TEST_COMPLEMENTARY_MODE_TODO
    TEST_IGNORE_MESSAGE("This test is temporarily ignored for RA6T2 pending further testing.");
#else

    /* Intermediate variables for readability */
    timer_cfg_t        * p_timer_cfg        = &g_timer_test_capture_cfg;
    gpt_extended_cfg_t * p_timer_extend_cfg = &g_timer_test_capture_cfg_extend;
    const timer_api_t  * p_timer_api        = gp_timer_test_input_capture_api;

    uint32_t             capture = 0U;
    timer_test_capture_t gtioca_capture;
    timer_test_capture_t gtiocb_capture;
    uint32_t             duty_cycle_counts = g_timer_test_ram_cfg.period_counts * 60 / 100; // NOLINT(readability-magic-numbers)

    /* Create and modify the configuration for GPT PWM mode with a given duty cycle */
    g_timer_test_ram_cfg.mode          = TIMER_MODE_PWM;
    g_timer_test_ram_cfg.p_callback    = NULL;
    g_timer_test_ram_cfg.cycle_end_irq = FSP_INVALID_VECTOR;

    gpt_gtior_setting_t gtior_cfgs[GPT_COMPLEMENTARY_TESTS_NUM_CFGS] = {0};

    /* Set GTIOA to initial low, cycle end low and GTIOB to initial high, cycle end high */
    gtior_cfgs[0].gtior_b.gtioa  = 0x07; // Initial low, low at cycle end, toggled on GTCCRA/B compare match
    gtior_cfgs[0].gtior_b.oadflt = 0;    // Output low when counting stops
    gtior_cfgs[0].gtior_b.oae    = 1;    // Output enabled
    gtior_cfgs[0].gtior_b.gtiob  = 0x1B; // Initial high, high at cycle end, toggled on GTCCRA/B compare match
    gtior_cfgs[0].gtior_b.obdflt = 1;    // Output high when counting stops
    gtior_cfgs[0].gtior_b.obe    = 1;    // Output enabled

    /* Set GTIOA to initial high, cycle end high and GTIOB to initial low, cycle end low */
    gtior_cfgs[1].gtior_b.gtioa  = 0x1B; // Initial high, high at cycle end, toggled on GTCCRA/B compare match
    gtior_cfgs[1].gtior_b.oadflt = 1;    // Output high when counting stops
    gtior_cfgs[1].gtior_b.oae    = 1;    // Output enabled
    gtior_cfgs[1].gtior_b.gtiob  = 0x07; // Initial low, low at cycle end, toggled on GTCCRA/B compare match
    gtior_cfgs[1].gtior_b.obdflt = 0;    // Output low when counting stops
    gtior_cfgs[1].gtior_b.obe    = 1;    // Output enabled

    /* Perform test loop */
    for (uint32_t i = 0; i < GPT_COMPLEMENTARY_TESTS_NUM_CFGS; i++)
    {
        /* Intermediate variables for readability */
        uint32_t oadflt = gtior_cfgs[i].gtior_b.oadflt;
        uint32_t obdflt = gtior_cfgs[i].gtior_b.obdflt;
        uint32_t gtioa  = gtior_cfgs[i].gtior_b.gtioa;
        uint32_t gtiob  = gtior_cfgs[i].gtior_b.gtiob;

        g_timer_test_ram_cfg_extend.gtior_setting = gtior_cfgs[i];

        /* Capture high for active-high config and low for active-low config */
        gtioca_capture = (oadflt ? TIMER_TEST_CAPTURE_PULSE_WIDTH_LOW : TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH);
        gtiocb_capture = (obdflt ? TIMER_TEST_CAPTURE_PULSE_WIDTH_LOW : TIMER_TEST_CAPTURE_PULSE_WIDTH_HIGH);

        /* Open, start, and set to 0% duty and Wait for the waveform to update to avoid capturing partial waveforms */
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, 0, GPT_IO_PIN_GTIOCA_AND_GTIOCB));

        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

        /* Verify that GTIOCA and GTIOCB are complementary for 0%/100% duty and correct per configuration. */
        volatile uint32_t timeout = TIMER_TEST_WAVEFORM_TIMEOUT / TIMER_TEST_NUM_CAPTURES;
        while ((timeout--) > 0)
        {
            TEST_ASSERT_EQUAL(((gtioa & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A));
            TEST_ASSERT_EQUAL(((gtiob & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));
        }

        /* Set both GTIOCA and GTIOCB using complementary mode and Wait for the waveform to update to avoid capturing partial waveforms */
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_GPT_DutyCycleSet(&g_gpt_ctrl, duty_cycle_counts, GPT_IO_PIN_GTIOCA_AND_GTIOCB));
        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

        uint32_t gtcnt = g_gpt_ctrl.p_reg->GTCNT;
        while (gtcnt > (duty_cycle_counts / 4))
        {
            /* Wait for gtcnt to be within the OFF portion of a period. */
            gtcnt = g_gpt_ctrl.p_reg->GTCNT;
        }

        /* Check that pins are complementary */
        TEST_ASSERT_EQUAL(TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A), !TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));

        /* Test that both GTIOCA and GTIOCB are running at the appropriate duty by measuring pulse widths. */
        timer_test_capture_cfg(p_timer_extend_cfg, gtioca_capture, GPT_IO_PIN_GTIOCA);
        capture = r_timer_capture_output(p_timer_cfg,
                                         p_timer_api,
                                         TIMER_TEST_NUM_CAPTURES,
                                         TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts - duty_cycle_counts, capture); // Increased from 1 to 2. See FSPRA-2975

        gtcnt = g_gpt_ctrl.p_reg->GTCNT;
        while (gtcnt > (duty_cycle_counts / 4))
        {
            /* Wait for gtcnt to be within the OFF portion of a period. */
            gtcnt = g_gpt_ctrl.p_reg->GTCNT;
        }

        timer_test_capture_cfg(p_timer_extend_cfg, gtiocb_capture, GPT_IO_PIN_GTIOCB);
        capture = r_timer_capture_output(p_timer_cfg,
                                         p_timer_api,
                                         TIMER_TEST_NUM_CAPTURES,
                                         TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts - duty_cycle_counts, capture); // Increased from 1 to 2. See FSPRA-2975

        gtcnt = g_gpt_ctrl.p_reg->GTCNT;
        while (gtcnt < duty_cycle_counts)
        {
            /* Wait for gtcnt to be within the ON portion of a period. */
            gtcnt = g_gpt_ctrl.p_reg->GTCNT;
        }

        /* Check that pins are complementary */
        TEST_ASSERT_EQUAL(TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A), !TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));

        /* Set both GTIOCA and GTIOCB to 100% duty and Wait for the waveform to update to avoid capturing partial waveforms */
        fsp_err_t err =
            R_GPT_DutyCycleSet(&g_gpt_ctrl, g_timer_test_ram_cfg.period_counts, GPT_IO_PIN_GTIOCA_AND_GTIOCB);
        TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

        /* Verify that GTIOCA and GTIOCB are complementary for 0%/100% duty and correct per configuration. */
        timeout = TIMER_TEST_WAVEFORM_TIMEOUT / TIMER_TEST_NUM_CAPTURES;
        while ((timeout--) > 0)
        {
            TEST_ASSERT_EQUAL(!((gtioa & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A));
            TEST_ASSERT_EQUAL(!((gtiob & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));
        }

        R_GPT_Close(&g_gpt_ctrl);
    }
#endif
}

/*
 * @verify{gpt_pwm_output_delay} Verify that the PWM Output Delay API functions return the current error codes.
 */
TEST(R_GPT_TG2, TC_PwmOutputDelayParameterChecking)
{
#if 0U == BSP_PERIPHERAL_GPT_ODC_PRESENT || GPT_CFG_PARAM_CHECKING_ENABLE == 0 || \
    !defined(TIMER_TEST_ODC_CHANNEL)
    TEST_IGNORE_MESSAGE("This test requires the PWM Output Delay Circuit to be present.");
#else

    /**
     * On the RA6T2, the GPT Core clock will be operating at 25 Mhz at this point.
     * The GPT Core clock must operate in the range [80, 200] Mhz.
     *
     * Verify that an error is returned if the GPT Core clock is out of the valid operating range for the PWM Output Delay Circuit.
     */
 #if (BSP_PERIPHERAL_GPT_GTCLK_PRESENT && TEST_GPT && !GPT_CFG_GPTCLK_BYPASS)
    TEST_ASSERT_GREATER_OR_EQUAL(0, TIMER_TEST_GPT_CKDIVCR_MAX);
    TIMER_TEST_UPDATE_GPTCKDIVCR(TIMER_TEST_GPT_CKDIVCR_MAX)
 #endif
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_STATE, R_GPT_PwmOutputDelayInitialize());

    /* Restore the clocks to there original settings. */
    r_gpt_tests_restore_clock_settings();

    /** Configure the PWM timer with an invalid PWM Output Delay Circuit channel. */
    g_timer_test_ram_cfg.channel = (uint8_t) (32U - __CLZ(BSP_FEATURE_GPT_GPTEH_CHANNEL_MASK) + 1U) & // NOLINT(readability-magic-numbers)
                                   UINT8_MAX;
    g_timer_test_ram_cfg.mode          = TIMER_MODE_PWM;
    g_timer_test_ram_cfg.p_callback    = NULL;
    g_timer_test_ram_cfg.cycle_end_irq = FSP_INVALID_VECTOR;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /**
     * On the RA6T2, channels [0,3] are PWM Output Delay Circuit channels.
     *
     * Verify that an error is returned if the channel is not a PWM Output Delay Circuit channel.
     */
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_CHANNEL,
                      R_GPT_PwmOutputDelaySet(&g_gpt_ctrl, GPT_PWM_OUTPUT_DELAY_EDGE_RISING,
                                              GPT_PWM_OUTPUT_DELAY_SETTING_31_32, GPT_IO_PIN_GTIOCA));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_gpt_ctrl));

    /** Configure the PWM timer to use the custom waveform. */
    g_timer_test_ram_cfg.channel = TIMER_TEST_ODC_CHANNEL;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /** Verify that an error is returned if the GPT PWM Output Delay Circuit setting is set while the PWM Output Delay Circuit is not initialized. */
    TEST_ASSERT_EQUAL(FSP_ERR_NOT_INITIALIZED,
                      R_GPT_PwmOutputDelaySet(&g_gpt_ctrl, GPT_PWM_OUTPUT_DELAY_EDGE_RISING,
                                              GPT_PWM_OUTPUT_DELAY_SETTING_31_32, GPT_IO_PIN_GTIOCA));
#endif
}

typedef struct st_pwm_output_delay_test_case_type
{
    gpt_pwm_output_delay_edge_t    edge;
    gpt_pwm_output_delay_setting_t delay_setting;
    uint32_t const                 pin;
} pwm_output_delay_test_case_type;

/**
 * @req{gpt_pwm_output_delay,SWFLEX-3614,R_GPT_PwmOutputDelaySet} The GPT driver shall support configuring the PWM Output Delay Circuit on supported MCUs.
 * @verify{gpt_pwm_output_delay} The appropriate registers are set after calling the PWM Output Delay API functions.
 *
 * Note: It is not possible to measure the delay using a GPT instance in input-capture mode because the delay is too small.
 */
TEST(R_GPT_TG2, TC_PwmOutputDelay)
{
#if (0U == BSP_PERIPHERAL_GPT_ODC_PRESENT)
    TEST_IGNORE_MESSAGE("This test requires the PWM Output Delay Circuit to be present.");
#else
    fsp_err_t err;

    /* Restore the clocks to there original settings. */
    r_gpt_tests_restore_clock_settings();

    /** Initialize the PWM Output Generation Delay Circuit. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_PwmOutputDelayInitialize());

    /* Derrive GPT Clock Frequency - GTCLK or PCLKD, based on configuration macros */
 #if (BSP_FEATURE_GPT_ODC_FRANGE_FREQ_MIN > 0)

    /** Verify that the circuit is enabled. */
    uint32_t gtdlycr1 = (uint32_t) R_GPT_ODC->GTDLYCR1;

  #if (BSP_PERIPHERAL_GPT_GTCLK_PRESENT && (GPT_CFG_GPTCLK_BYPASS == 0))
    uint8_t  divider       = R_SYSTEM->GPTCKDIVCR;
    uint32_t gpt_frequency = R_BSP_SourceClockHzGet((fsp_priv_source_clock_t) R_SYSTEM->GPTCKCR_b.GPTCKSEL) >> divider;
  #else
    uint32_t gpt_frequency = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_PCLKD);
  #endif

    uint32_t expected_frange   = BSP_FEATURE_GPT_ODC_FRANGE_SET_BIT(gpt_frequency);
    uint32_t expected_gtdlycr1 = expected_frange | R_GPT_ODC_GTDLYCR1_DLLEN_Msk;
    TEST_ASSERT_BITS((R_GPT_ODC_GTDLYCR1_FRANGE_Msk | R_GPT_ODC_GTDLYCR1_DLYRST_Msk), expected_gtdlycr1, gtdlycr1);
 #endif

    /** Configure a valid PWM Output Delay Circuit channel. */
    g_timer_test_ram_cfg.channel = TIMER_TEST_ODC_CHANNEL;

    /** Period increased in order to test parameter checking in triangle mode while counting down. */
    g_timer_test_ram_cfg.period_counts = 2 * GPT_TEST_PERIOD_COUNTS;
    g_timer_test_ram_cfg.mode          = TIMER_MODE_PWM;
    g_timer_test_ram_cfg.p_callback    = NULL;
    g_timer_test_ram_cfg.cycle_end_irq = FSP_INVALID_VECTOR;

    pwm_output_delay_test_case_type test_case[] =
    {
        {GPT_PWM_OUTPUT_DELAY_EDGE_RISING,  GPT_PWM_OUTPUT_DELAY_SETTING_31_32,   GPT_IO_PIN_GTIOCA     },
        {GPT_PWM_OUTPUT_DELAY_EDGE_RISING,  GPT_PWM_OUTPUT_DELAY_SETTING_BYPASS,  GPT_IO_PIN_GTIOCA     },
        {GPT_PWM_OUTPUT_DELAY_EDGE_FALLING, GPT_PWM_OUTPUT_DELAY_SETTING_31_32,   GPT_IO_PIN_GTIOCB     },
        {GPT_PWM_OUTPUT_DELAY_EDGE_FALLING, GPT_PWM_OUTPUT_DELAY_SETTING_BYPASS,  GPT_IO_PIN_GTIOCB     }
    };

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    uint8_t  channel      = g_gpt_ctrl.p_cfg->channel;
    uint32_t channel_mask = g_gpt_ctrl.channel_mask;

    for (uint32_t i = 0; i < sizeof(test_case) / sizeof(test_case[0]); i++)
    {
        err = R_GPT_PwmOutputDelaySet(&g_gpt_ctrl, test_case[i].edge, test_case[i].delay_setting, test_case[i].pin);
        TEST_ASSERT_EQUAL(FSP_SUCCESS, err);

        /* Expected Values */
        bool     bypass           = GPT_PWM_OUTPUT_DELAY_SETTING_BYPASS == test_case[i].delay_setting;
        uint32_t bypass_expected  = bypass ? 0 : channel_mask;  // Bypass is active-low (default: enabled)
        uint32_t enabled_expected = 0;                          // Enabled is active-low (default: enabled) FSP driver does not currently set this value.
        uint32_t delay_expected   = test_case[i].delay_setting; // 4x for 128-count resolution, since api is for 32-count resolution.
 #if defined(BSP_FEATURE_GPT_ODC_128_RESOLUTION_SUPPORTED) && BSP_FEATURE_GPT_ODC_128_RESOLUTION_SUPPORTED
        delay_expected *= 4;
 #endif

        /* Actual Values */
        bool channel_a = (GPT_IO_PIN_GTIOCA == test_case[i].pin);
        bool rising    = (GPT_PWM_OUTPUT_DELAY_EDGE_RISING == test_case[i].edge);
        volatile R_GPT_ODC_GTDLYR_Type * delay_reg_ptr = rising ? R_GPT_ODC->GTDLYR : R_GPT_ODC->GTDLYF;
        uint32_t bypass_actual  = (uint32_t) 0xF & R_GPT_ODC->GTDLYCR2;
        uint32_t enabled_actual = (uint32_t) 0xF & (R_GPT_ODC->GTDLYCR2 >> 8);
        uint32_t delay_actual   = (uint32_t) channel_a ? delay_reg_ptr[channel].A : delay_reg_ptr[channel].B;

        TEST_ASSERT_EQUAL(bypass_expected, bypass_actual);   // Verify Circuit bypass
        TEST_ASSERT_EQUAL(enabled_expected, enabled_actual); // Verify Circuit enabled
        if (!bypass)
        {
            TEST_ASSERT_EQUAL(delay_expected, delay_actual); // Verify delay value if enabled, else the value doesn't matter
        }
    }

 #if GPT_CFG_PARAM_CHECKING_ENABLE == 1

    /** Set the duty cycle count to a value that is in the range described in Table 22.4 of the RA6T2 manual R01UH0951EJ0100. */
    err = R_GPT_DutyCycleSet(&g_gpt_ctrl, g_timer_test_ram_cfg.period_counts - 1, GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);

    /** Verify that an error is returned when the PWM Output delay settings are set. */
    err = R_GPT_PwmOutputDelaySet(&g_gpt_ctrl,
                                  GPT_PWM_OUTPUT_DELAY_EDGE_FALLING,
                                  GPT_PWM_OUTPUT_DELAY_SETTING_31_32,
                                  GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_STATE, err);

    /** Reconfigure the GPT instance in triangle-mode. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_gpt_ctrl));
    g_timer_test_ram_cfg.mode = TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /** Set the duty cycle count to a value that is in the range described in Table 22.4 of the RA6T2 manual R01UH0951EJ0100. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, 2, GPT_IO_PIN_GTIOCA | GPT_IO_PIN_CREST));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /** Wait for the timer to be counting down. */
    FSP_HARDWARE_REGISTER_WAIT(g_gpt_ctrl.p_reg->GTST_b.TUCF, 0U);

    /** Verify that an error is returned when the PWM Output delay settings are set. */
    err = R_GPT_PwmOutputDelaySet(&g_gpt_ctrl,
                                  GPT_PWM_OUTPUT_DELAY_EDGE_FALLING,
                                  GPT_PWM_OUTPUT_DELAY_SETTING_31_32,
                                  GPT_IO_PIN_GTIOCA);
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_STATE, err);

    /** Reconfigure the GPT instance in triangle-mode. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Close(&g_gpt_ctrl));
    g_timer_test_ram_cfg.mode = TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /** Set the duty cycle count to a value that is in the range described in Table 22.4 of the RA6T2 manual R01UH0951EJ0100. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_DutyCycleSet(&g_gpt_ctrl, 2, GPT_IO_PIN_GTIOCB));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /** Wait for the timer to be counting down. */
    FSP_HARDWARE_REGISTER_WAIT(g_gpt_ctrl.p_reg->GTST_b.TUCF, 0U);

    /** Verify that an error is returned when the PWM Output delay settings are set. */
    err = R_GPT_PwmOutputDelaySet(&g_gpt_ctrl,
                                  GPT_PWM_OUTPUT_DELAY_EDGE_FALLING,
                                  GPT_PWM_OUTPUT_DELAY_SETTING_31_32,
                                  GPT_IO_PIN_GTIOCB);
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_STATE, err);
 #endif
#endif
}

/*
 * @verify{gpt_pwm_output_delay} The configured output delay setting will be visible on a scope or logic analyzer.
 */
TEST(R_GPT_TG2, TC_PwmOutputDelayManualTest)
{
#define TIMER_TEST_ODC_TODO    (!(BSP_MCU_GROUP_RA6T2 || BSP_MCU_GROUP_RA8P1 || BSP_MCU_GROUP_RA8M2)) // TODO: There are a lot of MCUs that support ODC but aren't supported by this test case.
#if (0U == BSP_PERIPHERAL_GPT_ODC_PRESENT) || TIMER_TEST_ODC_TODO
    TEST_IGNORE_MESSAGE("This test requires the PWM Output Delay Circuit to be present.");
#else
    TEST_IGNORE_MESSAGE("Enable this test to verify that the output delay settings are working correctly using a scope.");

    /** Restore the clocks to there original settings. */
    r_gpt_tests_restore_clock_settings();

    /** Configure the manual test pin. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_ODC_MANUAL_TEST_PIN,
                                      (uint32_t) IOPORT_CFG_PERIPHERAL_PIN | (uint32_t) IOPORT_PERIPHERAL_GPT1 |
                                      (uint32_t) IOPORT_CFG_DRIVE_HIGH));

    /** Initialize the PWM Output Generation Delay Circuit. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_PwmOutputDelayInitialize());

    /** Configure a valid PWM Output Delay Circuit channel. */
    g_timer_test_ram_cfg.channel = TIMER_TEST_ODC_CHANNEL;

    /** Configure the PWM timer to use the PWM waveform. */
    g_timer_test_ram_cfg.period_counts = GPT_TEST_PERIOD_COUNTS;
    g_timer_test_ram_cfg.mode          = TIMER_MODE_PWM;
    g_timer_test_ram_cfg.p_callback    = NULL;
    g_timer_test_ram_cfg.cycle_end_irq = FSP_INVALID_VECTOR;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputEnable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA));

    /** Generate the PWM signal for 1 second without any delay settings. */
    R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS); // NOLINT(readability-magic-numbers)

    /** Disable Output for 500 ms. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputDisable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA));
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);  // NOLINT(readability-magic-numbers)
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_OutputEnable(&g_gpt_ctrl, GPT_IO_PIN_GTIOCA));

    /** Add the maximum PWM Output Delay to the falling edge of the PWM pulse. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_PwmOutputDelaySet(&g_gpt_ctrl, GPT_PWM_OUTPUT_DELAY_EDGE_FALLING,
                                              GPT_PWM_OUTPUT_DELAY_SETTING_31_32, GPT_IO_PIN_GTIOCA));

    /** Generate the PWM signal for 1 second with the added delay settings. */
    R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS); // NOLINT(readability-magic-numbers)
#endif
}

/*******************************************************************************************************************//**
 * @}
 **********************************************************************************************************************/

/*
 * During test setup, clock divider are set for the PWM tests. These settings set the GPT Core clock out of the
 * operating range for using the PWM Output Delay circuit. This function restores the settings to their
 * original settings.
 */
void r_gpt_tests_restore_clock_settings (void)
{
    static cgc_instance_ctrl_t g_cgc_ctrl;
    static cgc_cfg_t           g_cgc_cfg =
    {
        .p_callback = NULL,
        .p_context  = NULL,
    };

    /** Reconfigure the GPTCLK Divider so that the clock is in the valid range for the PWM Output Delay Circuit.
     * GPTCLK = 100Mhz.
     */
#if (BSP_PERIPHERAL_GPT_GTCLK_PRESENT && TEST_GPT && !GPT_CFG_GPTCLK_BYPASS)
    TEST_ASSERT_GREATER_OR_EQUAL(0, TIMER_TEST_GPT_CKDIVCR_DFLT);
    TIMER_TEST_UPDATE_GPTCKDIVCR(TIMER_TEST_GPT_CKDIVCR_DFLT)
#endif

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_CGC_Open(&g_cgc_ctrl, &g_cgc_cfg));

    cgc_divider_cfg_t dividers;
    dividers.sckdivcr_b.iclk_div = (cgc_sys_clock_div_t) BSP_CFG_ICLK_DIV;
#if BSP_FEATURE_CGC_HAS_FCLK
    dividers.sckdivcr_b.fclk_div = (cgc_sys_clock_div_t) BSP_CFG_FCLK_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_PCLKA
    dividers.sckdivcr_b.pclka_div = (cgc_sys_clock_div_t) BSP_CFG_PCLKA_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_PCLKB
    dividers.sckdivcr_b.pclkb_div = (cgc_sys_clock_div_t) BSP_CFG_PCLKB_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_PCLKC
    dividers.sckdivcr_b.pclkc_div = (cgc_sys_clock_div_t) BSP_CFG_PCLKC_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_PCLKD
    dividers.sckdivcr_b.pclkd_div = (cgc_sys_clock_div_t) BSP_CFG_PCLKD_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_PCLKE
    dividers.sckdivcr_b.pclke_div = (cgc_sys_clock_div_t) BSP_CFG_PCLKE_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_BCLK
    dividers.sckdivcr_b.bclk_div = (cgc_sys_clock_div_t) BSP_CFG_BCLK_DIV;
#elif BSP_FEATURE_CGC_HAS_PCLKB
    dividers.sckdivcr_b.bclk_div = (cgc_sys_clock_div_t) BSP_CFG_PCLKB_DIV;
#endif

#if BSP_FEATURE_CGC_HAS_CPUCLK
    dividers.sckdivcr2_b.cpuclk_div = (cgc_sys_clock_div_t) BSP_CFG_CPUCLK_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_CPUCLK1
    dividers.sckdivcr2_b.cpuclk1_div = (cgc_sys_clock_div_t) BSP_CFG_CPUCLK1_DIV;
#endif
#if BSP_FEATURE_CGC_HAS_MRICLK
    dividers.sckdivcr2_b.mriclk_div = (cgc_sys_clock_div_t) BSP_CFG_MRICLK_DIV;
 #if BSP_FEATURE_CGC_HAS_NPUCLK
    dividers.sckdivcr2_b.npuclk_div = (cgc_sys_clock_div_t) BSP_CFG_NPUCLK_DIV;
 #else
    dividers.sckdivcr2_b.npuclk_div = (cgc_sys_clock_div_t) BSP_CFG_MRICLK_DIV;
 #endif
#endif

    /**
     * Restore system clocks to their configured values.
     *
     * When the GPTCLK is bypassed, the GPT Core clock will be PCLKD = 120 Mhz.
     */
    cgc_clock_t clock_source = (cgc_clock_t) (BSP_FEATURE_CGC_HAS_PLL ? CGC_CLOCK_PLL : BSP_CFG_CLOCK_SOURCE);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_CGC_SystemClockSet(&g_cgc_ctrl, clock_source, &dividers));

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_CGC_Close(&g_cgc_ctrl));
}
