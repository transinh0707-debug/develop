

Phân tích Kiến trúc & Cách Implement

1. Ánh xạ 3 Stack FSP → Hardware

e2studio Stack          Hardware Channel       Vai trò
────────────────────────────────────────────────────────
g_timer_master (ch0)  → GPT160  (GPT16n)    Master: chu kỳ, dead time (GTDVU)
g_timer_slave1 (ch1)  → GPT161  (GPT16n+1)  Slave 1: Negative-phase B offset
g_timer_slave2 (ch2)  → GPT162  (GPT16n+2)  Slave 2: Linearity 0%/100%

Lưu ý FSP: Slave 1 & 2 chỉ cần R_GPT_Open() — không R_GPT_Start(). Master start sẽ kéo theo cả hai slave qua cơ chế cascade start bit.

2. Công thức tính Register

// Timer clock = PCLKD (ví dụ 32 MHz)
#define TIMER_CLOCK_HZ    32000000UL

// GTPR (triangle wave period)
// Một chu kỳ PWM = 2 × GTPR counts (đếm lên + đếm xuống)
uint32_t gtpr = (TIMER_CLOCK_HZ / (2 * freq_hz)) - 1;

// GTDVU (dead time — ghi thẳng, KHÔNG buffer)
uint32_t gtdvu = (uint32_t)(dead_time_ns * (TIMER_CLOCK_HZ / 1e9f));

// GTCCRA (compare match point)
uint32_t gtccra = (uint32_t)(duty_percent / 100.0f * gtpr);

// ── 3 vùng GTCCRA ──────────────────────────────────────────
// Vùng 1: 0 < gtccra < gtdvu          → gần 0%, Slave2 tiếp quản
// Vùng 2: gtdvu < gtccra < gtpr-gtdvu → bình thường, Middle section
// Vùng 3: gtpr-gtdvu < gtccra < gtpr  → gần 100%, Slave2 tiếp quản

3. Sự khác nhau giữa Mode 1 → 4

Thuộc tínhMode 1Mode 2Mode 3Mode 4
Initial outputA=L, B=HA=H, B=LA=L, B=HA=H, B=L
Buffer GTCCRASingle (GTCCRC)Single (GTCCRC)Double (GTCCRC→GTCCRE)Double (GTCCRC→GTCCRE)
GTBER2.CP3DB0011
GTBER2.CPADIF0101
Ngõ ra A lúc startLOWHIGHLOWHIGH


4. Cấu trúc Source

src/
├── complementary_pwm.h
└── complementary_pwm.c

complementary_pwm.h:

#ifndef COMPLEMENTARY_PWM_H
#define COMPLEMENTARY_PWM_H

#include "hal_data.h"

/* ── Mode định nghĩa ─────────────────────────────── */
typedef enum {
    COMP_PWM_MODE1 = 1,   /* A start LOW,  single buffer */
    COMP_PWM_MODE2 = 2,   /* A start HIGH, single buffer */
    COMP_PWM_MODE3 = 3,   /* A start LOW,  double buffer */
    COMP_PWM_MODE4 = 4,   /* A start HIGH, double buffer */
} comp_pwm_mode_t;

/* ── Vùng GTCCRA ─────────────────────────────────── */
typedef enum {
    COMP_PWM_REGION_LOW    = 1,  /* 0 < GTCCRA < GTDVU          */
    COMP_PWM_REGION_NORMAL = 2,  /* GTDVU < GTCCRA < GTPR-GTDVU */
    COMP_PWM_REGION_HIGH   = 3,  /* GTPR-GTDVU < GTCCRA < GTPR  */
} comp_pwm_region_t;

/* ── Config struct ───────────────────────────────── */
typedef struct {
    uint32_t            freq_hz;        /* Tần số PWM (Hz)          */
    float               duty_percent;   /* Duty cycle (0.0–100.0)   */
    uint32_t            dead_time_ns;   /* Dead time (nanosecond)   */
    comp_pwm_mode_t     mode;
} comp_pwm_config_t;

/* ── API ─────────────────────────────────────────── */
fsp_err_t comp_pwm_init   (comp_pwm_mode_t mode);
fsp_err_t comp_pwm_start  (void);
fsp_err_t comp_pwm_stop   (void);

fsp_err_t comp_pwm_set_period    (uint32_t freq_hz);
fsp_err_t comp_pwm_set_duty      (float duty_percent);
fsp_err_t comp_pwm_set_dead_time (uint32_t dead_time_ns);
fsp_err_t comp_pwm_set_compare_region (comp_pwm_region_t region);

void      comp_pwm_rtt_menu (void);   /* RTT Viewer test submenu  */

#endif

complementary_pwm.c — Skeleton có chú thích đầy đủ:

#include "complementary_pwm.h"
#include "SEGGER_RTT.h"
#include <stdio.h>

