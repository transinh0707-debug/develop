

1 — New header additions (top of test file)

/* ============================================================
 *  Duty-Cycle Measurement — GPT Input Capture Channel
 * ============================================================
 *  Physical requirement:
 *    Connect FPB-RA2T1 P105 (GPTIO0A_Master / GPT ch0 GTIOCA)
 *    to P103 (GPT ch3 GTIOCA) with a jumper wire before running
 *    any duty-cycle capture test.
 * ============================================================ */

/* Measurement channel — must not overlap with three-phase ch 0/1/2 */
#define DUTY_CAP_GPT_CH             (3U)

/* GTCCR[] array indices ---------------------------------------- */
#define COMP_PWM_PRV_GTCCRA         (0U)   /* rising  edge capture */
#define COMP_PWM_PRV_GTCCRB         (1U)   /* falling edge capture */
/* COMP_PWM_PRV_GTCCRD already defined elsewhere as (3U)          */

/* Tolerance in timer counts (±1 for dead-time edge rounding)     */
#define DUTY_CAP_TOLERANCE_COUNTS   (1U)

/* Poll timeout — ~100 000 iterations ≈ several PWM periods       */
#define DUTY_CAP_POLL_TIMEOUT       (100000U)

/* Base address stride between consecutive GPT channels on RA2T1  */
#define GPT_CH_REG_STRIDE  ((uint32_t)((uintptr_t)R_GPT1 - (uintptr_t)R_GPT0))

2 — Static instances for the capture channel

/* -----------------------------------------------------------------------
 *  GPT ch3 — free-running up-counter with GTIOCA input capture
 *  (opened/closed around each measurement, never left running)
 * ----------------------------------------------------------------------- */
static gpt_instance_ctrl_t g_duty_cap_ctrl;

/* Extended cfg: capture A on rising, capture B on falling of GTIOCA pin  */
static const gpt_extended_cfg_t g_duty_cap_ext_cfg =
{
    .gtioca             = { .output_enabled = false,
                            .stop_level     = GPT_PIN_LEVEL_LOW },
    .gtiocb             = { .output_enabled = false,
                            .stop_level     = GPT_PIN_LEVEL_LOW },
    .capture_a_source   = GPT_CAPTURE_SOURCE_GTIOCA_RISING,   /* → GTCCRA */
    .capture_b_source   = GPT_CAPTURE_SOURCE_GTIOCA_FALLING,  /* → GTCCRB */
    .dead_time_count_up = 0U,
    .dead_time_count_down = 0U,
    .adc_trigger        = GPT_ADC_TRIGGER_NONE,
    .start_source       = GPT_SOURCE_NONE,
    .stop_source        = GPT_SOURCE_NONE,
    .clear_source       = GPT_SOURCE_NONE,
    .count_up_source    = GPT_SOURCE_NONE,
    .count_down_source  = GPT_SOURCE_NONE,
};

static const timer_cfg_t g_duty_cap_timer_cfg =
{
    .mode              = TIMER_MODE_PERIODIC,   /* free-running up-counter  */
    .period_counts     = UINT32_MAX,            /* wrap only after ~13 min  */
    .duty_cycle_counts = 0U,
    .source_div        = TIMER_SOURCE_DIV_1,    /* same clock as PWM chans  */
    .channel           = DUTY_CAP_GPT_CH,
    .p_callback        = NULL,
    .p_context         = NULL,
    .p_extend          = &g_duty_cap_ext_cfg,
    .cycle_end_ipl     = BSP_IRQ_DISABLED,
    .capture_a_ipl     = BSP_IRQ_DISABLED,      /* polled, not IRQ-driven   */
    .capture_b_ipl     = BSP_IRQ_DISABLED,
};

3 — Helper functions

/* -----------------------------------------------------------------------
 * @brief  Open and start the capture GPT channel.
 *         Call BEFORE starting the three-phase timer so the counter is
 *         already running when the first PWM edge arrives.
 * @return FSP_SUCCESS or FSP error code.
 * ----------------------------------------------------------------------- */
