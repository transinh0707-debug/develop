/* ${REA_DISCLAIMER_PLACEHOLDER} */

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_gpt_tz_s_vectors.h"

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_MAX_ENTRIES] BSP_PLACE_IN_SECTION(
    BSP_SECTION_APPLICATION_VECTORS) =
{
    [0] = gpt_counter_overflow_isr,    /* GPT0 */
};

const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_MAX_ENTRIES] =
{
    [0] = BSP_PRV_IELS_ENUM(EVENT_GPT0_COUNTER_OVERFLOW)
};
