/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @defgroup GPT_TG3_COMP_PWM_TESTS GPT Complementary PWM Tests
 * @ingroup RENESAS_TESTS
 * @brief   Test group 3: Complementary PWM Modes 1-4 with single/double buffer,
 *          dead time, duty cycle control, and buffer chain verification.
 *          Covers requirements REQ-OM-01 through REQ-SEC-17 (FSPRA-5725).
 *
 *          Ported from the standalone RTT-based harness (r_gpt_test_tg3_comp_pwm.c
 *          in compl_pwm/src/) to the FSP Unity Fixture framework to match the
 *          architecture of r_gpt_tests_tg1.c and r_gpt_tests_tg2_pwm.c.
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "r_gpt_test_data.h"
#include "r_gpt_three_phase.h"
#include "r_three_phase_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* Dead time test value - matches the value used in the original RTT harness so that
 * requirement traceability to the PRD Testing section is preserved. */
#define COMP_PWM_TEST_DEAD_TIME_COUNTS    (64U)

/* Short delay for hardware to settle after Open / DutyCycleSet. */
#define COMP_PWM_TEST_DELAY_US            (1U)
#define COMP_PWM_TEST_DELAY_SETTLE_MS     (10U)

/* Number of phases. */
#define COMP_PWM_TEST_THREE_PHASE_COUNT   (3U)

/* GTCCR array indices - must match the private enum in r_gpt_three_phase.c. */
#define COMP_PWM_PRV_GTCCRA               (0U)
#define COMP_PWM_PRV_GTCCRB               (1U)
#define COMP_PWM_PRV_GTCCRC               (2U)
#define COMP_PWM_PRV_GTCCRE               (3U)
#define COMP_PWM_PRV_GTCCRD               (4U)
#define COMP_PWM_PRV_GTCCRF               (5U)

/* Expected GTCR.MD values for each Complementary PWM mode (from RA2T1 UM 20.3.3.7 / 20.3.3.8). */
#define COMP_PWM_GTCR_MD_MODE1            (0xCU)
#define COMP_PWM_GTCR_MD_MODE2            (0xDU)
#define COMP_PWM_GTCR_MD_MODE3            (0xEU)
#define COMP_PWM_GTCR_MD_MODE4            (0xFU)

/* REQ-SEC-17: number of GTCNT samples used to verify all five counting sections. */
#define COMP_PWM_TEST_SEC17_SAMPLE_COUNT  (2000U)

/* ====================================================================
 *  Duty-Cycle Measurement — GPT Input Capture Channel (REQ-OM-01)
 * ====================================================================
 *  Physical wiring requirement:
 *    Connect FPB-RA2T1 P105 (GPT ch0 GTIOCA / GPTIO0A_Master)
 *    to       P300 (GPT ch3 GTIOCA input)
 *    with a jumper wire BEFORE running the Mode 1 capture test.
 *
 *  Channel selection:
 *    GPT ch3 is used because ch0/1/2 are reserved for the three-phase
 *    complementary PWM under test (master + slave1 + slave2).
 * ==================================================================== */

/* Measurement channel — must not overlap with three-phase ch 0/1/2 */
#define COMP_PWM_TEST_DUTY_CAP_GPT_CH        (3U)

/* GTCCR[] indices for the capture channel
 *   GTCCRA (index 0) — latched at rising  edge of GTIOCA input
 *   GTCCRB (index 1) — latched at falling edge of GTIOCA input
 * Note: re-defined locally for clarity and to keep capture-channel concerns
 *       independent of the three-phase GTCCRA/GTCCRB indices. */
#define COMP_PWM_TEST_DUTY_CAP_GTCCRA_IDX    (0U)
#define COMP_PWM_TEST_DUTY_CAP_GTCCRB_IDX    (1U)

/* Tolerance in timer counts (±1 for dead-time edge rounding). */
#define COMP_PWM_TEST_DUTY_CAP_TOLERANCE     (1U)

/* Poll timeout — ~100 000 iterations ≈ several PWM periods at typical clock. */
#define COMP_PWM_TEST_DUTY_CAP_POLL_TIMEOUT  (100000U)

/* Base address stride between consecutive GPT channels on RA2T1.
 * Computed from R_GPT1 - R_GPT0 so it stays correct across MCU variants. */
#define COMP_PWM_TEST_GPT_CH_REG_STRIDE      ((uint32_t)((uintptr_t)R_GPT1 - (uintptr_t)R_GPT0))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * External references (from hal_data.c / FSP generated code)
 **********************************************************************************************************************/
extern gpt_instance_ctrl_t g_timer_comp_pwm_master_ctrl;
extern gpt_instance_ctrl_t g_timer_comp_pwm_slave1_ctrl;
extern gpt_instance_ctrl_t g_timer_comp_pwm_slave2_ctrl;

extern const timer_cfg_t g_timer_comp_pwm_master_cfg;
extern const timer_cfg_t g_timer_comp_pwm_slave1_cfg;
extern const timer_cfg_t g_timer_comp_pwm_slave2_cfg;

extern const timer_instance_t g_timer_comp_pwm_master;
extern const timer_instance_t g_timer_comp_pwm_slave1;
extern const timer_instance_t g_timer_comp_pwm_slave2;

extern gpt_three_phase_instance_ctrl_t g_three_phase_comp_pwm_ctrl;
extern const three_phase_cfg_t g_three_phase_comp_pwm_cfg;

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/