#define TIMER_CLK_HZ    32000000UL
#define GTPR_REG        R_GPT0->GTPR
#define GTDVU_REG       R_GPT0->GTDVU
#define GTCCRA_REG      R_GPT0->GTCCRA
#define GTCCRC_REG      R_GPT0->GTCCRC    /* buffer 1 của GTCCRA */
#define GTCCRE_REG      R_GPT0->GTCCRE    /* buffer 2 — mode 3/4 */
#define GTBER2_REG      R_GPT0->GTBER     /* bit CP3DB, CPADIF    */

/* ── State nội bộ ─────────────────────────────────── */
static uint32_t s_gtpr   = 0;
static uint32_t s_gtdvu  = 0;
static uint32_t s_gtccra = 0;
static comp_pwm_mode_t s_mode = COMP_PWM_MODE1;

/* ────────────────────────────────────────────────────
 * comp_pwm_init()
 * Gọi Open cho cả 3 stack, set GTBER2 theo mode
 * ──────────────────────────────────────────────────── */
fsp_err_t comp_pwm_init(comp_pwm_mode_t mode)
{
    fsp_err_t err;
    s_mode = mode;

    /* 1. Open master — slave tự nhận sync từ master */
    err = R_GPT_Open(&g_timer_master_ctrl, &g_timer_master_cfg);
    if(FSP_SUCCESS != err) return err;

    err = R_GPT_Open(&g_timer_slave1_ctrl, &g_timer_slave1_cfg);
    if(FSP_SUCCESS != err) return err;

    err = R_GPT_Open(&g_timer_slave2_ctrl, &g_timer_slave2_cfg);
    if(FSP_SUCCESS != err) return err;

    /* 2. Set GTBER2 theo mode
     *    CP3DB = 1 → double buffer (mode 3, 4)
     *    CPADIF = 1 → A start HIGH (mode 2, 4)      */
    uint32_t gtber = GTBER2_REG;
    gtber &= ~((1u<<17)|(1u<<16));  /* clear CP3DB, CPADIF */
    if(mode == COMP_PWM_MODE3 || mode == COMP_PWM_MODE4)
        gtber |= (1u<<17);          /* CP3DB = 1 */
    if(mode == COMP_PWM_MODE2 || mode == COMP_PWM_MODE4)
        gtber |= (1u<<16);          /* CPADIF = 1 */
    GTBER2_REG = gtber;

    return FSP_SUCCESS;
}

/* ────────────────────────────────────────────────────
 * comp_pwm_set_period()
 * Chỉ ghi GTPR — slave counter tự scale theo master
 * ──────────────────────────────────────────────────── */
fsp_err_t comp_pwm_set_period(uint32_t freq_hz)
{
    if(freq_hz == 0) return FSP_ERR_INVALID_ARGUMENT;

    s_gtpr = (TIMER_CLK_HZ / (2u * freq_hz)) - 1u;
    GTPR_REG = s_gtpr;

    /* Giữ nguyên duty ratio → update GTCCRA */
    float ratio = (float)s_gtccra / (float)(GTPR_REG + 1u);
    s_gtccra = (uint32_t)(ratio * (s_gtpr + 1u));
    GTCCRA_REG = s_gtccra;
    GTCCRC_REG = s_gtccra;                    /* buffer cũng update */
    if(s_mode >= COMP_PWM_MODE3)
        GTCCRE_REG = s_gtccra;

    return FSP_SUCCESS;
}

/* ────────────────────────────────────────────────────
 * comp_pwm_set_duty()
 * Tính GTCCRA từ duty%, ghi qua buffer (GTCCRC/E)
 * KHÔNG ghi thẳng GTCCRA để tránh glitch mid-cycle
 * ──────────────────────────────────────────────────── */
fsp_err_t comp_pwm_set_duty(float duty_percent)
{
    if(duty_percent < 0.0f || duty_percent > 100.0f)
        return FSP_ERR_INVALID_ARGUMENT;

    s_gtccra = (uint32_t)(duty_percent / 100.0f * (float)s_gtpr);

    /* Clamp: không cho chạm 0 hoặc GTPR */
    if(s_gtccra == 0) s_gtccra = 1;
    if(s_gtccra >= s_gtpr) s_gtccra = s_gtpr - 1;

    GTCCRC_REG = s_gtccra;
    if(s_mode >= COMP_PWM_MODE3)
        GTCCRE_REG = s_gtccra;

    return FSP_SUCCESS;
}

/* ────────────────────────────────────────────────────
 * comp_pwm_set_dead_time()
 * QUAN TRỌNG: Ghi thẳng GTDVU, KHÔNG buffer
 * Chỉ thay đổi khi timer đang dừng hoặc tại điểm
 * đồng bộ an toàn (trough/crest boundary)
 * ──────────────────────────────────────────────────── */
fsp_err_t comp_pwm_set_dead_time(uint32_t dead_time_ns)
{
    uint32_t counts = (uint32_t)((float)dead_time_ns
                                * (TIMER_CLK_HZ / 1e9f));

    /* Validation: dead_time < GTPR/2 */
    if(counts >= s_gtpr / 2u) return FSP_ERR_INVALID_ARGUMENT;

    s_gtdvu = counts;
    GTDVU_REG = s_gtdvu;   /* Non-buffered — trực tiếp */

    return FSP_SUCCESS;
}

