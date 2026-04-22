/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @defgroup GPT_TESTS GPT Tests
 * @ingroup RENESAS_TESTS
 *
 * # Test Specifications
 *
 * ## R_GPT
 *
 * ## Related Drivers
 * The GPT driver test does not yet support ADC_B.
 * Therefore, for products with ADC_B, please remove ADC from the related driver.
 *
 * 1. CGC
 * 2. AGT
 * 3. ELC
 * 4. IOPORT
 * 5. ADC(If ADC_B is not implemented.)
 *
 * ## Required Peripherals
 *
 * 1. GPT : 2ch
 * 2. ADC : 1ch
 *
 * ## Required Hardware Setup
 * "3." is required only if the GTIO is not dual-use with the GTETRG.
 * 1. Connect GTIOn_Apin(n = TEST_CHANNEL) and GTIOm_Apin(m = TEST_INPUT_CAPTURE_CHANNEL).
 * 2. Connect GTIOn_Bpin(n = TEST_CHANNEL) and GTIOm_Bpin(m = TEST_INPUT_CAPTURE_CHANNEL).
 * 3. Connect GTETRGpin and IOpin.
 *
 * ## Common Test Code Summary
 *
 * In the test, events are generated and counted using GPT's event counter function.
 * If the number of events and counts are the same, the test passes.
 *
 * # Hardware Setup
 *
 * ## R_GPT
 *
 * Below is the "Required Hardware Setup" applicable to each test board.
 *
 * ## Hardware Setup for r_gpt  on FPB-RA2E3
 *
 * To set up the board for tests on r_gpt:
 *
 * 1. Connect GPT Unit Channel 5 output terminal (P1.03) on J4 to GPT Unit Channel 7 input terminal (P3.02) on J5.
 * 2. Connect GPT Unit Channel 5 output terminal (P1.02) on J4 to GPT Unit Channel 7 input terminal (P3.01) on J3.
 * 3. Connect P1.11 on J4 to GPT trigger input (P9.13) on J3.
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
#include "r_elc_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ELC_ELSEGRN_STEP1            (0U)                  /* WI = 0, WE = 0, SEG = 0 */
#define ELC_ELSEGRN_STEP2            (0x40U)               /* WI = 0, WE = 1, SEG = 0 */
#define ELC_ELSEGRN_STEP3            (0x41U)               /* WI = 0, WE = 1, SEG = 1 */

#define GPT_TEST_EVENT_COUNT_TODO    (BSP_MCU_GROUP_RA2E3) // TODO: RA2E3 wasn't previously tested for Event Counting and is not currently setup for it. See: FSPRA-2966

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
static gpt_instance_ctrl_t g_gpt_ctrl;

/* Define test groups. */
TEST_GROUP(R_GPT_TG1);

/*******************************************************************************************************************//**
 * Generates an ELC software event
 **********************************************************************************************************************/
static void gpt_test_elc_event_generate (void)
{
    /* Generate an ELC software event. */
    R_ELC->ELSEGR[0].BY = ELC_ELSEGRN_STEP1;
    R_ELC->ELSEGR[0].BY = ELC_ELSEGRN_STEP2;
    R_ELC->ELSEGR[0].BY = ELC_ELSEGRN_STEP3;

    R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
}

/*******************************************************************************************************************//**
 * Test setup function called before every test in this test group.
 **********************************************************************************************************************/
TEST_SETUP(R_GPT_TG1)
{
    /* Configure ELC connection. */
    R_BSP_MODULE_START(FSP_IP_ELC, 0);
    R_ELC->ELCR_b.ELCON = 1U;
    R_ELC->ELSR[ELC_PERIPHERAL_GPT_A].HA = ELC_EVENT_ELC_SOFTWARE_EVENT_0;
}

/*******************************************************************************************************************//**
 * @brief Test tear down function called after every test in this test group.
 **********************************************************************************************************************/
TEST_TEAR_DOWN(R_GPT_TG1)
{
    if (0U != g_gpt_ctrl.open)
    {
        R_GPT_Close(&g_gpt_ctrl);
    }
}

/**
 * @verify{gpt_event_count_gtetrg} An error is returned when an invalid channel is selected for event counting when parameter checking is enabled.
 */