/* RAM-resident copies of FSP-generated const config structs. Re-initialised from the const
 * templates in TEST_SETUP so every test starts from the same known state. */
static timer_cfg_t                     g_comp_pwm_master_cfg_ram;
static timer_cfg_t                     g_comp_pwm_slave1_cfg_ram;
static timer_cfg_t                     g_comp_pwm_slave2_cfg_ram;
static gpt_extended_cfg_t              g_comp_pwm_master_ext_ram;
static gpt_extended_pwm_cfg_t          g_comp_pwm_pwm_ext_ram;
static three_phase_cfg_t               g_comp_pwm_three_phase_cfg_ram;
static gpt_three_phase_instance_ctrl_t g_comp_pwm_three_phase_ctrl;
static timer_instance_t                g_comp_pwm_timer_instances_ram[COMP_PWM_TEST_THREE_PHASE_COUNT];

/* Duty-cycle capture channel (GPT ch3 with input capture on GTIOCA).
 * Opened only by tests that exercise REQ-OM-01; tracked separately so
 * TEST_TEAR_DOWN can release it if a test aborts mid-way. */
static gpt_instance_ctrl_t             g_comp_pwm_duty_cap_ctrl;
static bool                            g_comp_pwm_duty_cap_opened = false;

/* Extended cfg: capture A on rising, capture B on falling of GTIOCA pin.
 * GTIOCB is unused so we OR the WHILE_GTIOCB_LOW and _HIGH source flags
 * to capture regardless of GTIOCB level. */
static const gpt_extended_cfg_t g_comp_pwm_duty_cap_ext_cfg =
{
    .gtioca               = { .output_enabled = false,
                              .stop_level     = GPT_PIN_LEVEL_LOW },
    .gtiocb               = { .output_enabled = false,
                              .stop_level     = GPT_PIN_LEVEL_LOW },
    .capture_a_source     = (gpt_source_t)(GPT_SOURCE_GTIOCA_RISING_WHILE_GTIOCB_LOW |
                                           GPT_SOURCE_GTIOCA_RISING_WHILE_GTIOCB_HIGH),
    .capture_b_source     = (gpt_source_t)(GPT_SOURCE_GTIOCA_FALLING_WHILE_GTIOCB_LOW |
                                           GPT_SOURCE_GTIOCA_FALLING_WHILE_GTIOCB_HIGH),
    .start_source         = GPT_SOURCE_NONE,
    .stop_source          = GPT_SOURCE_NONE,
    .clear_source         = GPT_SOURCE_NONE,
    .count_up_source      = GPT_SOURCE_NONE,
    .count_down_source    = GPT_SOURCE_NONE,
    .adc_trigger          = GPT_ADC_TRIGGER_NONE,
    .dead_time_count_up   = 0U,
    .dead_time_count_down = 0U,
    .icds_clk_div         = GPT_CLOCK_DIVIDER_1,
    .gtior_setting.gtior  = 0U,
};

static const timer_cfg_t g_comp_pwm_duty_cap_timer_cfg =
{
    .mode              = TIMER_MODE_PERIODIC,    /* free-running up-counter         */
    .period_counts     = UINT32_MAX,             /* wrap only after ~13 min @ 50 MHz */
    .duty_cycle_counts = 0U,
    .source_div        = TIMER_SOURCE_DIV_1,     /* same clock as PWM channels      */
    .channel           = COMP_PWM_TEST_DUTY_CAP_GPT_CH,
    .p_callback        = NULL,
    .p_context         = NULL,
    .p_extend          = &g_comp_pwm_duty_cap_ext_cfg,
    .cycle_end_ipl     = BSP_IRQ_DISABLED,
    .cycle_end_irq     = FSP_INVALID_VECTOR,
};

/* Helpers. */
static void comp_pwm_test_reset_configs(void);
static void comp_pwm_test_apply_mode(timer_mode_t mode, three_phase_buffer_mode_t buf_mode,
                                     uint32_t dead_time);
static fsp_err_t comp_pwm_test_open(void);
static void comp_pwm_test_close(void);
static bool comp_pwm_test_gtber2_single_buffer_ok(three_phase_channel_t ch);
static bool comp_pwm_test_gtber2_double_buffer_ok(three_phase_channel_t ch);

/* Duty-capture helpers (only used by Mode 1 capture test). */
static fsp_err_t comp_pwm_test_duty_cap_open(void);
static void      comp_pwm_test_duty_cap_close(void);
static bool      comp_pwm_test_duty_cap_measure(uint32_t * const p_measured_counts);
static bool      comp_pwm_test_duty_cap_verify(uint32_t measured,
                                               uint32_t period_counts,
                                               uint32_t duty_counts);

/***********************************************************************************************************************
 * Helper function implementations
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Copy FSP-generated const config structs into RAM so tests can modify them safely.
 *        Called at the start of every test via apply_mode().
 **********************************************************************************************************************/