/* ────────────────────────────────────────────────────
 * comp_pwm_set_compare_region()
 * Test 3 vùng GTCCRA theo tài liệu:
 *
 * REGION_LOW:    GTCCRA = GTDVU/2
 *   → Slave2 tiếp quản, duty thực tế ≈ 0%
 *   → Kiểm tra linearity tại trough section
 *
 * REGION_NORMAL: GTCCRA = GTPR/2
 *   → Middle section xử lý, hoạt động bình thường
 *
 * REGION_HIGH:   GTCCRA = GTPR - GTDVU/2
 *   → Slave2 tiếp quản, duty thực tế ≈ 100%
 *   → Kiểm tra linearity tại crest section
 * ──────────────────────────────────────────────────── */
fsp_err_t comp_pwm_set_compare_region(comp_pwm_region_t region)
{
    uint32_t new_gtccra;

    switch(region)
    {
        case COMP_PWM_REGION_LOW:
            /* Vùng 1: 0 < GTCCRA < GTDVU */
            new_gtccra = s_gtdvu / 2u;
            if(new_gtccra == 0) new_gtccra = 1;
            break;

        case COMP_PWM_REGION_NORMAL:
            /* Vùng 2: GTDVU < GTCCRA < GTPR-GTDVU */
            new_gtccra = s_gtpr / 2u;
            break;

        case COMP_PWM_REGION_HIGH:
            /* Vùng 3: GTPR-GTDVU < GTCCRA < GTPR */
            new_gtccra = s_gtpr - s_gtdvu / 2u;
            break;

        default:
            return FSP_ERR_INVALID_ARGUMENT;
    }

    s_gtccra = new_gtccra;
    GTCCRC_REG = s_gtccra;
    if(s_mode >= COMP_PWM_MODE3)
        GTCCRE_REG = s_gtccra;

    return FSP_SUCCESS;
}

5. RTT Viewer Test Submenu

void comp_pwm_rtt_menu(void)
{
    SEGGER_RTT_printf(0,
        "\r\n=== Complementary PWM Test ===\r\n"
        " 1. Mode 1 (A-LOW start, single buf)\r\n"
        " 2. Mode 2 (A-HIGH start, single buf)\r\n"
        " 3. Mode 3 (A-LOW start, double buf)\r\n"
        " 4. Mode 4 (A-HIGH start, double buf)\r\n"
        " 5. Set Period (freq Hz)\r\n"
        " 6. Set Duty (%)\r\n"
        " 7. Set Dead Time (ns)\r\n"
        " 8. Compare Region: LOW    (near 0%%)\r\n"
        " 9. Compare Region: NORMAL (middle)\r\n"
        " A. Compare Region: HIGH   (near 100%%)\r\n"
        " 0. Stop & Exit\r\n"
        "> ");

    /* Đọc key và dispatch đến từng handler */
}

6. Lưu ý Critical khi Implement

ĐiểmLý do
Slave 1 & 2 chỉ Open, không StartMaster cascade-start tự động
GTDVU ghi thẳng, không bufferSlave counter phụ thuộc realtime — đã phân tích trước
GTCCRA update qua GTCCRC (và GTCCRE cho mode 3/4)Tránh glitch giữa chu kỳ
Slave 2 ghi cuối cùngCritical ordering rule trong codebase này
Validate: gtdvu < gtpr/2Nếu dead time quá lớn → không còn vùng 
MiddleValidate: gtccra ≠ 0 và ≠ gtprBoundary case gây output không xác định

Kiến trúc Automatic Test Framework

1. Tổng quan cấu trúc

src/
├── complementary_pwm.c/.h          (production code)
└── test/
    ├── comp_pwm_test_framework.h   (framework core)
    ├── comp_pwm_test_framework.c
    ├── comp_pwm_test_runner.c      (main entry point)
    │
    ├── tc_mode/                    (test cases theo mode)
    │   ├── tc_mode1.c
    │   ├── tc_mode2.c
    │   ├── tc_mode3.c
    │   └── tc_mode4.c
    │
    ├── tc_period/                  (test cases period)
    │   └── tc_period.c
    │
    ├── tc_duty/                    (test cases duty)
    │   └── tc_duty.c
    │
    ├── tc_dead_time/               (test cases dead time)
    │   └── tc_dead_time.c
    │
    └── tc_compare_region/          (test cases 3 vùng GTCCRA)
        └── tc_compare_region.c

        2. Test Framework Core
comp_pwm_test_framework.h:

#ifndef COMP_PWM_TEST_FRAMEWORK_H
#define COMP_PWM_TEST_FRAMEWORK_H

#include "hal_data.h"
#include "complementary_pwm.h"
#include "SEGGER_RTT.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* ── Result codes ─────────────────────────────────── */
typedef enum {
    TEST_PASS    = 0,
    TEST_FAIL    = 1,
    TEST_SKIP    = 2,
    TEST_ERROR   = 3,
} test_result_t;

