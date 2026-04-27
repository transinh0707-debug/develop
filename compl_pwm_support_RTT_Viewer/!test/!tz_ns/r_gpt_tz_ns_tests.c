/* ${REA_DISCLAIMER_PLACEHOLDER} */

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "r_gpt.h"
#include "../!tz_s/r_gpt_tz_guard.h"
#include "unity_fixture.h"
#include "r_ioport.h"

/*******************************************************************************************************************//**
 * @addtogroup GPT_TESTS
 *
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#if defined(BSP_MCU_GROUP_RA8D1) | defined(BSP_MCU_GROUP_RA8M2)
 #define GPT_TZ_TEST_PSARE    (0x0000030EU)
#elif defined(BSP_MCU_GROUP_RA6T3) || defined(BSP_MCU_GROUP_RA4L1)
 #define GPT_TZ_TEST_PSARE    (0x03FFFFFFU)
#else
 #define GPT_TZ_TEST_PSARE    (0x003FFFFFU)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
void user_gpt_callback(timer_callback_args_t * p_args);

timer_callback_args_t g_timer_callback_args;

volatile bool g_timer_overflow = false;

void user_gpt_callback (timer_callback_args_t * p_args)
{
    /* At least one element of p_args must be accessed to ensure the non-secure application can access the callback
     * arguments. */
    if (TIMER_EVENT_CYCLE_END == p_args->event)
    {
        g_timer_overflow = true;
    }
}

/* Define test groups. */
TEST_GROUP(R_GPT_TZ_TG1);

/*******************************************************************************************************************//**
 * @brief   TEST_SETUP
 **********************************************************************************************************************/
TEST_SETUP(R_GPT_TZ_TG1)
{
}

/*******************************************************************************************************************//**
 * @brief   TEST_TEAR_DOWN
 **********************************************************************************************************************/
TEST_TEAR_DOWN(R_GPT_TZ_TG1)
{
}

/**
 * @verify{gpt_tz_callback} A secure callback is registered and called from the secure module.
 */
TEST(R_GPT_TZ_TG1, TC_Callback_Secure)
{
    TEST_ASSERT_EQUAL(FSP_SUCCESS, gpt_test_secure_callback());
}

/**
 * @req{gpt_tz_callback,SWFLEX-963,R_GPT_Open} Supplying a non-secure callback shall be supported.
 * @verify{gpt_tz_callback} A non-secure callback is registered with an NSC callbackSet function.
 */
TEST(R_GPT_TZ_TG1, TC_Callback)
{
    /* Verify PSAR is set as expected. */
    TEST_ASSERT_EQUAL(GPT_TZ_TEST_PSARE, R_PSCU->PSARE);

    /* Open timer */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, g_gpt_test_open_guard(NULL, NULL));

    /* Check that p_callback_memory is required when setting a non-secure callback */
    TEST_ASSERT_EQUAL(FSP_ERR_ASSERTION, g_gpt_test_callback_set_guard(NULL, user_gpt_callback, NULL, NULL));

    /* Set the callback and callback memory, then start the timer */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      g_gpt_test_callback_set_guard(NULL, user_gpt_callback, NULL, &g_timer_callback_args));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, g_gpt_test_start_guard(NULL));

    /** Wait for compare match. */
    uint32_t timeout = UINT32_MAX / 4;
    while ((false == g_timer_overflow) && (--timeout > 0))
    {
        ;
    }

    TEST_ASSERT_TRUE(g_timer_overflow);
}

/** @} (end addtogroup GPT_TESTS) */