TEST(R_GPT_TG1, TC_EventCountUnsupported)
{
#if !GPT_CFG_PARAM_CHECKING_ENABLE
    TEST_IGNORE_MESSAGE("This test is only run when parameter checking is enabled.");
#elif (BSP_FEATURE_GPT_EVENT_COUNT_CHANNEL_MASK == BSP_PERIPHERAL_GPT_CHANNEL_MASK) || GPT_TEST_EVENT_COUNT_TODO
    TEST_IGNORE_MESSAGE("All channels on this MCU support event counting.");
#else

    /* Open timer in event counting mode, but with an invalid channel. */
    g_timer_test_ram_cfg          = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend   = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend = &g_timer_test_ram_cfg_extend;
 #if !defined(BSP_MCU_GROUP_RA2T1)
    g_timer_test_ram_cfg.channel = (uint8_t) (31 - __CLZ(BSP_PERIPHERAL_GPT_CHANNEL_MASK)); // Pick highest GPT channel
 #endif
    g_timer_test_ram_cfg.cycle_end_irq          = BSP_VECTOR_GPT_INPUT_GTETRG_OVERFLOW;
    g_timer_test_ram_cfg_extend.count_up_source = TIMER_TEST_GPT_GTETRG_COUNT_UP;

    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_MODE, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
#endif
}

/**
 * @req{gpt_event_count_gtetrg,SWFLEX-9,R_GPT_Open} The GPT shall support event counting edges on GTETRG pins.
 *
 * @verify{gpt_event_count_gtetrg} The GPT counter matches the number of GTETRG events generated after each event.
 */
TEST(R_GPT_TG1, TC_EventCountGtetrg)
{
    /* Configure pins. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_GPT_GTETRG_INPUT_PIN,
                                      ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN |
                                       (uint32_t) IOPORT_PERIPHERAL_GPT0)));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN,
                                      ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT)));

    /* POEG must be on to use GTETRG.  Reference Note 1 of Table 23.2 "GPT functions" in the RA6M3 manual R01UH0886EJ0100. */
#if defined(BSP_MCU_GROUP_RA6T2) || defined(BSP_MCU_GROUP_RA6T3) || defined(BSP_MCU_GROUP_RA2T1)
    R_BSP_MODULE_START(FSP_IP_POEG, 1U);
#elif defined(BSP_MCU_GROUP_RA8M1) || defined(BOARD_RA8D1_EVAL)
    R_BSP_MODULE_START(FSP_IP_POEG, 2U);
#else
    R_BSP_MODULE_START(FSP_IP_POEG, 0U);
#endif

    /* Verify pin connection. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN, BSP_IO_LEVEL_HIGH));
    bsp_io_level_t level;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_HIGH, level);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_INPUT_PIN, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_HIGH, level);
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN, BSP_IO_LEVEL_LOW));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_LOW, level);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_INPUT_PIN, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_LOW, level);

    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg          = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend   = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend = &g_timer_test_ram_cfg_extend;
    g_timer_test_ram_cfg.channel  = TIMER_TEST_GPT_GTETRG_CHANNEL;
#if defined(BSP_MCU_GROUP_RA6T2) || defined(BSP_MCU_GROUP_RA2T1)
    g_timer_test_ram_cfg.cycle_end_irq = BSP_VECTOR_GPT_INPUT_GTETRG_OVERFLOW;
#endif
    g_timer_test_ram_cfg_extend.count_up_source = TIMER_TEST_GPT_GTETRG_COUNT_UP;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Count with external trigger. */
    for (uint32_t i = 0U; i < 10; i++)
    {
        /* Toggle the control pin. */
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN, BSP_IO_LEVEL_HIGH));
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_GPT_GTETRG_CONTROL_PIN, BSP_IO_LEVEL_LOW));

        /* Verify the counter incremented. */
        timer_status_t status;
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_StatusGet(&g_gpt_ctrl, &status));
        TEST_ASSERT_EQUAL(i + 1, status.counter);
    }
}

/**
 * @req{gpt_event_count_gtioc,SWFLEX-9,R_GPT_Open} The GPT shall support event counting edges on GTIOC pins.
 *
 * @verify{gpt_event_count_gtioc} The GPT counter matches the number of GTIOC edge events generated after each event.
 */