/* ── Một test case ────────────────────────────────── */
typedef struct {
    const char    *name;
    const char    *description;
    test_result_t (*run)(void);
} test_case_t;

/* ── Một test suite ───────────────────────────────── */
typedef struct {
    const char        *suite_name;
    const test_case_t *cases;
    uint32_t           count;
    void             (*setup)(void);      /* gọi trước suite   */
    void             (*teardown)(void);   /* gọi sau suite     */
} test_suite_t;

/* ── Kết quả tổng hợp ─────────────────────────────── */
typedef struct {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
} test_summary_t;

/* ── Tolerance cho floating point ────────────────── */
#define TOLERANCE_FREQ_PCT    2.0f    /* ±2% tần số          */
#define TOLERANCE_DUTY_PCT    2.0f    /* ±2% duty            */
#define TOLERANCE_DT_NS       100.0f  /* ±100ns dead time     */

/* ── Macro assertions ────────────────────────────── */
#define TEST_ASSERT(cond, msg) \
    do { \
        if(!(cond)) { \
            SEGGER_RTT_printf(0, "  [FAIL] %s:%d — %s\r\n", \
                              __func__, __LINE__, msg); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_NEAR(actual, expected, tol, msg) \
    do { \
        float _diff = fabsf((float)(actual) - (float)(expected)); \
        float _pct  = _diff / (float)(expected) * 100.0f; \
        if(_pct > (tol)) { \
            SEGGER_RTT_printf(0, \
                "  [FAIL] %s:%d — %s: got %.2f, exp %.2f (%.1f%%)\r\n", \
                __func__, __LINE__, msg, \
                (float)(actual), (float)(expected), _pct); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_IN_RANGE(val, lo, hi, msg) \
    do { \
        if((val) < (lo) || (val) > (hi)) { \
            SEGGER_RTT_printf(0, \
                "  [FAIL] %s:%d — %s: %lu not in [%lu, %lu]\r\n", \
                __func__, __LINE__, msg, \
                (uint32_t)(val), (uint32_t)(lo), (uint32_t)(hi)); \
            return TEST_FAIL; \
        } \
    } while(0)

/* ── Register read helpers ───────────────────────── */
#define REG_GTPR    R_GPT0->GTPR
#define REG_GTDVU   R_GPT0->GTDVU
#define REG_GTCCRA  R_GPT0->GTCCRA
#define REG_GTCCRC  R_GPT0->GTCCRC
#define REG_GTCCRE  R_GPT0->GTCCRE
#define REG_GTUDC   R_GPT0->GTUDC   /* count direction status */

/* ── Timing helper ───────────────────────────────── */
#define TIMER_CLK_HZ    32000000UL
#define NS_TO_COUNTS(ns)  ((uint32_t)((float)(ns) * (TIMER_CLK_HZ / 1e9f)))
#define COUNTS_TO_NS(c)   ((float)(c) / (TIMER_CLK_HZ / 1e9f))
#define FREQ_TO_GTPR(f)   ((TIMER_CLK_HZ / (2u * (f))) - 1u)

/* ── API ─────────────────────────────────────────── */
test_summary_t test_run_suite  (const test_suite_t *suite);
test_summary_t test_run_all    (void);
void           test_print_summary (const test_summary_t *s);
void           test_delay_ms   (uint32_t ms);

/* Small wait cho register settle */
#define TEST_SETTLE_MS    5

#endif

3. Test Cases — Mode Initialization
tc_mode/tc_mode1.c:

#include "comp_pwm_test_framework.h"

/* ── TC-MODE1-001: Init thành công ───────────────── */
static test_result_t tc_mode1_init_ok(void)
{
    comp_pwm_stop();
    fsp_err_t err = comp_pwm_init(COMP_PWM_MODE1);
    TEST_ASSERT(err == FSP_SUCCESS, "Mode1 init failed");

    /* GTBER2.CP3DB = 0, CPADIF = 0 */
    uint32_t gtber = R_GPT0->GTBER;
    TEST_ASSERT((gtber & (1u<<17)) == 0, "CP3DB should be 0 in mode1");
    TEST_ASSERT((gtber & (1u<<16)) == 0, "CPADIF should be 0 in mode1");

    return TEST_PASS;
}

/* ── TC-MODE1-002: Ngõ ra A bắt đầu LOW ─────────── */
static test_result_t tc_mode1_a_starts_low(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(1000);
    comp_pwm_set_duty(50.0f);
    comp_pwm_set_dead_time(500);
    comp_pwm_start();
    test_delay_ms(1);

    /* Đọc initial output level từ GTIOR register */
    uint32_t gtior = R_GPT0->GTIOR;
    uint8_t gtioa_init = (gtior >> 4) & 0x3;  /* bit [5:4] */
    TEST_ASSERT(gtioa_init == 0, "GTIOCnA initial level should be LOW");

    comp_pwm_stop();
    return TEST_PASS;
}

/* ── TC-MODE1-003: Single buffer — GTCCRC active ─── */
static test_result_t tc_mode1_single_buffer(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(1000);
    comp_pwm_set_duty(50.0f);
    comp_pwm_start();
    test_delay_ms(TEST_SETTLE_MS);

    uint32_t gtccra = REG_GTCCRA;
    uint32_t gtccrc = REG_GTCCRC;

    /* Trong mode 1: GTCCRC = GTCCRA (single buffer active) */
    TEST_ASSERT(gtccra == gtccrc, "GTCCRC must mirror GTCCRA in mode1");

    /* GTCCRE không được dùng → giá trị reset */
    /* (Không assert GTCCRE vì có thể giữ giá trị cũ) */

    comp_pwm_stop();
    return TEST_PASS;
}

static const test_case_t s_mode1_cases[] = {
    {"TC-MODE1-001", "Init mode 1 OK, GTBER correct",    tc_mode1_init_ok},
    {"TC-MODE1-002", "GTIOCnA starts LOW in mode 1",     tc_mode1_a_starts_low},
    {"TC-MODE1-003", "Single buffer GTCCRC active",      tc_mode1_single_buffer},
};

const test_suite_t g_suite_mode1 = {
    .suite_name = "Mode 1 — A-LOW start, single buffer",
    .cases      = s_mode1_cases,
    .count      = sizeof(s_mode1_cases)/sizeof(s_mode1_cases[0]),
    .setup      = NULL,
    .teardown   = comp_pwm_stop,
};

4. Test Cases — Period
tc_period/tc_period.c:

#include "comp_pwm_test_framework.h"

/* Bảng test: [freq_hz, expected_gtpr] */
static const struct { uint32_t freq; uint32_t gtpr; } s_period_table[] = {
    {  500,  31999 },   /* 500 Hz  */
    { 1000,  15999 },   /* 1 kHz   */
    { 5000,   3199 },   /* 5 kHz   */
    {10000,   1599 },   /* 10 kHz  */
    {20000,    799 },   /* 20 kHz  */
};

/* ── TC-PERIOD-001: GTPR tính đúng theo freq ────── */
static test_result_t tc_period_register_value(void)
{
    comp_pwm_init(COMP_PWM_MODE1);

    for(uint32_t i = 0; i < sizeof(s_period_table)/sizeof(s_period_table[0]); i++)
    {
        comp_pwm_set_period(s_period_table[i].freq);
        test_delay_ms(TEST_SETTLE_MS);

        uint32_t actual = REG_GTPR;
        TEST_ASSERT_NEAR(actual,
                         s_period_table[i].gtpr,
                         TOLERANCE_FREQ_PCT,
                         "GTPR mismatch");
    }
    return TEST_PASS;
}

/* ── TC-PERIOD-002: Duty ratio bảo toàn sau khi đổi period ── */
static test_result_t tc_period_duty_preserved(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(1000);
    comp_pwm_set_duty(40.0f);
    comp_pwm_start();
    test_delay_ms(TEST_SETTLE_MS);

    float duty_before = (float)REG_GTCCRA / (float)REG_GTPR * 100.0f;

    /* Đổi period */
    comp_pwm_set_period(5000);
    test_delay_ms(TEST_SETTLE_MS);

    float duty_after = (float)REG_GTCCRA / (float)REG_GTPR * 100.0f;

    TEST_ASSERT_NEAR(duty_after, duty_before,
                     TOLERANCE_DUTY_PCT, "Duty ratio not preserved after period change");

    comp_pwm_stop();
    return TEST_PASS;
}

/* ── TC-PERIOD-003: GTDVU không đổi sau khi đổi period ── */
static test_result_t tc_period_deadtime_preserved(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_dead_time(500);
    uint32_t dt_before = REG_GTDVU;

    comp_pwm_set_period(1000);
    test_delay_ms(TEST_SETTLE_MS);
    comp_pwm_set_period(10000);
    test_delay_ms(TEST_SETTLE_MS);

    TEST_ASSERT(REG_GTDVU == dt_before, "GTDVU changed after period update");
    return TEST_PASS;
}

/* ── TC-PERIOD-004: Reject freq = 0 ─────────────── */
static test_result_t tc_period_reject_zero(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    fsp_err_t err = comp_pwm_set_period(0);
    TEST_ASSERT(err == FSP_ERR_INVALID_ARGUMENT, "Zero freq should be rejected");
    return TEST_PASS;
}

static const test_case_t s_period_cases[] = {
    {"TC-PERIOD-001", "GTPR value matches freq formula",      tc_period_register_value},
    {"TC-PERIOD-002", "Duty ratio preserved after period chg",tc_period_duty_preserved},
    {"TC-PERIOD-003", "Dead time unchanged after period chg",  tc_period_deadtime_preserved},
    {"TC-PERIOD-004", "Reject freq=0",                        tc_period_reject_zero},
};

const test_suite_t g_suite_period = {
    .suite_name = "Period — frequency sweep & boundary",
    .cases      = s_period_cases,
    .count      = sizeof(s_period_cases)/sizeof(s_period_cases[0]),
    .setup      = NULL,
    .teardown   = comp_pwm_stop,
};

5. Test Cases — Dead Time
tc_dead_time/tc_dead_time.c:

#include "comp_pwm_test_framework.h"

static const struct {
    uint32_t dt_ns;
    const char *label;
} s_dt_table[] = {
    {  100, "100ns" },
    {  500, "500ns" },
    { 1000, "1us"   },
    { 2000, "2us"   },
};

/* ── TC-DT-001: GTDVU counts tính đúng ──────────── */
static test_result_t tc_dt_register_value(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(10000);

    for(uint32_t i = 0; i < sizeof(s_dt_table)/sizeof(s_dt_table[0]); i++)
    {
        comp_pwm_set_dead_time(s_dt_table[i].dt_ns);
        test_delay_ms(TEST_SETTLE_MS);

        uint32_t expected = NS_TO_COUNTS(s_dt_table[i].dt_ns);
        uint32_t actual   = REG_GTDVU;

        TEST_ASSERT_NEAR(actual, expected,
                         1.0f,   /* ±1% — dead time cần chính xác hơn */
                         s_dt_table[i].label);
    }
    return TEST_PASS;
}

/* ── TC-DT-002: GTDVU ghi trực tiếp (non-buffered) ─
 * Kiểm tra: giá trị có hiệu lực ngay, không chờ sync
 * ──────────────────────────────────────────────────*/
static test_result_t tc_dt_non_buffered(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(1000);
    comp_pwm_start();

    /* Ghi trong khi timer đang chạy */
    comp_pwm_set_dead_time(500);
    uint32_t read_back = REG_GTDVU;
    uint32_t expected  = NS_TO_COUNTS(500);

    /* Nếu có buffer, read_back sẽ khác expected đến next cycle */
    TEST_ASSERT(read_back == expected,
                "GTDVU must be non-buffered: read-back mismatch");

    comp_pwm_stop();
    return TEST_PASS;
}

/* ── TC-DT-003: Reject dead_time >= GTPR/2 ──────── */
static test_result_t tc_dt_reject_too_large(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(1000);   /* GTPR = 15999 */

    /* dead_time lớn hơn GTPR/2 → không còn Middle section */
    uint32_t too_large_ns = (uint32_t)(
        COUNTS_TO_NS(REG_GTPR / 2u + 10u));

    fsp_err_t err = comp_pwm_set_dead_time(too_large_ns);
    TEST_ASSERT(err == FSP_ERR_INVALID_ARGUMENT,
                "Oversized dead time should be rejected");
    return TEST_PASS;
}

/* ── TC-DT-004: Dead time không đổi khi update duty ── */
static test_result_t tc_dt_stable_across_duty_change(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(10000);
    comp_pwm_set_dead_time(1000);
    comp_pwm_set_duty(50.0f);
    comp_pwm_start();

    uint32_t dt_before = REG_GTDVU;

    for(float d = 10.0f; d <= 90.0f; d += 10.0f) {
        comp_pwm_set_duty(d);
        test_delay_ms(2);
        TEST_ASSERT(REG_GTDVU == dt_before,
                    "GTDVU changed during duty sweep");
    }

    comp_pwm_stop();
    return TEST_PASS;
}

static const test_case_t s_dt_cases[] = {
    {"TC-DT-001","GTDVU counts = NS_TO_COUNTS(dt_ns)",      tc_dt_register_value},
    {"TC-DT-002","GTDVU is non-buffered (immediate effect)", tc_dt_non_buffered},
    {"TC-DT-003","Reject dead_time >= GTPR/2",               tc_dt_reject_too_large},
    {"TC-DT-004","GTDVU stable during duty sweep",           tc_dt_stable_across_duty_change},
};

const test_suite_t g_suite_dead_time = {
    .suite_name = "Dead time — GTDVU correctness & constraints",
    .cases      = s_dt_cases,
    .count      = sizeof(s_dt_cases)/sizeof(s_dt_cases[0]),
    .setup      = NULL,
    .teardown   = comp_pwm_stop,
};

6. Test Cases — Compare Region (3 vùng GTCCRA)
tc_compare_region/tc_compare_region.c:

#include "comp_pwm_test_framework.h"

/* ── Helper: đọc GTCCRA và tính vùng thực tế ─────── */
static comp_pwm_region_t _get_actual_region(void)
{
    uint32_t gtccra = REG_GTCCRA;
    uint32_t gtdvu  = REG_GTDVU;
    uint32_t gtpr   = REG_GTPR;

    if(gtccra < gtdvu)
        return COMP_PWM_REGION_LOW;
    else if(gtccra > gtpr - gtdvu)
        return COMP_PWM_REGION_HIGH;
    else
        return COMP_PWM_REGION_NORMAL;
}

/* ── Setup chung cho cả suite ────────────────────── */
static void region_suite_setup(void)
{
    comp_pwm_init(COMP_PWM_MODE1);
    comp_pwm_set_period(10000);     /* 10kHz */
    comp_pwm_set_dead_time(1000);   /* 1us   */
    comp_pwm_start();
    test_delay_ms(TEST_SETTLE_MS);
}

/* ── TC-REG-001: Vùng LOW — 0 < GTCCRA < GTDVU ──── */
static test_result_t tc_region_low(void)
{
    comp_pwm_set_compare_region(COMP_PWM_REGION_LOW);
    test_delay_ms(TEST_SETTLE_MS);

    uint32_t gtccra = REG_GTCCRA;
    uint32_t gtdvu  = REG_GTDVU;

    /* Assert vùng đúng */
    TEST_ASSERT(gtccra > 0,      "GTCCRA must be > 0");
    TEST_ASSERT(gtccra < gtdvu,  "GTCCRA must be < GTDVU (Region LOW)");

    /* Assert Slave2 tiếp quản: GTCCRA được map qua GTCCRC */
    TEST_ASSERT(REG_GTCCRC == gtccra,
                "GTCCRC must mirror GTCCRA for Slave2 linearity");

    /* Actual region confirm */
    TEST_ASSERT(_get_actual_region() == COMP_PWM_REGION_LOW,
                "Region detection mismatch");

    return TEST_PASS;
}

/* ── TC-REG-002: Vùng NORMAL — GTDVU < GTCCRA < GTPR-GTDVU ── */
static test_result_t tc_region_normal(void)
{
    comp_pwm_set_compare_region(COMP_PWM_REGION_NORMAL);
    test_delay_ms(TEST_SETTLE_MS);

    uint32_t gtccra = REG_GTCCRA;
    uint32_t gtdvu  = REG_GTDVU;
    uint32_t gtpr   = REG_GTPR;

    TEST_ASSERT(gtccra > gtdvu,
                "GTCCRA must be > GTDVU (Region NORMAL lower bound)");
    TEST_ASSERT(gtccra < (gtpr - gtdvu),
                "GTCCRA must be < GTPR-GTDVU (Region NORMAL upper bound)");

    /* Middle section: Master GTCNT vs GTCCRA active */
    TEST_ASSERT(_get_actual_region() == COMP_PWM_REGION_NORMAL,
                "Region detection mismatch");

    return TEST_PASS;
}

/* ── TC-REG-003: Vùng HIGH — GTPR-GTDVU < GTCCRA < GTPR ── */
static test_result_t tc_region_high(void)
{
    comp_pwm_set_compare_region(COMP_PWM_REGION_HIGH);
    test_delay_ms(TEST_SETTLE_MS);

    uint32_t gtccra = REG_GTCCRA;
    uint32_t gtdvu  = REG_GTDVU;
    uint32_t gtpr   = REG_GTPR;

    TEST_ASSERT(gtccra > (gtpr - gtdvu),
                "GTCCRA must be > GTPR-GTDVU (Region HIGH)");
    TEST_ASSERT(gtccra < gtpr,
                "GTCCRA must be < GTPR");

    TEST_ASSERT(_get_actual_region() == COMP_PWM_REGION_HIGH,
                "Region detection mismatch");

    return TEST_PASS;
}

/* ── TC-REG-004: Transition LOW → NORMAL → HIGH ──── */
static test_result_t tc_region_transition(void)
{
    comp_pwm_region_t seq[] = {
        COMP_PWM_REGION_LOW,
        COMP_PWM_REGION_NORMAL,
        COMP_PWM_REGION_HIGH,
        COMP_PWM_REGION_NORMAL,
        COMP_PWM_REGION_LOW,
    };

    for(uint32_t i = 0; i < sizeof(seq)/sizeof(seq[0]); i++) {
        comp_pwm_set_compare_region(seq[i]);
        test_delay_ms(10);   /* wait 2+ PWM cycles */
        TEST_ASSERT(_get_actual_region() == seq[i],
                    "Region not stable after transition");
    }
    return TEST_PASS;
}

/* ── TC-REG-005: GTCCRA không chạm biên 0 hoặc GTPR ── */
static test_result_t tc_region_boundary_clamp(void)
{
    /* Đặt duty = 0% → phải bị clamp, không được = 0 */
    comp_pwm_set_duty(0.0f);
    test_delay_ms(TEST_SETTLE_MS);
    TEST_ASSERT(REG_GTCCRA > 0,
                "GTCCRA must never be 0 (clamp required)");

    /* Đặt duty = 100% → phải bị clamp, không được = GTPR */
    comp_pwm_set_duty(100.0f);
    test_delay_ms(TEST_SETTLE_MS);
    TEST_ASSERT(REG_GTCCRA < REG_GTPR,
                "GTCCRA must never equal GTPR (clamp required)");

    return TEST_PASS;
}

static const test_case_t s_region_cases[] = {
    {"TC-REG-001","Region LOW:    0 < GTCCRA < GTDVU",         tc_region_low},
    {"TC-REG-002","Region NORMAL: GTDVU < GTCCRA < GTPR-GTDVU",tc_region_normal},
    {"TC-REG-003","Region HIGH:   GTPR-GTDVU < GTCCRA < GTPR", tc_region_high},
    {"TC-REG-004","Transition sequence L→N→H→N→L stable",      tc_region_transition},
    {"TC-REG-005","GTCCRA clamped: never 0 or GTPR",           tc_region_boundary_clamp},
};

const test_suite_t g_suite_compare_region = {
    .suite_name = "Compare Region — 3 vùng GTCCRA correctness",
    .cases      = s_region_cases,
    .count      = sizeof(s_region_cases)/sizeof(s_region_cases[0]),
    .setup      = region_suite_setup,
    .teardown   = comp_pwm_stop,
};

7. Test Runner & Report
comp_pwm_test_runner.c:

#include "comp_pwm_test_framework.h"

/* Declare tất cả suites */
extern const test_suite_t g_suite_mode1;
extern const test_suite_t g_suite_mode2;
extern const test_suite_t g_suite_mode3;
extern const test_suite_t g_suite_mode4;
extern const test_suite_t g_suite_period;
extern const test_suite_t g_suite_duty;
extern const test_suite_t g_suite_dead_time;
extern const test_suite_t g_suite_compare_region;

static const test_suite_t *s_all_suites[] = {
    &g_suite_mode1,
    &g_suite_mode2,
    &g_suite_mode3,
    &g_suite_mode4,
    &g_suite_period,
    &g_suite_duty,
    &g_suite_dead_time,
    &g_suite_compare_region,
};

/* ── Framework engine ─────────────────────────────── */
test_summary_t test_run_suite(const test_suite_t *suite)
{
    test_summary_t sum = {0};
    SEGGER_RTT_printf(0,
        "\r\n┌─────────────────────────────────────────\r\n"
        "│ SUITE: %s\r\n"
        "└─────────────────────────────────────────\r\n",
        suite->suite_name);

    if(suite->setup) suite->setup();

    for(uint32_t i = 0; i < suite->count; i++) {
        const test_case_t *tc = &suite->cases[i];
        SEGGER_RTT_printf(0, "  [RUN ] %s\r\n", tc->name);

        test_result_t r = tc->run();
        sum.total++;

        const char *badge;
        if     (r == TEST_PASS)  { sum.passed++;  badge = "PASS"; }
        else if(r == TEST_SKIP)  { sum.skipped++; badge = "SKIP"; }
        else if(r == TEST_ERROR) { sum.failed++;  badge = "ERR "; }
        else                     { sum.failed++;  badge = "FAIL"; }

        SEGGER_RTT_printf(0, "  [%s] %s — %s\r\n\r\n",
                          badge, tc->name, tc->description);
    }

    if(suite->teardown) suite->teardown();
    return sum;
}

test_summary_t test_run_all(void)
{
    test_summary_t total = {0};
    uint32_t n = sizeof(s_all_suites)/sizeof(s_all_suites[0]);

    for(uint32_t i = 0; i < n; i++) {
        test_summary_t s = test_run_suite(s_all_suites[i]);
        total.total   += s.total;
        total.passed  += s.passed;
        total.failed  += s.failed;
        total.skipped += s.skipped;
    }
    return total;
}

void test_print_summary(const test_summary_t *s)
{
    SEGGER_RTT_printf(0,
        "\r\n══════════════════════════════════════════\r\n"
        "  TEST SUMMARY\r\n"
        "  Total  : %lu\r\n"
        "  Passed : %lu\r\n"
        "  Failed : %lu\r\n"
        "  Skipped: %lu\r\n"
        "  Result : %s\r\n"
        "══════════════════════════════════════════\r\n",
        s->total, s->passed, s->failed, s->skipped,
        (s->failed == 0) ? "ALL PASS ✓" : "FAILED ✗");
}

/* ── Entry point (gọi từ hal_entry.c) ────────────── */
void comp_pwm_run_all_tests(void)
{
    SEGGER_RTT_printf(0,
        "\r\n==========================================\r\n"
        "  Complementary PWM Auto Test Suite\r\n"
        "==========================================\r\n");

    test_summary_t result = test_run_all();
    test_print_summary(&result);
}

8. Bảng Test Case Map
SuiteTest CaseKiểm tra
Mode 1–4TC-MODEx-001GTBER bits đúng
TC-MODEx-002Initial output level A
TC-MODEx-003Single/double buffer active
PeriodTC-PERIOD-001GTPR = formula
TC-PERIOD-002Duty ratio bảo toàn
TC-PERIOD-003GTDVU không đổi
TC-PERIOD-004Reject freq=0
Dead TimeTC-DT-001GTDVU counts đúng
TC-DT-002Non-buffered, hiệu lực ngay
TC-DT-003Reject DT ≥ GTPR/2
TC-DT-004GTDVU ổn định khi sweep duty
RegionTC-REG-001
Vùng LOW: 0 < GTCCRA < GTDVUTC-REG-002
Vùng NORMAL: Middle sectionTC-REG-003
Vùng HIGH: GTPR-DT < GTCCRA < GTPR
TC-REG-004Transition L→N→H stable
TC-REG-005Clamp tại 0% và 100%