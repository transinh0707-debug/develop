/* ${REA_DISCLAIMER_PLACEHOLDER} */

#ifndef R_GPT_TEST_DATA_H
#define R_GPT_TEST_DATA_H

/*******************************************************************************************************************//**
 * @addtogroup GPT_TESTS
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include <stdint.h>
#include <string.h>
#include "unity_fixture.h"
#include "r_gpt.h"
#include "test_cfg.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* define in case unsupported */
#ifndef BSP_FEATURE_GPT_GPTEH_CHANNEL_MASK
 #define BSP_FEATURE_GPT_GPTEH_CHANNEL_MASK              (0)
#endif
#ifndef BSP_FEATURE_GPT_GPTE_CHANNEL_MASK
 #define BSP_FEATURE_GPT_GPTE_CHANNEL_MASK               (0)
#endif
#ifndef BSP_FEATURE_GPT_GTDVU_CHANNEL_MASK
 #define BSP_FEATURE_GPT_GTDVU_CHANNEL_MASK              (0)
#endif
#ifndef BSP_FEATURE_GPT_AD_DIRECT_START_CHANNEL_MASK
 #define BSP_FEATURE_GPT_AD_DIRECT_START_CHANNEL_MASK    (0)
#endif
#ifndef BSP_FEATURE_GPT_EVENT_COUNT_CHANNEL_MASK
 #define BSP_FEATURE_GPT_EVENT_COUNT_CHANNEL_MASK        (0)
#endif

/***********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
extern const gpt_extended_cfg_t g_gpt_timer_ext;
extern const gpt_extended_cfg_t g_gpt_timer_ext_off;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public function prototypes
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @} (end addtogroup GPT_TESTS)
 **********************************************************************************************************************/

#endif
