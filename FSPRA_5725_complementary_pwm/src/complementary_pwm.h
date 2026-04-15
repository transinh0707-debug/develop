 /***********************************************************************************
 * File name    : complementary_pwm.h
 * Description  : Contains macros and function declarations for Complementary PWM modes.
 * Implements support for GPT Complementary PWM Modes 1, 2, 3, 4 per RA2T1
 * User's Manual sections 20.3.3.7 and 20.3.3.8
*************************************************************************************/

#ifndef COMPLEMENTARY_PWM_H_
#define COMPLEMENTARY_PWM_H_

#include "common_utils.h"
#include "hal_data.h"

 /***********************************************************************************
  * Macro Definitions for Complementary PWM Mode
*************************************************************************************/
/* Menu option indices for complementary PWM modes */
#define COMP_PWM_MODE1_TIMER            (7U)    /* Menu option 4: Complementary PWM Mode 1 */
#define COMP_PWM_MODE2_TIMER            (8U)    /* Menu option 5: Complementary PWM Mode 2 */
#define COMP_PWM_MODE3_TIMER            (9U)    /* Menu option 6: Complementary PWM Mode 3 */
#define COMP_PWM_MODE4_TIMER            (10U)    /* Menu option 7: Complementary PWM Mode 4 */

/* Default dead time in timer counts (adjust per application requirement) */
#define COMP_PWM_DEFAULT_DEAD_TIME      (0x0100U)

/* Dead timer constrains for 16-bit GPT timer */
#define COMP_PWM_MAX_DEAD_TIME          (0x0FFFU)   /* Maximum dead time value for 16-bit timer*/
#define COMP_PWM_MIN_DEAD_TIME          (0x0001U)   /* Minimum dead time value (must be > 0) */

/* Default PWM period for complementary mode (triangle wave cycle) */
#define COMP_PWM_DEFAULT_PERIOD         (0x2000U)

#if defined (BOARD_RA2T1_FPB) || defined (BOARD_RA6T2_EK) || defined (BOARD_RA8T2)
#define GPT_MAX_PERIOD_COUNT            (0XFFFF)        /* Max period count for 16-bit Timer */
#else
#define GPT_MAX_PERIOD_COUNT            (0XFFFFFFFF)    /* Max period count for 32-bit Timer */
#endif

/* Duty cycle range for complementary PWM (from 0% to 100%) */
#define COMP_PWM_MAX_DUTY               (100U)
#define COMP_PWM_MIN_DUTY               (0U)

/* Three-phase output channel indices*/
#define COMP_PWM_PHASE_U                (0U)    /* U-phase: Master channel (ch0) */
#define COMP_PWM_PHASE_V                (1U)    /* V-phase: Slave channel1 (ch1) */
#define COMP_PWM_PHASE_W                (2U)    /* W-phase: Slave channel2 (ch2) */
#define COMP_PWM_NUM_PHASES             (3U)    /* Total number of phases */

/* Size of buffer for RTT input data */
#define BUF_SIZE                        (16U)

#define RESET_VALUE             (0x00)

/* GPT timer pin for board */
#if defined (BOARD_RA2T1_FPB) || defined (BOARD_RA6T2_EK) || defined (BOARD_RA8T2)
#define TIMER_PIN                       (GPT_IO_PIN_GTIOCA)
#else
#define TIMER_PIN                       (GPT_IO_PIN_GTIOCA)
#endif



 /***********************************************************************************
 * Configuration structure for Complementary PWM
*************************************************************************************/
typedef struct st_comp_pwm_cfg
{
    uint8_t  mode;                   /* Active complementary PWM mode (1-4) */
    uint16_t dead_time;             /* Dead timer value in timer counts (GTDVU) */
    uint16_t period;                /* PWM cycle period (GTPR) */
    uint16_t duty_u;                /* U-phase duty (compare match value GTCCRA ch0) */
    uint16_t duty_v;                /* V-phase duty (compare match value GTCCRA ch1) */
    uint16_t duty_w;                /* W-phase duty (compare match value GTCCRA ch2) */
    bool     double_buffer;             /* Enable double buffer (mode 3 only: GTBER2.CP3DB) */
    bool     is_running;                /* Flag: complementary PWM is currently running */
} comp_pwm_cfg_t;

/***********************************************************************************
* Function prototypes
*************************************************************************************/
/* Process user inputs for complementary PWM mode selections and parameters */
void comp_pwm_process_input(void);

/* Initialize GPT channels for complementary PWM operation */
fsp_err_t comp_pwm_init(void);

/* Start Complementary PWM output on all 3 channels */
fsp_err_t comp_pwm_start(void);

/* Set dead time value for complementary PWM */
fsp_err_t comp_pwm_set_dead_time(uint16_t dead_time_counts);

/* Set three-phase duty cycles for U, V, W phases */
fsp_err_t comp_pwm_set_duty_3phase(uint8_t duty_u, uint8_t duty_v, uint8_t duty_w);

/* Stop Complementary PWM output on all 3 channels */
void comp_pwm_stop(void);

/* Get the current complementary PWM configuration */
const comp_pwm_cfg_t * comp_pwm_get_config(void);

/* Print the complementary PWM sub-menu */
void print_comp_pwm_submenu(void);

/* Print the complementary PWM menu */
void print_comp_pwm_menu(void);

#endif /* COMPLEMENTARY_PWM_H_ */