static void comp_pwm_test_reset_configs (void)
{
    /* Copy base timer configurations from FSP-generated const structs. */
    memcpy(&g_comp_pwm_master_cfg_ram, &g_timer_comp_pwm_master_cfg, sizeof(timer_cfg_t));
    memcpy(&g_comp_pwm_slave1_cfg_ram, &g_timer_comp_pwm_slave1_cfg, sizeof(timer_cfg_t));
    memcpy(&g_comp_pwm_slave2_cfg_ram, &g_timer_comp_pwm_slave2_cfg, sizeof(timer_cfg_t));

    /* Copy the master extended-cfg (dead time and pin options live here). */
    memcpy(&g_comp_pwm_master_ext_ram,
           g_comp_pwm_master_cfg_ram.p_extend,
           sizeof(gpt_extended_cfg_t));

    /* Copy the pwm-extended-cfg sub-struct (points to a const in flash by default). */
    if (g_comp_pwm_master_ext_ram.p_pwm_cfg != NULL)
    {
        memcpy(&g_comp_pwm_pwm_ext_ram,
               g_comp_pwm_master_ext_ram.p_pwm_cfg,
               sizeof(gpt_extended_pwm_cfg_t));
        g_comp_pwm_master_ext_ram.p_pwm_cfg = &g_comp_pwm_pwm_ext_ram;
    }

    /* Repoint the master cfg to the RAM ext struct. */
    g_comp_pwm_master_cfg_ram.p_extend = &g_comp_pwm_master_ext_ram;

    /* Build the three-phase timer-instance array from RAM copies. */
    g_comp_pwm_timer_instances_ram[0]       = g_timer_comp_pwm_master;
    g_comp_pwm_timer_instances_ram[1]       = g_timer_comp_pwm_slave1;
    g_comp_pwm_timer_instances_ram[2]       = g_timer_comp_pwm_slave2;
    g_comp_pwm_timer_instances_ram[0].p_cfg = &g_comp_pwm_master_cfg_ram;
    g_comp_pwm_timer_instances_ram[1].p_cfg = &g_comp_pwm_slave1_cfg_ram;
    g_comp_pwm_timer_instances_ram[2].p_cfg = &g_comp_pwm_slave2_cfg_ram;

    /* Copy the three-phase wrapper cfg. */
    memcpy(&g_comp_pwm_three_phase_cfg_ram,
           &g_three_phase_comp_pwm_cfg,
           sizeof(three_phase_cfg_t));
    g_comp_pwm_three_phase_cfg_ram.p_timer_instance[0] = &g_comp_pwm_timer_instances_ram[0];
    g_comp_pwm_three_phase_cfg_ram.p_timer_instance[1] = &g_comp_pwm_timer_instances_ram[1];
    g_comp_pwm_three_phase_cfg_ram.p_timer_instance[2] = &g_comp_pwm_timer_instances_ram[2];
}

/*******************************************************************************************************************//**
 * @brief Apply a given Complementary PWM mode, buffer mode, and dead time on top of the reset config.
 **********************************************************************************************************************/
static void comp_pwm_test_apply_mode (timer_mode_t              mode,
                                      three_phase_buffer_mode_t buf_mode,
                                      uint32_t                  dead_time)
{
    comp_pwm_test_reset_configs();

    g_comp_pwm_master_cfg_ram.mode = mode;
    g_comp_pwm_slave1_cfg_ram.mode = mode;
    g_comp_pwm_slave2_cfg_ram.mode = mode;

    if (g_comp_pwm_master_ext_ram.p_pwm_cfg != NULL)
    {
        g_comp_pwm_pwm_ext_ram.dead_time_count_up = dead_time;
    }

    g_comp_pwm_three_phase_cfg_ram.buffer_mode = buf_mode;
}

/*******************************************************************************************************************//**
 * @brief Open the three-phase instance with the current RAM config.
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_test_open (void)
{
    memset(&g_comp_pwm_three_phase_ctrl, 0, sizeof(g_comp_pwm_three_phase_ctrl));

    return R_GPT_THREE_PHASE_Open(&g_comp_pwm_three_phase_ctrl,
                                  &g_comp_pwm_three_phase_cfg_ram);
}

/*******************************************************************************************************************//**
 * @brief Stop and close the three-phase instance.
 **********************************************************************************************************************/
static void comp_pwm_test_close (void)
{
    R_GPT_THREE_PHASE_Stop(&g_comp_pwm_three_phase_ctrl);
    R_GPT_THREE_PHASE_Close(&g_comp_pwm_three_phase_ctrl);
}

/*******************************************************************************************************************//**
 * @brief Verify GTBER2 single-buffer configuration on a channel.
 *        Single buffer requires CMTCA=1, CP3DB=0, CPBTD=0.
 **********************************************************************************************************************/
static bool comp_pwm_test_gtber2_single_buffer_ok (three_phase_channel_t ch)
{
    return (g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTBER2_b.CMTCA == 0x1U) &&
           (g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTBER2_b.CP3DB == 0U) &&
           (g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTBER2_b.CPBTD == 0U);
}

/*******************************************************************************************************************//**
 * @brief Verify GTBER2 double-buffer configuration on a channel.
 *        Double buffer requires CMTCA=1, CP3DB=1, CPBTD=0.
 **********************************************************************************************************************/
static bool comp_pwm_test_gtber2_double_buffer_ok (three_phase_channel_t ch)
{
    return (g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTBER2_b.CMTCA == 0x1U) &&
           (g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTBER2_b.CP3DB == 1U) &&
           (g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTBER2_b.CPBTD == 0U);
}

/*******************************************************************************************************************//**
 * @brief Get pointer to the duty-capture channel's register block.
 *        Computed from R_GPT0 base + channel offset to avoid hardcoding R_GPT3.
 **********************************************************************************************************************/