TEST(R_GPT_TG1, TC_EventCountGtioc)
{
    /* Configure pins. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl,
                                      TIMER_TEST_INPUT_PIN_B,
                                      ((uint32_t) IOPORT_CFG_PERIPHERAL_PIN |
                                       (uint32_t) IOPORT_PERIPHERAL_GPT1)));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_IOPORT_PinCfg(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B,
                                      ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT)));

    /* Verify pin connection. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B, BSP_IO_LEVEL_HIGH));
    bsp_io_level_t level;
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_HIGH, level);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_INPUT_PIN_B, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_HIGH, level);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B, BSP_IO_LEVEL_LOW));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_LOW, level);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinRead(&g_ioport_ctrl, TIMER_TEST_INPUT_PIN_B, &level));
    TEST_ASSERT_EQUAL(BSP_IO_LEVEL_LOW, level);

    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg                        = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend                 = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend               = &g_timer_test_ram_cfg_extend;
    g_timer_test_ram_cfg.channel                = TIMER_TEST_INPUT_CAPTURE_CHANNEL;
    g_timer_test_ram_cfg_extend.count_up_source =
        (gpt_source_t) ((uint32_t) GPT_SOURCE_GTIOCB_RISING_WHILE_GTIOCA_LOW |
                        (uint32_t) GPT_SOURCE_GTIOCB_RISING_WHILE_GTIOCA_HIGH);

    g_timer_test_ram_cfg_extend.gtioca.output_enabled = false;
    g_timer_test_ram_cfg_extend.gtiocb.output_enabled = false;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Count with external trigger. */
    for (uint32_t i = 0U; i < 10; i++)
    {
        /* Toggle the control pin. */
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B, BSP_IO_LEVEL_HIGH));
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_IOPORT_PinWrite(&g_ioport_ctrl, TIMER_TEST_OUTPUT_PIN_B, BSP_IO_LEVEL_LOW));

        /* Verify the counter incremented. */
        timer_status_t status;
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_StatusGet(&g_gpt_ctrl, &status));
        TEST_ASSERT_EQUAL(i + 1, status.counter); // Note: Connecting to the oscilloscope may make this verification fail
    }
}

/**
 * @req{gpt_event_count_elc,SWFLEX-9,R_GPT_Open} The GPT shall support event counting ELC events linked to GPT.
 *
 * @verify{gpt_event_count_elc} The GPT counter matches the number of ELC events generated after each event.
 */
TEST(R_GPT_TG1, TC_EventCountElc)
{
    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg          = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend   = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend = &g_timer_test_ram_cfg_extend;
#if defined(BSP_MCU_GROUP_RA6T2) || defined(BSP_MCU_GROUP_RA2T1)
    g_timer_test_ram_cfg.channel = TIMER_TEST_INPUT_CAPTURE_CHANNEL; // Perform test for ELC trigger on Channel 1 as channel 7 does not support it on RA6T2.
#endif
    g_timer_test_ram_cfg_extend.count_up_source = GPT_SOURCE_GPT_A;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Count with external trigger. */
    for (uint32_t i = 0U; i < 10; i++)
    {
        gpt_test_elc_event_generate();

        /* Verify the counter incremented. */
        timer_status_t status;
        TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_StatusGet(&g_gpt_ctrl, &status));
        TEST_ASSERT_EQUAL(i + 1, status.counter);
    }
}

/**
 * @req{gpt_count_down_external_source,SWFLEX-9,R_GPT_Open} The GPT shall support event counting down with external sources.
 *
 * @verify{gpt_count_down_external_source} The GPT counter counts down after generating an ELC event.
 */
TEST(R_GPT_TG1, TC_CountDownElc)
{
    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg          = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend   = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend = &g_timer_test_ram_cfg_extend;
#if defined(BSP_MCU_GROUP_RA6T2) || defined(BSP_MCU_GROUP_RA2T1)
    g_timer_test_ram_cfg.channel = TIMER_TEST_INPUT_CAPTURE_CHANNEL; // Perform test for ELC trigger on Channel 1 as channel 7 does not support it on RA6T2.
#endif
    g_timer_test_ram_cfg_extend.count_down_source = GPT_SOURCE_GPT_A;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Generate ELC event. */
    gpt_test_elc_event_generate();

    /* Counter underflowed. */
    TEST_ASSERT_EQUAL(g_timer_test_ram_cfg.period_counts - 1U, g_gpt_ctrl.p_reg->GTCNT);
}

/**
 * @req{gpt_start_external_source,SWFLEX-9,R_GPT_Open} The GPT shall starting the timer with external sources.
 *
 * @verify{gpt_start_external_source} The GPT counter starts after generating an ELC event.
 */