static fsp_err_t duty_cap_open (void)
{
    fsp_err_t err = R_GPT_Open(&g_duty_cap_ctrl, &g_duty_cap_timer_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* Clear any stale capture flags before arming                         */
    R_GPT0_Type * p_reg =
        (R_GPT0_Type *)((uintptr_t)R_GPT0 + DUTY_CAP_GPT_CH * GPT_CH_REG_STRIDE);
    p_reg->GTST = 0U;   /* write 0 to clear all status flags               */

    return R_GPT_Start(&g_duty_cap_ctrl);
}

/* -----------------------------------------------------------------------
 * @brief  Close the capture GPT channel and release the resource.
 * ----------------------------------------------------------------------- */
static void duty_cap_close (void)
{
    (void)R_GPT_Close(&g_duty_cap_ctrl);
}

/* -----------------------------------------------------------------------
 * @brief  Wait for one complete high-pulse on GTIOCA of the capture
 *         channel, then return the measured high-time in timer counts.
 *
 *         Capture registers used:
 *           GTCCRA (index 0) — latched at rising  edge of GTIOCA input
 *           GTCCRB (index 1) — latched at falling edge of GTIOCA input
 *
 * @param[out] p_measured_counts   High-time measured in timer counts.
 * @return true  if both edges captured within timeout.
 * @return false if timeout expired before both edges arrived.
 * ----------------------------------------------------------------------- */
static bool duty_cap_measure (uint32_t * const p_measured_counts)
{
    R_GPT0_Type * const p_reg =
        (R_GPT0_Type *)((uintptr_t)R_GPT0 + DUTY_CAP_GPT_CH * GPT_CH_REG_STRIDE);

    /* ------------------------------------------------------------------ */
    /* Step 1: Wait for rising-edge capture (GTST.TCFSA set by hardware)  */
    /* ------------------------------------------------------------------ */
    uint32_t timeout = DUTY_CAP_POLL_TIMEOUT;
    while (!(p_reg->GTST & R_GPT0_GTST_TCFSA_Msk))
    {
        if (0U == timeout--)
        {
            return false;   /* rising edge never came — check jumper wire  */
        }
    }
    uint32_t t_rise = p_reg->GTCCR[COMP_PWM_PRV_GTCCRA];  /* latch value  */

    /* Clear TCFSA so the next rising edge does not confuse TCFSB polling  */
    p_reg->GTST &= ~R_GPT0_GTST_TCFSA_Msk;

    /* ------------------------------------------------------------------ */
    /* Step 2: Wait for the immediately following falling-edge capture     */
    /* ------------------------------------------------------------------ */
    timeout = DUTY_CAP_POLL_TIMEOUT;
    while (!(p_reg->GTST & R_GPT0_GTST_TCFSB_Msk))
    {
        if (0U == timeout--)
        {
            return false;   /* falling edge never came                     */
        }
    }
    uint32_t t_fall = p_reg->GTCCR[COMP_PWM_PRV_GTCCRB]; /* latch value   */

    /* ------------------------------------------------------------------ */
    /* Step 3: Compute high-time, handling 32-bit counter wrap-around      */
    /* ------------------------------------------------------------------ */
    *p_measured_counts = (t_fall >= t_rise)
                         ? (t_fall - t_rise)
                         : (UINT32_MAX - t_rise + t_fall + 1U);

    return true;
}

/* -----------------------------------------------------------------------
 * @brief  Compare measured high-time against duty_cycle_counts.
 *
 *  In complementary PWM (triangle wave), for the master channel (ch U):
 *    high_time = GTPR - GTCCRA
 *              = period_counts - duty_cycle_counts
 *
 *  This function computes that expected value and checks the measurement
 *  falls within ±DUTY_CAP_TOLERANCE_COUNTS.
 *
 * @param[in] measured      High-time returned by duty_cap_measure().
 * @param[in] period_counts GTPR value of the master channel.
 * @param[in] duty_counts   duty_cycle_counts set in the timer config.
 * @return true if within tolerance.
 * ----------------------------------------------------------------------- */
static bool duty_cap_verify (uint32_t measured,
                              uint32_t period_counts,
                              uint32_t duty_counts)
{
    /* Expected high-time in the complementary triangle-wave              */
    uint32_t expected = period_counts - duty_counts;

    return (measured >= (expected - DUTY_CAP_TOLERANCE_COUNTS)) &&
           (measured <= (expected + DUTY_CAP_TOLERANCE_COUNTS));
}

4 — Updated comp_pwm_test_REQ_OM_01

static void comp_pwm_test_REQ_OM_01 (void)
{
    test_setup_config(TIMER_MODE_COMPLEMENTARY_PWM_MODE1,
                      THREE_PHASE_BUFFER_MODE_SINGLE,
                      DEAD_TIME_TEST_COUNTS);

    /* ------------------------------------------------------------------ */
    /* Open capture channel FIRST so its counter is running before the    */
    /* first PWM edge arrives.                                            */
    /* ------------------------------------------------------------------ */
    fsp_err_t cap_err = duty_cap_open();
    bool pass = (FSP_SUCCESS == cap_err);

    /* ------------------------------------------------------------------ */
    /* Open and start the three-phase complementary PWM channels          */
    /* ------------------------------------------------------------------ */
    fsp_err_t err = test_open_three_phase();
    pass &= (FSP_SUCCESS == err);

    if (pass)
    {
        err = R_GPT_THREE_PHASE_Start(&g_three_phase_comp_pwm_ctrl_test);
        pass &= (FSP_SUCCESS == err);

        /* -------------------------------------------------------------- */
        /* Verify single buffer GTBER2 config on all three channels       */
        /* -------------------------------------------------------------- */
        for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U;
             ch <= THREE_PHASE_CHANNEL_W; ch++)
        {
            pass &= verify_gtber2_single_buffer(&g_three_phase_comp_pwm_ctrl_test, ch);

            /* Verify GTCCRD is initialized to the configured duty counts */
            uint32_t gtccrd =
                g_three_phase_comp_pwm_ctrl_test.p_reg[ch]->GTCCR[COMP_PWM_PRV_GTCCRD];
            pass &= (gtccrd == p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts);
        }

        /* -------------------------------------------------------------- */
        /* Verify GTCR.MD = 0xC (Complementary PWM Mode 1)               */
        /* -------------------------------------------------------------- */
        uint32_t gtcr_md =
            (g_three_phase_comp_pwm_ctrl_test.p_reg[THREE_PHASE_CHANNEL_U]->GTCR
             >> R_GPT0_GTCR_MD_Pos) & 0xFU;
        pass &= (gtcr_md == 0xCU);

        /* -------------------------------------------------------------- */
        /* Duty-cycle capture: measure GPTIO0A_Master high-time           */
        /*                                                                */
        /*  Expected relationship (triangle wave, channel U):             */
        /*    high_time = period_counts - duty_cycle_counts               */
        /*                                                                */
        /*  Physical requirement:                                         */
        /*    Jumper P105 (GPT ch0 GTIOCA) → P103 (GPT ch3 GTIOCA)      */
        /* -------------------------------------------------------------- */
        uint32_t measured_counts = 0U;
        bool cap_ok = duty_cap_measure(&measured_counts);
        pass &= cap_ok;

        if (cap_ok)
        {
            /* Retrieve period and duty from master channel U config      */
            const timer_cfg_t * const p_master_cfg =
                p_cfg->p_timer_instance[THREE_PHASE_CHANNEL_U]->p_cfg;

            pass &= duty_cap_verify(measured_counts,
                                    p_master_cfg->period_counts,
                                    p_master_cfg->duty_cycle_counts);
        }

        test_close_three_phase();
    }

    duty_cap_close();

    /* ------------------------------------------------------------------ */
    /* Report                                                              */
    /* ------------------------------------------------------------------ */
    test_report("REQ-OM-01", "Operating mode 1 (crest transfer)", pass,
                pass ? "Mode 1 opened, GTBER2 single, GTCR.MD=0xC, "
                       "duty-cap within tolerance"
                     : "Mode 1 open, register, or duty-cap verification failed");
}