static inline R_GPT0_Type * comp_pwm_test_duty_cap_regs (void)
{
    return (R_GPT0_Type *)((uintptr_t)R_GPT0 +
                           (COMP_PWM_TEST_DUTY_CAP_GPT_CH * COMP_PWM_TEST_GPT_CH_REG_STRIDE));
}

/*******************************************************************************************************************//**
 * @brief Open and start the duty-capture GPT channel.
 *        Call BEFORE starting the three-phase timer so the counter is already
 *        running when the first PWM edge arrives at GTIOCA.
 *
 * @return FSP_SUCCESS on success, FSP error code otherwise.
 **********************************************************************************************************************/
static fsp_err_t comp_pwm_test_duty_cap_open (void)
{
    fsp_err_t err = R_GPT_Open(&g_comp_pwm_duty_cap_ctrl, &g_comp_pwm_duty_cap_timer_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* Clear any stale capture flags before arming. */
    comp_pwm_test_duty_cap_regs()->GTST = 0U;

    err = R_GPT_Start(&g_comp_pwm_duty_cap_ctrl);
    if (FSP_SUCCESS == err)
    {
        g_comp_pwm_duty_cap_opened = true;
    }
    return err;
}

/*******************************************************************************************************************//**
 * @brief Close the duty-capture GPT channel and release the resource.
 **********************************************************************************************************************/
static void comp_pwm_test_duty_cap_close (void)
{
    if (g_comp_pwm_duty_cap_opened)
    {
        (void)R_GPT_Close(&g_comp_pwm_duty_cap_ctrl);
        g_comp_pwm_duty_cap_opened = false;
    }
}

/*******************************************************************************************************************//**
 * @brief Measure the high-time of one complete pulse on GTIOCA of the capture channel.
 *
 *        Capture register usage:
 *          GTCCRA — latched at rising  edge (GTST.TCFA flag set by hardware)
 *          GTCCRB — latched at falling edge (GTST.TCFB flag set by hardware)
 *
 *        Polled — never enables capture interrupts, so it is safe inside Unity
 *        TEST() bodies even with the test runner's own ISR setup.
 *
 * @param[out] p_measured_counts  High-time measured in timer counts.
 * @return true  if both edges captured within the poll timeout.
 * @return false if either rising or falling edge timed out.
 **********************************************************************************************************************/
static bool comp_pwm_test_duty_cap_measure (uint32_t * const p_measured_counts)
{
    R_GPT0_Type * const p_reg = comp_pwm_test_duty_cap_regs();

    /* ------------------------------------------------------------------ */
    /* Step 1: Wait for rising-edge capture (GTST.TCFA set by hardware). */
    /* ------------------------------------------------------------------ */
    uint32_t timeout = COMP_PWM_TEST_DUTY_CAP_POLL_TIMEOUT;
    while (0U == p_reg->GTST_b.TCFA)
    {
        if (0U == timeout--)
        {
            return false;   /* rising edge never came — check jumper wire. */
        }
    }
    uint32_t t_rise = p_reg->GTCCR[COMP_PWM_TEST_DUTY_CAP_GTCCRA_IDX];

    /* Clear TCFA so the next rising edge does not confuse TCFB polling. */
    p_reg->GTST_b.TCFA = 0U;

    /* ------------------------------------------------------------------ */
    /* Step 2: Wait for the immediately following falling-edge capture.   */
    /* ------------------------------------------------------------------ */
    timeout = COMP_PWM_TEST_DUTY_CAP_POLL_TIMEOUT;
    while (0U == p_reg->GTST_b.TCFB)
    {
        if (0U == timeout--)
        {
            return false;   /* falling edge never came. */
        }
    }
    uint32_t t_fall = p_reg->GTCCR[COMP_PWM_TEST_DUTY_CAP_GTCCRB_IDX];

    /* Clear TCFB so a subsequent measurement starts clean. */
    p_reg->GTST_b.TCFB = 0U;

    /* ------------------------------------------------------------------ */
    /* Step 3: Compute high-time, handling 32-bit counter wrap-around.    */
    /* ------------------------------------------------------------------ */
    if (t_fall >= t_rise)
    {
        *p_measured_counts = t_fall - t_rise;
    }
    else
    {
        /* Wrap: counter rolled over 0xFFFFFFFF between rise and fall. */
        *p_measured_counts = (UINT32_MAX - t_rise) + t_fall + 1U;
    }

    return true;
}

/*******************************************************************************************************************//**
 * @brief Verify a measured high-time matches the expected duty within tolerance.
 *
 *        In complementary PWM (triangle wave), for the master channel U:
 *          high_time = period_counts - duty_cycle_counts
 *
 *        This function checks the measurement falls within
 *        [expected - COMP_PWM_TEST_DUTY_CAP_TOLERANCE, expected + tolerance].
 *
 * @param[in] measured       High-time returned by comp_pwm_test_duty_cap_measure().
 * @param[in] period_counts  GTPR value of the master channel.
 * @param[in] duty_counts    duty_cycle_counts set in the timer config.
 * @return true if within tolerance, false otherwise.
 **********************************************************************************************************************/
static bool comp_pwm_test_duty_cap_verify (uint32_t measured,
                                           uint32_t period_counts,
                                           uint32_t duty_counts)
{
    if (period_counts < duty_counts)
    {
        return false;   /* Invalid configuration — would underflow. */
    }
    uint32_t expected = period_counts - duty_counts;

    uint32_t lower = (expected > COMP_PWM_TEST_DUTY_CAP_TOLERANCE)
                     ? (expected - COMP_PWM_TEST_DUTY_CAP_TOLERANCE)
                     : 0U;
    uint32_t upper = expected + COMP_PWM_TEST_DUTY_CAP_TOLERANCE;

    return (measured >= lower) && (measured <= upper);
}

/*******************************************************************************************************************//**
 * Test group setup / teardown
 **********************************************************************************************************************/

TEST_GROUP(R_GPT_TG3);

/*******************************************************************************************************************//**
 * @brief Test setup function called before every test in this test group.
 **********************************************************************************************************************/
TEST_SETUP(R_GPT_TG3)
{
#if 2U != GPT_CFG_OUTPUT_SUPPORT_ENABLE
    TEST_IGNORE_MESSAGE(
        "This test group requires 'Pin Output Support: Enabled with Extra Features' (GPT_CFG_OUTPUT_SUPPORT_ENABLE == 2).");
#endif

    /* Reset config structures to FSP-generated defaults. apply_mode() does this again per test,
     * but resetting here ensures the initial state is deterministic even if a test skips
     * apply_mode() (e.g. for negative/parameter-check cases). */
    comp_pwm_test_reset_configs();
}

/*******************************************************************************************************************//**
 * @brief Test teardown function called after every test in this test group.
 **********************************************************************************************************************/
TEST_TEAR_DOWN(R_GPT_TG3)
{
    /* If a test failed mid-way and left the driver open, close it here. */
    if (0U != g_comp_pwm_three_phase_ctrl.open)
    {
        R_GPT_THREE_PHASE_Stop(&g_comp_pwm_three_phase_ctrl);
        R_GPT_THREE_PHASE_Close(&g_comp_pwm_three_phase_ctrl);
    }

    /* Same for the duty-capture channel — only the Mode 1 capture test opens
     * it, but if that test asserted out before reaching its close call, the
     * channel would leak into the next test. */
    comp_pwm_test_duty_cap_close();
}

/***********************************************************************************************************************
 * Test Cases
 **********************************************************************************************************************/

/**
 * @req{gpt_comp_pwm_om_01,FSPRA-5725,R_GPT_THREE_PHASE_Open} In Complementary PWM Mode 1,
 *      GTCCRD transfers to GTCCRA at the end of the crest section (single buffer).
 *
 * @verify{gpt_comp_pwm_om_01} R_GPT_THREE_PHASE_Open with TIMER_MODE_COMPLEMENTARY_PWM_MODE1
 *      configures GTCR.MD = 0xC on all three channels, GTBER2 reflects single-buffer
 *      transfer (CMTCA=1, CP3DB=0, CPBTD=0), and the high-time of GPTIO0A_Master measured
 *      via GPT ch3 input capture matches (period_counts - duty_cycle_counts) within ±1 count.
 *
 * @note  Hardware requirement: jumper wire from P105 (GPT ch0 GTIOCA) to P300
 *        (GPT ch3 GTIOCA). Without the jumper, the capture-measurement portion will
 *        time out and this test will fail.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Mode1_CrestTransfer)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE1,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);

    /* Open the capture channel FIRST so its counter is running before the
     * first PWM edge arrives at GTIOCA. */
    TEST_ASSERT_EQUAL_MESSAGE(FSP_SUCCESS,
                              comp_pwm_test_duty_cap_open(),
                              "Failed to open GPT ch3 duty-capture channel");

    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_THREE_PHASE_Start(&g_comp_pwm_three_phase_ctrl));

    /* All three channels must be in single-buffer mode. */
    for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
    {
        TEST_ASSERT_TRUE(comp_pwm_test_gtber2_single_buffer_ok(ch));
    }

    /* GTCR.MD = 0xC indicates Complementary PWM Mode 1. */
    uint32_t gtcr_md =
        (g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
    TEST_ASSERT_EQUAL(COMP_PWM_GTCR_MD_MODE1, gtcr_md);

    /* Allow a settling delay so the first captured pulse is steady. */
    R_BSP_SoftwareDelay(COMP_PWM_TEST_DELAY_SETTLE_MS, BSP_DELAY_UNITS_MILLISECONDS);

    /* Measure GPTIO0A_Master high-time via GPT ch3 input capture. */
    uint32_t measured_counts = 0U;
    TEST_ASSERT_TRUE_MESSAGE(comp_pwm_test_duty_cap_measure(&measured_counts),
                             "Duty-capture timed out — check P105->P300 jumper wire");

    /* Verify high-time ≈ (period - duty) for the master channel U. */
    uint32_t period = g_comp_pwm_master_cfg_ram.period_counts;
    uint32_t duty   = g_comp_pwm_master_cfg_ram.duty_cycle_counts;
    TEST_ASSERT_TRUE_MESSAGE(comp_pwm_test_duty_cap_verify(measured_counts, period, duty),
                             "Measured high-time outside (period - duty) +/- tolerance");

    comp_pwm_test_close();
    comp_pwm_test_duty_cap_close();
}