TEST(R_GPT_TG1, TC_StartExternalSource)
{
    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg                     = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend              = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend            = &g_timer_test_ram_cfg_extend;
    g_timer_test_ram_cfg_extend.start_source = GPT_SOURCE_GPT_A;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_gpt_ctrl));

    /* GPT not started. */
    TEST_ASSERT_EQUAL(0U, g_gpt_ctrl.p_reg->GTCR_b.CST);

    /* Generate ELC event. */
    gpt_test_elc_event_generate();

    /* GPT started. */
    TEST_ASSERT_EQUAL(1U, g_gpt_ctrl.p_reg->GTCR_b.CST);
}

/**
 * @req{gpt_stop_external_source,SWFLEX-9,R_GPT_Open} The GPT shall stopping the timer with external sources.
 *
 * @verify{gpt_stop_external_source} The GPT counter stops after generating an ELC event.
 */
TEST(R_GPT_TG1, TC_StopExternalSource)
{
    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg                    = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend             = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend           = &g_timer_test_ram_cfg_extend;
    g_timer_test_ram_cfg_extend.stop_source = GPT_SOURCE_GPT_A;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_gpt_ctrl));

    /* GPT started. */
    TEST_ASSERT_EQUAL(1U, g_gpt_ctrl.p_reg->GTCR_b.CST);

    /* Generate ELC event. */
    gpt_test_elc_event_generate();

    /* GPT stopped. */
    TEST_ASSERT_EQUAL(0U, g_gpt_ctrl.p_reg->GTCR_b.CST);
}

/**
 * @req{gpt_clear_external_source,SWFLEX-9,R_GPT_Open} The GPT shall clearing the timer with external sources.
 *
 * @verify{gpt_clear_external_source} The GPT counter is cleared after generating an ELC event.
 */
TEST(R_GPT_TG1, TC_ClearExternalSource)
{
    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg                     = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend              = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend            = &g_timer_test_ram_cfg_extend;
    g_timer_test_ram_cfg_extend.clear_source = GPT_SOURCE_GPT_A;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Enable(&g_gpt_ctrl));

    /* Start GPT. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    /* Stop GPT. */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Stop(&g_gpt_ctrl));

    /* Counter is not 0. */
    TEST_ASSERT_NOT_EQUAL(0U, g_gpt_ctrl.p_reg->GTCNT);

    /* Generate ELC event. */
    gpt_test_elc_event_generate();

    /* Counter is 0. */
    TEST_ASSERT_EQUAL(0U, g_gpt_ctrl.p_reg->GTCNT);
}

/**
 * @req{gpt_GTIOC_polarity_control,FSPRA-4994,R_GPT_Open} The GPT shall support GTIOC input/output polarity control.
 *
 * @verify{gpt_GTIOC_polarity_control} The GPT input/output pin (GTIOCnA/B) can have its polarity inverted.
 */
TEST(R_GPT_TG1, TC_GTIOCPolarity)
{
#if !BSP_FEATURE_GPT_POLARITY_CONTROL_SUPPORTED || !GPT_CFG_GTIOC_POLARITY_TEST
    TEST_IGNORE_MESSAGE("MCU does not support GTIOCnA/B polarity control.");
#else

    /* Open timer in event counting mode. */
    g_timer_test_ram_cfg          = g_timer_test_default_cfg;
    g_timer_test_ram_cfg_extend   = g_gpt_timer_ext;
    g_timer_test_ram_cfg.p_extend = &g_timer_test_ram_cfg_extend;

    g_timer_test_ram_cfg_extend.gtioca_polarity = GPT_GTIOC_POLARITY_INVERTED;
    g_timer_test_ram_cfg_extend.gtiocb_polarity = GPT_GTIOC_POLARITY_INVERTED;

    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));

    /* GPT not started. */
    TEST_ASSERT_EQUAL(0U, g_gpt_ctrl.p_reg->GTCR_b.CST);

    /* Check the AINV and BINV bitfields. */
    TEST_ASSERT(g_gpt_ctrl.p_reg->GTCR_b.AINV);
    TEST_ASSERT(g_gpt_ctrl.p_reg->GTCR_b.BINV);

    R_GPT_Close(&g_gpt_ctrl);
#endif
}

/*******************************************************************************************************************//**
 * @}
 **********************************************************************************************************************/
