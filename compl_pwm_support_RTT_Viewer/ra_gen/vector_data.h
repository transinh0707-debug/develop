/* generated vector header file - do not edit */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
        extern "C" {
        #endif
/* Number of interrupts allocated */
#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (2)
#endif
/* ISR prototypes */
void gpt_capture_compare_a_isr(void);
void gpt_capture_compare_b_isr(void);

/* Vector table allocations */
#define VECTOR_NUMBER_GPT3_CAPTURE_COMPARE_A ((IRQn_Type) 4) /* GPT3 CAPTURE COMPARE A (Capture/Compare match A) */
#define GPT3_CAPTURE_COMPARE_A_IRQn          ((IRQn_Type) 4) /* GPT3 CAPTURE COMPARE A (Capture/Compare match A) */
#define VECTOR_NUMBER_GPT3_CAPTURE_COMPARE_B ((IRQn_Type) 5) /* GPT3 CAPTURE COMPARE B (Capture/Compare match B) */
#define GPT3_CAPTURE_COMPARE_B_IRQn          ((IRQn_Type) 5) /* GPT3 CAPTURE COMPARE B (Capture/Compare match B) */
/* The number of entries required for the ICU vector table. */
#define BSP_ICU_VECTOR_NUM_ENTRIES (6)

#ifdef __cplusplus
        }
        #endif
#endif /* VECTOR_DATA_H */