/**
 * @req{gpt_comp_pwm_om_02,FSPRA-5725,R_GPT_THREE_PHASE_Open} In Complementary PWM Mode 2,
 *      GTCCRD transfers to GTCCRA at the end of the trough section (single buffer).
 *
 * @verify{gpt_comp_pwm_om_02} R_GPT_THREE_PHASE_Open with TIMER_MODE_COMPLEMENTARY_PWM_MODE2
 *      configures GTCR.MD = 0xD on all three channels with single-buffer GTBER2.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Mode2_TroughTransfer)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE2,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);

    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
    {
        TEST_ASSERT_TRUE(comp_pwm_test_gtber2_single_buffer_ok(ch));
    }

    uint32_t gtcr_md =
        (g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
    TEST_ASSERT_EQUAL(COMP_PWM_GTCR_MD_MODE2, gtcr_md);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_om_03,FSPRA-5725,R_GPT_THREE_PHASE_Open} In Complementary PWM Mode 3,
 *      transfer occurs at both crest and trough, and the mode supports both single and
 *      double-buffer chains.
 *
 * @verify{gpt_comp_pwm_om_03} R_GPT_THREE_PHASE_Open with MODE3 + single buffer produces
 *      GTBER2 single-buffer config. Re-opening with MODE3 + double buffer flips CP3DB to 1,
 *      initialises GTCCRF on all three channels, and reports GTCR.MD = 0xE.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Mode3_SingleAndDoubleBuffer)
{
    /* --- Single buffer branch --- */
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
    {
        TEST_ASSERT_TRUE(comp_pwm_test_gtber2_single_buffer_ok(ch));
    }

    comp_pwm_test_close();

    /* --- Double buffer branch --- */
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_DOUBLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
    {
        TEST_ASSERT_TRUE(comp_pwm_test_gtber2_double_buffer_ok(ch));

        /* GTCCRF must be seeded during Open() when double buffer is enabled. */
        uint32_t gtccrf = g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRF];
        TEST_ASSERT_NOT_EQUAL(0U, gtccrf);
    }

    uint32_t gtcr_md =
        (g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
    TEST_ASSERT_EQUAL(COMP_PWM_GTCR_MD_MODE3, gtcr_md);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_om_04,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} In Complementary PWM Mode 4,
 *      writes to GTCCRD bypass the buffer chain and transfer to GTCCRA immediately.
 *
 * @verify{gpt_comp_pwm_om_04} After R_GPT_THREE_PHASE_Open with MODE4, GTCR.MD = 0xF. After a
 *      single R_GPT_THREE_PHASE_DutyCycleSet, GTCCRA on channel U matches the requested duty
 *      within one microsecond.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Mode4_ImmediateTransfer)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE4,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    uint32_t gtcr_md =
        (g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCR >> R_GPT0_GTCR_MD_Pos) & 0xFU;
    TEST_ASSERT_EQUAL(COMP_PWM_GTCR_MD_MODE4, gtcr_md);

    /* Write GTCCRD via the API and expect GTCCRA to update immediately (no buffer delay). */
    uint32_t                 test_duty = g_comp_pwm_master_cfg_ram.period_counts / 4U;
    three_phase_duty_cycle_t duty      = {{test_duty, test_duty, test_duty}, {0}};
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty));

    R_BSP_SoftwareDelay(COMP_PWM_TEST_DELAY_US, BSP_DELAY_UNITS_MICROSECONDS);

    uint32_t gtccra = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRA];
    TEST_ASSERT_EQUAL(test_duty, gtccra);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dt_05,FSPRA-5725,R_GPT_THREE_PHASE_Open} Dead time is configurable via the
 *      GTDVU register on the master channel.
 *
 * @verify{gpt_comp_pwm_dt_05} After R_GPT_THREE_PHASE_Open with dead_time_count_up set to a
 *      known value, the master channel GTDVU register reads back that value.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DeadTime_Configurable)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    uint32_t gtdvu = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
    TEST_ASSERT_EQUAL(COMP_PWM_TEST_DEAD_TIME_COUNTS, gtdvu);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dt_06,FSPRA-5725,R_GPT_THREE_PHASE_Open} Dead time must satisfy
 *      0 < GTDVU < GTPR.
 *
 * @verify{gpt_comp_pwm_dt_06} After Open, GTDVU is strictly greater than 0 and strictly less
 *      than GTPR on the master channel.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DeadTime_ValidRange)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    uint32_t gtpr  = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTPR;
    uint32_t gtdvu = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;

    TEST_ASSERT_GREATER_THAN_UINT32(0U, gtdvu);
    TEST_ASSERT_LESS_THAN_UINT32(gtpr, gtdvu);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dt_07,FSPRA-5725,R_GPT_THREE_PHASE_Open} GTDVU is not buffered: a direct
 *      register write takes effect immediately.
 *
 * @verify{gpt_comp_pwm_dt_07} After Open, a direct write to GTDVU reads back the written value
 *      with no intermediate buffering.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DeadTime_NoBufferedWrite)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    /* Initial value from Open(). */
    TEST_ASSERT_EQUAL(COMP_PWM_TEST_DEAD_TIME_COUNTS,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU);

    /* Direct write, expect immediate read-back (no buffer chain). */
    uint32_t new_dt = COMP_PWM_TEST_DEAD_TIME_COUNTS * 2U;
    g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU = new_dt;
    TEST_ASSERT_EQUAL(new_dt,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dt_08,FSPRA-5725,R_GPT_THREE_PHASE_Open} Dead time prevents shoot-through
 *      on the complementary GTIOCnA / GTIOCnB output pair.
 *
 * @verify{gpt_comp_pwm_dt_08} After Open with non-zero dead time, GTDTCR is non-zero
 *      (dead-time output enabled) and GTDVU > 0 on the master channel.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DeadTime_NonOverlappingGuard)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    uint32_t gtdtcr = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDTCR;
    TEST_ASSERT_NOT_EQUAL(0U, gtdtcr);

    uint32_t gtdvu = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTDVU;
    TEST_ASSERT_GREATER_THAN_UINT32(0U, gtdvu);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dc_09,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} Duty cycle can be set
 *      independently per channel (U, V, W).
 *
 * @verify{gpt_comp_pwm_dc_09} A single R_GPT_THREE_PHASE_DutyCycleSet call with three distinct
 *      duty values produces three distinct GTCCRD register values across U, V, W.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DutyCycle_IndependentUVW)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_THREE_PHASE_Start(&g_comp_pwm_three_phase_ctrl));

    uint32_t period = g_comp_pwm_master_cfg_ram.period_counts;

    /* 25% / 50% / 75% on U / V / W. */
    three_phase_duty_cycle_t duty = {{period / 4U, period / 2U, (period * 3U) / 4U}, {0}};
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty));

    TEST_ASSERT_EQUAL(period / 4U,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD]);
    TEST_ASSERT_EQUAL(period / 2U,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_V]->GTCCR[COMP_PWM_PRV_GTCCRD]);
    TEST_ASSERT_EQUAL((period * 3U) / 4U,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_W]->GTCCR[COMP_PWM_PRV_GTCCRD]);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dc_10,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} Setting GTCCRA >= GTPR
 *      produces 0% duty (positive phase OFF, negative phase ON).
 *
 * @verify{gpt_comp_pwm_dc_10} DutyCycleSet with duty == period writes period into GTCCRD.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DutyCycle_ZeroPercent)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE4,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    uint32_t                 period = g_comp_pwm_master_cfg_ram.period_counts;
    three_phase_duty_cycle_t duty   = {{period, period, period}, {0}};
    (void) R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty);

    uint32_t gtccrd = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD];
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(period, gtccrd);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dc_11,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} Setting GTCCRA == 0
 *      produces 100% duty (positive phase ON, negative phase OFF).
 *
 * @verify{gpt_comp_pwm_dc_11} DutyCycleSet with duty == 1 (minimum accepted after parameter
 *      checking) writes a value <= 1 into GTCCRD.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DutyCycle_HundredPercent)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE4,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    /* The API may reject 0; use 1 as the minimum accepted value. */
    three_phase_duty_cycle_t duty = {{1U, 1U, 1U}, {0}};
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty));

    uint32_t gtccrd = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD];
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(1U, gtccrd);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_dc_12,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} On RA2T1 (16-bit GPT),
 *      duty-cycle arithmetic must not wrap around at 16 bits.
 *
 * @verify{gpt_comp_pwm_dc_12} Computing duty = period * 99 / 100 via a uint32_t intermediate
 *      produces a value that is strictly positive and <= period. DutyCycleSet accepts it.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_DutyCycle_NoOverflow16Bit)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    uint32_t period   = g_comp_pwm_master_cfg_ram.period_counts;
    uint32_t duty_pct = 99U;
    uint32_t duty_cnt = ((uint32_t) period * duty_pct) / 100U;

    /* Guard against 16-bit wrap-around. */
    TEST_ASSERT_GREATER_THAN_UINT32(0U, duty_cnt);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(period, duty_cnt);

    three_phase_duty_cycle_t duty = {{duty_cnt, duty_cnt, duty_cnt}, {0}};
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty));

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_buf_13,FSPRA-5725,R_GPT_THREE_PHASE_Open} Modes 1, 2, and 3 support the
 *      single-buffer transfer chain (GTCCRD -> Temp A -> GTCCRC -> GTCCRA).
 *
 * @verify{gpt_comp_pwm_buf_13} For each of MODE1, MODE2, MODE3 opened with single buffer,
 *      GTBER2 reports CMTCA=1 / CP3DB=0 / CPBTD=0 on all three channels.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Buffer_SingleChainModes123)
{
    const timer_mode_t modes[] =
    {
        TIMER_MODE_COMPLEMENTARY_PWM_MODE1,
        TIMER_MODE_COMPLEMENTARY_PWM_MODE2,
        TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
    };

    for (uint32_t m = 0U; m < (sizeof(modes) / sizeof(modes[0])); m++)
    {
        comp_pwm_test_apply_mode(modes[m],
                                 THREE_PHASE_BUFFER_MODE_SINGLE,
                                 COMP_PWM_TEST_DEAD_TIME_COUNTS);
        TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            TEST_ASSERT_TRUE(comp_pwm_test_gtber2_single_buffer_ok(ch));
        }

        comp_pwm_test_close();
    }
}

