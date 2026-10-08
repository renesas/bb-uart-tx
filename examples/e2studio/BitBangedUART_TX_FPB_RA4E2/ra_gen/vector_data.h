/* generated vector header file - do not edit
 *
 * Enables the single interrupt this project needs: GPT0 counter overflow
 * (the Bit-Banged UART TX PES's bit-period tick). Derived with high
 * confidence, not guessed: BSP_PRV_VECT_ENUM(event, group) — see
 * ra/fsp/src/bsp/mcu/ra4e2/bsp_elc.h — expands to plain "ELC_ ## event"; the
 * "group" parameter is decorative only and has no effect on the compiled
 * value, so there is no IELSR group-slot allocation to get wrong for a
 * project with exactly one interrupt at index 0. Cross-checked against a
 * real RASC-generated FPB-RA4E2 bare-metal + GPT project's vector_data
 * pattern (VSCode_FPB_RA4E2_Baremetal reference; that project has this
 * interrupt disabled by default, so its BSP_ICU_VECTOR_NUM_ENTRIES stays 0 —
 * this file is what it becomes once the interrupt is enabled, matching the
 * general pattern also seen in RTC_EVENT_LOGGER_FPB_RA4E2's vector_data.c). */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
extern "C" {
#endif
/* Number of interrupts allocated */
#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (1)
#endif
/* ISR prototypes */
void gpt_counter_overflow_isr(void);

/* Vector table allocations */
#define VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW ((IRQn_Type) 0) /* GPT0 COUNTER OVERFLOW (Overflow) */
#define GPT0_COUNTER_OVERFLOW_IRQn          ((IRQn_Type) 0) /* GPT0 COUNTER OVERFLOW (Overflow) */
/* The number of entries required for the ICU vector table. */
#define BSP_ICU_VECTOR_NUM_ENTRIES (1)

#ifdef __cplusplus
}
#endif
#endif /* VECTOR_DATA_H */