Call-flow summary
comp_pwm_test_REQ_OM_01()
│
├── duty_cap_open()          ← GPT ch3: free-running counter starts
│     R_GPT_Open()              capture_a = RISING  → GTCCRA
│     R_GPT_Start()             capture_b = FALLING → GTCCRB
│
├── test_open_three_phase()  ← GPT ch0/1/2: Mode 1 opened
├── R_GPT_THREE_PHASE_Start()  GPTIO0A_Master begins toggling
│
├── verify_gtber2 / GTCR.MD  ← register-level checks (existing)
│
├── duty_cap_measure()       ← polls GTST.TCFSA → reads GTCCRA (t_rise)
│     wait TCFSA set               GTST.TCFSB → reads GTCCRB (t_fall)
│     wait TCFSB set          returns (t_fall - t_rise) with wrap-guard
│
├── duty_cap_verify()        ← checks: measured ≈ (period - duty_counts)
│
├── test_close_three_phase()
└── duty_cap_close()



GTIOC0A_Master
GTIOC0B_Master

GTIOC0A_Slave1
GTIOC0B_Slave1

GTIOC0A_Slave2
GTIOC0B_Slave2
 PulseView (64-bit)
GTCPPO0

GTIOC3A_Counting

link tải: https://sigrok.org/wiki/Downloads
cài PulseView (64-bit) ->Driver: fx2lafw (generic driver for FX2 based LAs) -> "Scan for devices using driver above"
Chọn sample rate: 24MHz (max của thiết bị)
Chọn số samples: 1M–10M tuỳ nhu cầu
Nhấn nút Run (tam giác ▶) → bắt đầu capture waveform

Link Zadig: https://zadig.akeo.ie
Options → List All Devices -> logic -> Chọn driver WinUSB -> Nhấn nút "Install WCID Driver" 