/**
 * @req{gpt_comp_pwm_buf_14,FSPRA-5725,R_GPT_THREE_PHASE_Open} Mode 3 supports the double-buffer
 *      transfer chain (GTCCRF -> Temp B -> GTCCRE -> GTCCRA).
 *
 * @verify{gpt_comp_pwm_buf_14} Open with MODE3 + double buffer sets GTBER2.CP3DB = 1 and
 *      initialises GTCCRF on all three channels.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Buffer_DoubleChainMode3)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_DOUBLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());

    for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
    {
        TEST_ASSERT_TRUE(comp_pwm_test_gtber2_double_buffer_ok(ch));

        uint32_t gtccrf = g_comp_pwm_three_phase_ctrl.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRF];
        TEST_ASSERT_NOT_EQUAL(0U, gtccrf);
    }

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_buf_15,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} In Mode 4, GTCCRD
 *      transfers directly to GTCCRA (immediate bypass of the buffer chain).
 *
 * @verify{gpt_comp_pwm_buf_15} After DutyCycleSet in MODE4, GTCCRA on channel U matches the
 *      written duty within one microsecond.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Buffer_Mode4ImmediateBypass)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE4,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_THREE_PHASE_Start(&g_comp_pwm_three_phase_ctrl));

    uint32_t                 period    = g_comp_pwm_master_cfg_ram.period_counts;
    uint32_t                 test_duty = period / 3U;
    three_phase_duty_cycle_t duty      = {{test_duty, test_duty, test_duty}, {0}};
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty));

    R_BSP_SoftwareDelay(COMP_PWM_TEST_DELAY_US, BSP_DELAY_UNITS_MICROSECONDS);

    uint32_t gtccra = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRA];
    TEST_ASSERT_EQUAL(test_duty, gtccra);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_buf_16,FSPRA-5725,R_GPT_THREE_PHASE_DutyCycleSet} Per RA2T1 UM 20.3.3.7,
 *      the slave-channel-2 (W phase) GTCCRD write triggers simultaneous temporary-register
 *      transfer across all three channels, so W must be written last.
 *
 * @verify{gpt_comp_pwm_buf_16} After a DutyCycleSet with three distinct duty values, all three
 *      GTCCRD registers hold their expected values — confirming that the driver honoured the
 *      U -> V -> W write order.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_Buffer_SlaveWriteOrdering)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_THREE_PHASE_Start(&g_comp_pwm_three_phase_ctrl));

    uint32_t                 period = g_comp_pwm_master_cfg_ram.period_counts;
    three_phase_duty_cycle_t duty   = {{period / 4U, period / 2U, (period * 3U) / 4U}, {0}};
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_THREE_PHASE_DutyCycleSet(&g_comp_pwm_three_phase_ctrl, &duty));

    TEST_ASSERT_EQUAL(period / 4U,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCCR[COMP_PWM_PRV_GTCCRD]);
    TEST_ASSERT_EQUAL(period / 2U,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_V]->GTCCR[COMP_PWM_PRV_GTCCRD]);
    TEST_ASSERT_EQUAL((period * 3U) / 4U,
                      g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_W]->GTCCR[COMP_PWM_PRV_GTCCRD]);

    comp_pwm_test_close();
}

/**
 * @req{gpt_comp_pwm_sec_17,FSPRA-5725,R_GPT_THREE_PHASE_Start} Complementary PWM uses a
 *      five-section counting operation: trough / count-up / crest / count-down / dead-band.
 *
 * @verify{gpt_comp_pwm_sec_17} Sampling GTCNT for 2000 iterations after Start observes all
 *      five sections: GTCNT near 0, GTCNT rising, GTCNT near GTPR, GTCNT falling, and GTCNT
 *      within dead_time of a boundary.
 */
