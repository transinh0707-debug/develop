/* ${REA_DISCLAIMER_PLACEHOLDER} */

/*******************************************************************************************************************//**
 * @addtogroup GPT_TESTS
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_gpt_test_data.h"

/***********************************************************************************************************************
 * Macro Definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/
const gpt_extended_cfg_t g_gpt_timer_ext =
{
    .gtioca               =
    {
        .output_enabled = true,
        .stop_level     = GPT_PIN_LEVEL_LOW,
    },
    .gtiocb               =
    {
        .output_enabled = true,
        .stop_level     = GPT_PIN_LEVEL_HIGH,
    },
    .clear_source         = GPT_SOURCE_NONE,
    .count_down_source    = GPT_SOURCE_NONE,
    .count_up_source      = GPT_SOURCE_NONE,
    .start_source         = GPT_SOURCE_NONE,
    .stop_source          = GPT_SOURCE_NONE,
    .capture_a_irq        = FSP_INVALID_VECTOR,
    .capture_b_irq        = FSP_INVALID_VECTOR,
    .compare_match_c_irq  = FSP_INVALID_VECTOR,
    .compare_match_d_irq  = FSP_INVALID_VECTOR,
    .compare_match_e_irq  = FSP_INVALID_VECTOR,
    .compare_match_f_irq  = FSP_INVALID_VECTOR,
    .compare_match_value  = {0,                0,0, 0, 0, 0},
    .compare_match_status = 0U,
    .gtior_setting.gtior  = 0U,
};

const gpt_extended_cfg_t g_gpt_timer_ext_off =
{
    .gtioca               =
    {
        .output_enabled = false,
        .stop_level     = GPT_PIN_LEVEL_LOW,
    },
    .gtiocb               =
    {
        .output_enabled = false,
        .stop_level     = GPT_PIN_LEVEL_HIGH,
    },
    .clear_source         = GPT_SOURCE_NONE,
    .count_down_source    = GPT_SOURCE_NONE,
    .count_up_source      = GPT_SOURCE_NONE,
    .start_source         = GPT_SOURCE_NONE,
    .stop_source          = GPT_SOURCE_NONE,
    .capture_a_irq        = FSP_INVALID_VECTOR,
    .capture_b_irq        = FSP_INVALID_VECTOR,
    .compare_match_c_irq  = FSP_INVALID_VECTOR,
    .compare_match_d_irq  = FSP_INVALID_VECTOR,
    .compare_match_e_irq  = FSP_INVALID_VECTOR,
    .compare_match_f_irq  = FSP_INVALID_VECTOR,
    .compare_match_value  = {0,                0,0, 0, 0, 0},
    .compare_match_status = 0U,
    .gtior_setting.gtior  = 0U,
};

/*******************************************************************************************************************//**
 * @} (end addtogroup GPT_TESTS)
 **********************************************************************************************************************/
