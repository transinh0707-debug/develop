/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @addtogroup GPT_TESTS
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "test_cfg.h"
#include "bsp_api.h"
#include "unity_fixture.h"
#ifdef TEST_IF_TIMER
 #include "../../!test/timer/r_timer_tests_runner.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Generated Code
 **********************************************************************************************************************/
void RunAllR_GPTTests(void);

TEST_GROUP_RUNNER(R_GPT_TG1)
{
    /** No test cases specific to HAL layer. */
    RUN_TEST_CASE(R_GPT_TG1, TC_EventCountUnsupported)
    RUN_TEST_CASE(R_GPT_TG1, TC_EventCountGtetrg);
    RUN_TEST_CASE(R_GPT_TG1, TC_EventCountGtioc);
    RUN_TEST_CASE(R_GPT_TG1, TC_EventCountElc);
    RUN_TEST_CASE(R_GPT_TG1, TC_CountDownElc);
    RUN_TEST_CASE(R_GPT_TG1, TC_StartExternalSource);
    RUN_TEST_CASE(R_GPT_TG1, TC_StopExternalSource);
    RUN_TEST_CASE(R_GPT_TG1, TC_ClearExternalSource);
    RUN_TEST_CASE(R_GPT_TG1, TC_GTIOCPolarity);
}
TEST_GROUP_RUNNER(R_GPT_TG2)
{
    RUN_TEST_CASE(R_GPT_TG2, TC_SymmetricPwm);
    RUN_TEST_CASE(R_GPT_TG2, TC_AsymmetricPwm);
    RUN_TEST_CASE(R_GPT_TG2, TC_OneShotPulseOutput);
    RUN_TEST_CASE(R_GPT_TG2, TC_DeadTime);
    RUN_TEST_CASE(R_GPT_TG2, TC_InterruptSkip);
    RUN_TEST_CASE(R_GPT_TG2, TC_AdcTrigger);
    RUN_TEST_CASE(R_GPT_TG2, TC_AdcTriggerUpdate);
    RUN_TEST_CASE(R_GPT_TG2, TC_Poeg);
    RUN_TEST_CASE(R_GPT_TG2, TC_OutputDisable);
    RUN_TEST_CASE(R_GPT_TG2, TC_OutputEnable);
    RUN_TEST_CASE(R_GPT_TG2, TC_CounterSet);
    RUN_TEST_CASE(R_GPT_TG2, TC_CustomWaveform);
    RUN_TEST_CASE(R_GPT_TG2, TC_AsymmetricTriangleMode3);
    RUN_TEST_CASE(R_GPT_TG2, TC_EventCountUnsupported_TrianglePWM);
    RUN_TEST_CASE(R_GPT_TG2, TC_ComplementaryPWM);
    RUN_TEST_CASE(R_GPT_TG2, TC_PwmOutputDelayParameterChecking);
    RUN_TEST_CASE(R_GPT_TG2, TC_PwmOutputDelay);
    RUN_TEST_CASE(R_GPT_TG2, TC_PwmOutputDelayManualTest);
}
void RunAllR_GPTTests (void)
{
    RUN_TEST_GROUP(R_GPT_TG1);
    RUN_TEST_GROUP(R_GPT_TG2);

#ifdef TEST_IF_TIMER
    RunAllDRV_TIMERTests();
#endif
}

/*******************************************************************************************************************//**
 * @} (end addtogroup GPT_TESTS)
 **********************************************************************************************************************/