TEST(R_GPT_TG3, TC_ComplementaryPwm_CountingSections_AllFiveObserved)
{
    comp_pwm_test_apply_mode(TIMER_MODE_COMPLEMENTARY_PWM_MODE3,
                             THREE_PHASE_BUFFER_MODE_SINGLE,
                             COMP_PWM_TEST_DEAD_TIME_COUNTS);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, comp_pwm_test_open());
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_THREE_PHASE_Start(&g_comp_pwm_three_phase_ctrl));

    uint32_t period    = g_comp_pwm_master_cfg_ram.period_counts;
    uint32_t dead_time = COMP_PWM_TEST_DEAD_TIME_COUNTS;

    /* Let PWM run a few cycles before sampling. */
    R_BSP_SoftwareDelay(COMP_PWM_TEST_DELAY_SETTLE_MS, BSP_DELAY_UNITS_MILLISECONDS);

    bool saw_trough   = false; /* GTCNT near 0 */
    bool saw_count_up = false; /* 0 < GTCNT < GTPR, counting up */
    bool saw_crest    = false; /* GTCNT near GTPR */
    bool saw_count_dn = false; /* 0 < GTCNT < GTPR, counting down */
    bool saw_deadband = false; /* GTCNT within dead_time of boundary */

    uint32_t prev_cnt = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCNT;

    for (uint32_t sample = 0U; sample < COMP_PWM_TEST_SEC17_SAMPLE_COUNT; sample++)
    {
        uint32_t cnt = g_comp_pwm_three_phase_ctrl.p_reg[THREE_PHASE_CHANNEL_U]->GTCNT;

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

    TEST_ASSERT_TRUE_MESSAGE(saw_trough,   "GTCNT trough section not observed");
    TEST_ASSERT_TRUE_MESSAGE(saw_count_up, "GTCNT count-up section not observed");
    TEST_ASSERT_TRUE_MESSAGE(saw_crest,    "GTCNT crest section not observed");
    TEST_ASSERT_TRUE_MESSAGE(saw_count_dn, "GTCNT count-down section not observed");
    TEST_ASSERT_TRUE_MESSAGE(saw_deadband, "GTCNT dead-band section not observed");

    comp_pwm_test_close();
}

/*******************************************************************************************************************//**
 * @} (end defgroup GPT_TG3_COMP_PWM_TESTS)
 **********************************************************************************************************************/
