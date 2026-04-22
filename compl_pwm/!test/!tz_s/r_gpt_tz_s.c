/* ${REA_DISCLAIMER_PLACEHOLDER} */

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "r_gpt.h"
#include "r_gpt_tz_s_vectors.h"
#include "r_gpt_tz_guard.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define TIMER_TEST_PERIOD_COUNTS    (2000)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
void gpt_secure_callback(timer_callback_args_t * p_args);

volatile bool g_timer_underflow = false;

void gpt_secure_callback (timer_callback_args_t * p_args)
{
    if (TIMER_EVENT_CYCLE_END == p_args->event)
    {
        g_timer_underflow = true;
    }
}

static gpt_instance_ctrl_t g_gpt_test_ctrl;
const gpt_extended_cfg_t   g_gpt_timer_ext_off =
{
    .gtioca              =
    {
        .output_enabled = false,
        .stop_level     = GPT_PIN_LEVEL_LOW,
    },
    .gtiocb              =
    {
        .output_enabled = false,
        .stop_level     = GPT_PIN_LEVEL_HIGH,
    },
    .clear_source        = GPT_SOURCE_NONE,
    .count_down_source   = GPT_SOURCE_NONE,
    .count_up_source     = GPT_SOURCE_NONE,
    .start_source        = GPT_SOURCE_NONE,
    .stop_source         = GPT_SOURCE_NONE,
    .capture_a_irq       = FSP_INVALID_VECTOR,
    .capture_b_irq       = FSP_INVALID_VECTOR,
    .compare_match_c_irq = FSP_INVALID_VECTOR,
    .compare_match_d_irq = FSP_INVALID_VECTOR,
    .compare_match_e_irq = FSP_INVALID_VECTOR,
    .compare_match_f_irq = FSP_INVALID_VECTOR,
};

const timer_cfg_t g_gpt_test_cfg =
{
    .channel           = 0,
    .period_counts     = TIMER_TEST_PERIOD_COUNTS,
    .duty_cycle_counts = TIMER_TEST_PERIOD_COUNTS / 4,
    .source_div        = TIMER_SOURCE_DIV_1,
    .mode              = TIMER_MODE_PERIODIC,
    .p_callback        = NULL,
    .p_context         = 0,
    .p_extend          = &g_gpt_timer_ext_off,
    .cycle_end_ipl     = 2,
    .cycle_end_irq     = VECTOR_NUMBER_TIMER_OVERFLOW,
};

/* Instance structure to use this module. */
const timer_instance_t g_gpt_test =
{
    .p_ctrl = &g_gpt_test_ctrl,
    .p_cfg  = &g_gpt_test_cfg,
    .p_api  = &g_timer_on_gpt
};

BSP_CMSE_NONSECURE_ENTRY fsp_err_t gpt_test_secure_callback ()
{
    fsp_err_t err;

    /* Open timer */
    err = R_GPT_Open(&g_gpt_test_ctrl, &g_gpt_test_cfg);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Set the callback and callback memory, then start the timer */
    err = R_GPT_CallbackSet(&g_gpt_test_ctrl, gpt_secure_callback, NULL, NULL);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    err = R_GPT_Start(&g_gpt_test_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Wait for compare match. */
    uint32_t timeout = UINT32_MAX / 4;
    while ((false == g_timer_underflow) && (--timeout > 0))
    {
        ;
    }

    FSP_ASSERT(g_timer_underflow);

    /* Close timer */
    err = R_GPT_Close(&g_gpt_test_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * Guard Functions
 **********************************************************************************************************************/

BSP_CMSE_NONSECURE_ENTRY fsp_err_t g_gpt_test_open_guard (timer_ctrl_t * const p_ctrl, timer_cfg_t const * const p_cfg)
{
    /* TODO: add your own security checks here */

    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_cfg);

    return R_GPT_Open(&g_gpt_test_ctrl, &g_gpt_test_cfg);
}

BSP_CMSE_NONSECURE_ENTRY fsp_err_t g_gpt_test_start_guard (timer_ctrl_t * const p_ctrl)
{
    /* TODO: add your own security checks here */

    FSP_PARAMETER_NOT_USED(p_ctrl);

    return R_GPT_Start(&g_gpt_test_ctrl);
}

BSP_CMSE_NONSECURE_ENTRY fsp_err_t g_gpt_test_callback_set_guard (timer_ctrl_t * const p_api_ctrl,
                                                                  void (             * p_callback)(
                                                                      timer_callback_args_t *),
                                                                  void * const                  p_context,
                                                                  timer_callback_args_t * const p_callback_memory)
{
    /* Verify all pointers are in non-secure memory. */
    void (* p_callback_checked)(timer_callback_args_t *) =
        (void (*)(timer_callback_args_t *))cmse_check_address_range((void *) p_callback,
                                                                    sizeof(void *),
                                                                    CMSE_AU_NONSECURE);
    FSP_ASSERT(NULL != p_callback_checked);
    timer_callback_args_t * const p_callback_memory_checked = cmse_check_pointed_object(p_callback_memory,
                                                                                        CMSE_AU_NONSECURE);
    FSP_ASSERT(NULL != p_callback_memory_checked);

    /* TODO: add your own security checks here */

    FSP_PARAMETER_NOT_USED(p_api_ctrl);
    FSP_PARAMETER_NOT_USED(p_context);

    return R_GPT_CallbackSet(&g_gpt_test_ctrl, p_callback_checked, p_context, p_callback_memory_checked);
}
