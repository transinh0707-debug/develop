# Hướng dẫn đọc hiểu `r_gpt_tests_tg2_pwm.c`

**Đối tượng:** Developer cần tham khảo / mở rộng test suite trong folder `compl_pwm/!test/`
**Framework test:** Unity + Unity Fixture (chuẩn FSP sử dụng)
**File phân tích:** `compl_pwm/!test/r_gpt_tests_tg2_pwm.c` (2054 dòng, 18 test case)
**File liên quan:**
- `r_gpt_test_data.h` / `.c` — shared config struct dùng chung giữa các test group
- `r_gpt_tests_runner.h` / `.c` — entry point gọi tất cả test group
- `r_gpt_tests_tg1.c` — test group 1 (event counting, non-PWM)

---

## 1. Tổng quan kiến trúc

Một test suite GPT trong FSP gồm 3 lớp file, chạy theo thứ tự từ trên xuống:

```
┌──────────────────────────────────────────────────────────┐
│  r_gpt_tests_runner.c                                    │
│  • RunAllR_GPTTests()     ← entry point duy nhất         │
│  • TEST_GROUP_RUNNER(TG1) ← liệt kê test case của TG1    │
│  • TEST_GROUP_RUNNER(TG2) ← liệt kê test case của TG2    │
└───────────────┬──────────────────────────────────────────┘
                │ gọi
                ▼
┌──────────────────────────────────────────────────────────┐
│  r_gpt_tests_tg2_pwm.c  (file này)                       │
│  • TEST_GROUP(R_GPT_TG2)      ← khai báo group           │
│  • TEST_SETUP(R_GPT_TG2)      ← chạy TRƯỚC mỗi test      │
│  • TEST_TEAR_DOWN(R_GPT_TG2)  ← chạy SAU mỗi test        │
│  • TEST(R_GPT_TG2, TC_xxx)    ← từng test case           │
│  • helper functions (static)                             │
│  • variables (static / extern)                           │
└───────────────┬──────────────────────────────────────────┘
                │ dùng
                ▼
┌──────────────────────────────────────────────────────────┐
│  r_gpt_test_data.c / r_gpt_test_data.h                   │
│  • g_gpt_timer_ext     ← shared config "output ON"       │
│  • g_gpt_timer_ext_off ← shared config "output OFF"      │
└──────────────────────────────────────────────────────────┘
```

**Điểm cần nắm:**
- Mọi test case đều kế thừa `TEST_SETUP` (chạy trước) và `TEST_TEAR_DOWN` (chạy sau). Chỉ cần viết phần setup/teardown một lần cho cả group, không copy lặp lại trong từng test.
- Tên test group (`R_GPT_TG2`) là identifier kiểu C, không phải string. Unity dùng nó để ghép tên symbol ở bước preprocessor.
- Mỗi `TEST(R_GPT_TG2, TC_xxx)` sẽ được runner gọi bằng dòng `RUN_TEST_CASE(R_GPT_TG2, TC_xxx)` trong `r_gpt_tests_runner.c`. Không có runner → test không chạy, dù có code trong file.

---

## 2. Cách define biến (variables)

File dùng 5 cách khác nhau để declare biến, mỗi cách có ý nghĩa riêng. Hiểu đúng scope và lifetime là điều kiện tiên quyết để debug cho đúng.

### 2.1. `static` global variables — scope chỉ trong file, nhưng tồn tại suốt chương trình

```c
// Dòng 124-130
static uint32_t            g_timer_test_limit_cycles = 0;
static gpt_instance_ctrl_t g_gpt_ctrl;
static timer_cfg_t                  g_timer_test_capture_cfg;
static gpt_extended_cfg_t           g_timer_test_capture_cfg_extend;
static gpt_extended_pwm_cfg_t       g_gpt_test_pwm_ram_cfg;
```

**Đặc điểm:**
- Từ khóa `static` giới hạn linker scope: các file khác (`.o` khác) KHÔNG thấy biến này → tránh xung đột tên giữa TG1, TG2, TG3.
- Tiền tố `g_` (global) là quy ước FSP để phân biệt biến global với biến local trong hàm.
- Biến kiểu `gpt_instance_ctrl_t` là control block của driver — driver ghi lên nó trong `R_GPT_Open()`, đọc từ nó trong `R_GPT_Start()`, `R_GPT_DutyCycleSet()`...
- Biến kiểu `_cfg`, `_cfg_extend`, `_pwm_ram_cfg` là các struct configuration. Chúng được **ghi lại (`= ...`) trong `TEST_SETUP` mỗi lần test mới** để đảm bảo test không bị ảnh hưởng bởi thay đổi của test trước.

**Khi nào dùng:** biến cần giữ state qua nhiều callback hoặc qua nhiều lần gọi API (ví dụ `g_gpt_ctrl` được Open một lần ở đầu test và Close ở TEAR_DOWN).

### 2.2. `static const` default configuration — read-only template

```c
// Dòng 131-147
static const gpt_extended_pwm_cfg_t g_gpt_test_pwm_default_cfg =
{
    .trough_ipl             = 3,
    .trough_irq             = FSP_INVALID_VECTOR,
    .poeg_link              = GPT_POEG_LINK_POEG0,
    .output_disable         = GPT_OUTPUT_DISABLE_NONE,
    .adc_trigger            = GPT_ADC_TRIGGER_NONE,
    .dead_time_count_up     = 0,
    .dead_time_count_down   = 0,
    // ...
};
```

**Đặc điểm:**
- `const` làm compiler đặt struct này vào flash (.rodata), không chiếm RAM.
- Dùng làm "factory reset" — `TEST_SETUP` copy struct này vào biến RAM (`g_gpt_test_pwm_ram_cfg`) để test có thể modify mà không làm hỏng template.

**Pattern reset trong TEST_SETUP (dòng 298):**
```c
g_gpt_test_pwm_ram_cfg = g_gpt_test_pwm_default_cfg;  // copy const → RAM
```

Đây là kỹ thuật quan trọng: template lưu ở flash (read-only), copy vào RAM khi cần modify.

### 2.3. `extern` variables — chia sẻ giữa nhiều file

```c
// Dòng 113 của r_gpt_tests_tg2_pwm.c
extern uint32_t g_timer_test_sckdivcr_saved;

// Dòng 44-45 của r_gpt_test_data.h
extern const gpt_extended_cfg_t g_gpt_timer_ext;
extern const gpt_extended_cfg_t g_gpt_timer_ext_off;
```

**Đặc điểm:**
- `extern` nói "biến này định nghĩa ở file khác, linker sẽ resolve khi link".
- Định nghĩa thực tế (không có `extern`) nằm ở `r_gpt_test_data.c` hoặc `r_gpt_tests_tg1.c`.
- Dùng khi nhiều test group cần chia sẻ cùng một biến (ví dụ `g_timer_test_sckdivcr_saved` lưu giá trị SCKDIVCR để khôi phục lại sau test).

### 2.4. `volatile` variables — báo compiler không tối ưu hóa

```c
// Dòng 101
volatile bool g_timer_test_halted_for_debug = true;

// Dòng 155 (trong #if TIMER_TEST_ADC)
static volatile uint32_t g_gpt_test_adc_callbacks = 0;
```

**Đặc điểm:**
- `volatile` báo compiler: "biến này có thể thay đổi ngoài luồng thực thi hiện tại" (do ISR ghi, debugger ghi, hoặc DMA ghi).
- Nếu không có `volatile`, compiler có thể cache giá trị trong register và bỏ qua các cập nhật từ ISR → test sẽ hang.

**Khi nào bắt buộc dùng:** bất cứ biến nào ISR/callback có thể ghi mà main code đọc (hoặc ngược lại). Ví dụ `g_gpt_test_adc_callbacks` được callback ADC increment, main code đọc để đếm số callback đã xảy ra.

### 2.5. Local variables trong test case — scope chỉ trong 1 test

```c
// Dòng 452-462 của TC_SymmetricPwm
uint32_t             capture_count        = TIMER_TEST_NUM_CAPTURES;
timer_cfg_t        * p_timer_cfg          = &g_timer_test_capture_cfg;
timer_cfg_t        * p_timer_ram_cfg      = &g_timer_test_ram_cfg;
const uint32_t gtioca_expected_max        = (uint32_t) (TIMER_TEST_DEFAULT_DUTY * 2);
const uint32_t gtioca_expected_min        = (uint32_t) (TIMER_TEST_DEFAULT_DUTY * 2);
```

**Pattern quan trọng — "Intermediate variables for readability":**
- Thay vì viết `g_timer_test_capture_cfg.p_extend->some_field` ở 10 chỗ, test code alias bằng pointer `p_timer_cfg` để code gọn hơn.
- Biến `const uint32_t gtioca_expected_*` là giá trị mong đợi được tính sẵn — so sánh vào đó ở các `TEST_ASSERT_UINT_WITHIN()` phía dưới.
- Đây là idiom của FSP test: phần đầu test case gần như luôn có block "Intermediate variables for readability" tạo các alias.

---

## 3. Cách define function (functions)

File có 4 loại function, mỗi loại có role rõ ràng:

### 3.1. Test framework macros — KHÔNG phải function thường

```c
// Dòng 204
TEST_GROUP(R_GPT_TG2);                    // Khai báo group

// Dòng 209
TEST_SETUP(R_GPT_TG2) { ... }             // Setup trước mỗi test

// Dòng 380
TEST_TEAR_DOWN(R_GPT_TG2) { ... }         // Cleanup sau mỗi test

// Dòng 448
TEST(R_GPT_TG2, TC_SymmetricPwm) { ... }  // Từng test case
```

**Bản chất:** đây là macro của Unity Fixture, expand thành function thực sự tên kiểu `test_R_GPT_TG2_TC_SymmetricPwm_()` ở bước preprocessor. Không cần viết prototype, không cần `void` return type — macro lo hết.

**Điểm dễ nhầm:**
- Hai `TEST()` cùng tên trong cùng group sẽ lỗi linker (duplicate symbol), không phải lỗi compile.
- Nếu viết `TEST(R_GPT_TG2, TC_Foo)` nhưng quên thêm `RUN_TEST_CASE(R_GPT_TG2, TC_Foo)` trong runner → test được build nhưng không chạy. Đây là bug thầm lặng, cần kiểm tra runner mỗi khi thêm test.

### 3.2. Helper functions `static` — dùng nội bộ trong file

```c
// Dòng 312 — callback cho one-shot pulse test
static void timer_test_one_shot_pulse_callback (timer_callback_args_t * p_args)
{
    if (TIMER_EVENT_CYCLE_END == p_args->event)
    {
        g_timer_test_cycle_end_counter++;
        if (g_timer_test_cycle_end_counter == g_timer_test_limit_cycles)
        {
            gp_timer_test_api->stop(&g_gpt_ctrl);
        }
    }
}

// Dòng 336 — callback update duty cycle cho asymmetric PWM
static void gpt_test_pwm_duty_set_callback (timer_callback_args_t * p_args)
{
    static volatile uint32_t trough_duty = TIMER_TEST_DEFAULT_DUTY;

    if (TIMER_EVENT_CREST == p_args->event)
    {
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_GPT_DutyCycleSet(&g_gpt_ctrl,
                                             g_timer_test_ram_cfg.duty_cycle_counts,
                                             GPT_IO_PIN_GTIOCA));
        // ...
    }
    // ...
}
```

**Đặc điểm:**
- `static` ở đây giới hạn scope trong file. Linker sẽ không expose ra ngoài.
- Forward declaration bắt buộc phải có ở đầu file (dòng 121-122):
  ```c
  static void timer_test_one_shot_pulse_callback(timer_callback_args_t * p_args);
  static void gpt_test_pwm_duty_set_callback(timer_callback_args_t * p_args);
  ```
- Các callback này được gán vào `g_timer_test_ram_cfg.p_callback` trong từng test case cần dùng.
- Lưu ý: `static volatile uint32_t trough_duty` bên trong callback — biến giữ state GIỮA các lần gọi callback (vì có `static`), và tránh compiler tối ưu (vì có `volatile`).

### 3.3. Helper functions non-static — có thể gọi từ file khác

```c
// Dòng 118-119 — prototype
void r_gpt_tests_restore_clock_settings(void);
void one_shot_pulse_preload_timings(void);

// Dòng 643 — definition
void one_shot_pulse_preload_timings (void)
{
    // ...
}
```

**Đặc điểm:**
- Không có `static` → function có thể gọi từ test group khác (ví dụ TG1 có thể gọi `r_gpt_tests_restore_clock_settings()`).
- Không cần khai báo `extern` trong file gọi — chỉ cần prototype.

### 3.4. Unity assertion macros — không phải function, nhưng dùng như function

```c
TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, p_timer_ram_cfg));
TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts * 2, capture);
TEST_ASSERT_GREATER_OR_EQUAL(0, TIMER_TEST_GPT_CKDIVCR_DFLT);
TEST_IGNORE_MESSAGE("This test is only run if triangle-wave PWM output is enabled.");
```

**Danh sách các macro thường dùng:**

| Macro | Ý nghĩa |
|---|---|
| `TEST_ASSERT_EQUAL(expected, actual)` | Fail nếu không bằng nhau |
| `TEST_ASSERT_NOT_EQUAL(expected, actual)` | Fail nếu bằng nhau |
| `TEST_ASSERT_UINT_WITHIN(delta, expected, actual)` | Fail nếu `abs(expected - actual) > delta` — dùng cho đo PWM có sai số |
| `TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)` | Fail nếu `actual < threshold` |
| `TEST_ASSERT_TRUE(condition)` / `TEST_ASSERT_FALSE(condition)` | Fail nếu sai/đúng |
| `TEST_ASSERT_NULL(pointer)` / `TEST_ASSERT_NOT_NULL(pointer)` | Fail nếu pointer sai giá trị |
| `TEST_IGNORE_MESSAGE("reason")` | Skip test, hiển thị lý do — dùng khi feature chưa support trên MCU hiện tại |
| `TEST_FAIL_MESSAGE("reason")` | Fail ngay, hiển thị lý do |

**Điểm quan trọng:** các macro này **không return** khi assert fail — chúng dùng `longjmp` để nhảy ra khỏi hàm test, cleanup rồi báo fail. Code sau assertion sẽ KHÔNG chạy nếu assertion fail.

---

## 4. Cách viết một test case

### 4.1. Cấu trúc chuẩn của một test case

Mọi `TEST()` đều theo cùng 6 bước, lấy `TC_SymmetricPwm` làm ví dụ (dòng 448-550):

```c
TEST(R_GPT_TG2, TC_SymmetricPwm)                              // (0) KHAI BÁO TEST
{
    r_gpt_tests_restore_clock_settings();                     // (1) KHÔI PHỤC CLOCK

    /* Intermediate variables for readability */              // (2) ALIAS / EXPECTED VALUES
    uint32_t       capture_count      = TIMER_TEST_NUM_CAPTURES;
    timer_cfg_t *  p_timer_ram_cfg    = &g_timer_test_ram_cfg;
    const uint32_t gtioca_expected_avg = (uint32_t)(TIMER_TEST_DEFAULT_DUTY * 2);
    // ...

    /* Create a configuration for GPT symmetric PWM. */       // (3) SỬA CONFIG
    p_timer_ram_cfg->mode          = TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM;
    p_timer_ram_cfg->p_callback    = NULL;
    p_timer_ram_cfg->cycle_end_irq = FSP_INVALID_VECTOR;

    /* Opening the GPT to generate PWM. */                    // (4) OPEN + START DRIVER
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, p_timer_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));

    for (uint32_t i = 1; i < 3; i++)                          // (5) THỰC HIỆN TEST + ASSERT
    {
        uint32_t offset = (i - 1) * (TIMER_TEST_DEFAULT_DUTY / 2);
        uint32_t duty   = TIMER_TEST_DEFAULT_DUTY + offset;
        TEST_ASSERT_EQUAL(FSP_SUCCESS,
                          R_GPT_DutyCycleSet(&g_gpt_ctrl, duty, GPT_IO_PIN_GTIOCA));

        /* Verify PWM waveform period */
        timer_test_capture_cfg(p_timer_extend_cfg,
                               TIMER_TEST_CAPTURE_PULSE_PERIOD, GPT_IO_PIN_GTIOCA);
        capture = r_timer_capture_output(p_timer_cfg, p_timer_api,
                                         capture_count, TIMER_TEST_CAPTURE_AVG_VERIFY);
        TEST_ASSERT_UINT_WITHIN(2, g_timer_test_ram_cfg.period_counts * 2, capture);
        // ...
    }
    // (6) KHÔNG CẦN CLEANUP — TEST_TEAR_DOWN lo việc R_GPT_Close()
}
```

### 4.2. Vì sao 6 bước đó?

| Bước | Lý do |
|---|---|
| (0) Khai báo | Macro `TEST(group, name)` expand thành function `test_group_name_()` mà runner sẽ gọi |
| (1) Khôi phục clock | Test trước có thể đã đổi clock divider. Về default để test hiện tại chạy đúng |
| (2) Alias / expected | Gọn code, tách biệt giá trị mong đợi với logic test → fail message rõ ràng hơn |
| (3) Sửa config | `TEST_SETUP` đã reset config về default. Test chỉ override các field cần khác default |
| (4) Open + Start | `R_GPT_Open()` apply config vào register. `R_GPT_Start()` cho counter chạy. Cả hai phải thành công trước khi test logic |
| (5) Test + Assert | Mỗi lần thay đổi (duty, dead time, mode, ...) là 1 assertion. Fail ở bất kỳ assertion nào → test fail, nhảy sang TEAR_DOWN |
| (6) Cleanup | **Không cần code!** `TEST_TEAR_DOWN` tự động đóng driver, khôi phục clock, unlock PRCR |

### 4.3. Doxygen annotation trước mỗi test

FSP yêu cầu mỗi test phải có doxygen comment theo format:

```c
/**
 * @req{gpt_symmetric_pwm,SWFLEX-592,R_GPT_Open} The GPT shall support generating symmetric PWM waveforms.
 *
 * @verify{gpt_symmetric_pwm} The high level waveforms of GTIOC pins match expected values for symmetric
 * PWM when they are measured with an input capture timer.
 */
TEST(R_GPT_TG2, TC_SymmetricPwm)
{
    // ...
}
```

**Giải thích:**
- `@req{req_id, jira_ticket, api}` — liên kết test case với requirement trong hệ thống truy xuất nguồn gốc (requirement traceability). Tool CI của Renesas sẽ parse các tag này để tạo báo cáo "mỗi requirement được test bằng những test case nào".
- `@verify{req_id}` — mô tả cách test case verify requirement đó (ngắn gọn, 1-2 câu).

**Không có doxygen = test không track được trong traceability report.** Đây không phải build error nhưng là lỗi review.

### 4.4. Pattern xử lý test conditional (skip trên MCU không support)

```c
// Dòng 1581-1585 của TC_ComplementaryPWM
TEST(R_GPT_TG2, TC_ComplementaryPWM)
{
#if TIMER_TEST_COMPLEMENTARY_MODE_TODO
    TEST_IGNORE_MESSAGE("This test is temporarily ignored for RA6T2 pending further testing.");
#else
    // ... test body ...
#endif
}
```

**Pattern:** macro `TIMER_TEST_xxx_TODO` được định nghĩa ở đầu file (dòng 47-51) bằng `#define ... (BSP_MCU_GROUP_RA6T2)`. Khi compile cho RA6T2, macro = 1 → `TEST_IGNORE_MESSAGE` chạy → test báo skipped chứ không báo fail. Khi compile cho RA2T1, macro = 0 → test body chạy bình thường.

**Khi nào dùng:** khi một feature phụ thuộc MCU (ví dụ GPTE dead time chỉ có trên GPT32E/GPT32EH) hoặc feature có bug tạm thời trên MCU nào đó.

---

## 5. Ví dụ thực tế — đọc hiểu `TC_ComplementaryPWM` từ đầu đến cuối

Dưới đây là đọc hiểu test case Complementary PWM (dòng 1581-1714) — test case gần nhất với công việc Complementary PWM Modes 1-4 đang phát triển.

### 5.1. Phần header — sửa config và khai báo biến

```c
TEST(R_GPT_TG2, TC_ComplementaryPWM)
{
#if TIMER_TEST_COMPLEMENTARY_MODE_TODO
    TEST_IGNORE_MESSAGE("...");
#else
    /* Intermediate variables for readability */
    timer_cfg_t        * p_timer_cfg        = &g_timer_test_capture_cfg;
    gpt_extended_cfg_t * p_timer_extend_cfg = &g_timer_test_capture_cfg_extend;
    const timer_api_t  * p_timer_api        = gp_timer_test_input_capture_api;

    uint32_t             capture = 0U;
    uint32_t             duty_cycle_counts = g_timer_test_ram_cfg.period_counts * 60 / 100;
```

**Giải thích:**
- Alias pointer cho config của **input capture timer** (khác với main PWM timer `g_gpt_ctrl`) — test dùng 2 timer: 1 cái generate PWM, 1 cái capture waveform để verify.
- `duty_cycle_counts = period * 60 / 100` — chọn 60% duty cycle, tính ra raw counts.

### 5.2. Phần khai báo mảng config — test nhiều configuration

```c
gpt_gtior_setting_t gtior_cfgs[GPT_COMPLEMENTARY_TESTS_NUM_CFGS] = {0};

/* Set GTIOA to initial low, cycle end low and GTIOB to initial high, cycle end high */
gtior_cfgs[0].gtior_b.gtioa  = 0x07; // Initial low, low at cycle end
gtior_cfgs[0].gtior_b.oadflt = 0;    // Output low when counting stops
gtior_cfgs[0].gtior_b.oae    = 1;    // Output enabled
gtior_cfgs[0].gtior_b.gtiob  = 0x1B; // Initial high, high at cycle end
gtior_cfgs[0].gtior_b.obdflt = 1;    // Output high when counting stops
gtior_cfgs[0].gtior_b.obe    = 1;    // Output enabled

/* Set GTIOA to initial high, ... (configuration đối ngược) */
gtior_cfgs[1].gtior_b.gtioa  = 0x1B;
// ...
```

**Pattern:** mảng `gtior_cfgs[]` chứa 2 configuration **đối ngược nhau** về initial/stop level của GTIOCA/GTIOCB. Test sẽ chạy cả 2 để verify:
- Complementary output hoạt động đúng bất kể GTIOCA cấu hình active-high hay active-low.
- Stop level (lúc counter dừng) đúng với cấu hình oadflt/obdflt.

### 5.3. Phần chính — loop test từng configuration

```c
for (uint32_t i = 0; i < GPT_COMPLEMENTARY_TESTS_NUM_CFGS; i++)
{
    /* Intermediate aliases for current config */
    uint32_t oadflt = gtior_cfgs[i].gtior_b.oadflt;
    uint32_t obdflt = gtior_cfgs[i].gtior_b.obdflt;
    uint32_t gtioa  = gtior_cfgs[i].gtior_b.gtioa;
    uint32_t gtiob  = gtior_cfgs[i].gtior_b.gtiob;

    g_timer_test_ram_cfg_extend.gtior_setting = gtior_cfgs[i];
```

**Pattern:** mỗi iteration, test **ghi config hiện tại vào struct RAM** rồi Open driver. Driver sẽ đọc struct này và apply vào register GTIOR.

### 5.4. Verify tại 0% duty

```c
    /* Open, start, and set to 0% duty */
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Open(&g_gpt_ctrl, &g_timer_test_ram_cfg));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, R_GPT_Start(&g_gpt_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, 0, GPT_IO_PIN_GTIOCA_AND_GTIOCB));

    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);  // chờ waveform ổn định

    /* Verify GTIOCA và GTIOCB complementary tại 0% duty */
    volatile uint32_t timeout = TIMER_TEST_WAVEFORM_TIMEOUT / TIMER_TEST_NUM_CAPTURES;
    while ((timeout--) > 0)
    {
        TEST_ASSERT_EQUAL(((gtioa & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A));
        TEST_ASSERT_EQUAL(((gtiob & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));
    }
```

**Phân tích:**
- `R_GPT_DutyCycleSet(..., 0, GPT_IO_PIN_GTIOCA_AND_GTIOCB)` — set duty 0% cho cả 2 pin cùng lúc.
- `R_BSP_SoftwareDelay(10ms)` — hardware cần vài chu kỳ PWM để thật sự apply duty mới.
- Loop `timeout` lần verify pin level (đọc GPIO thật, không phải register configuration). Giá trị mong đợi tính từ `gtioa` bit field — expression `(gtioa & 0xC) == 0x4` check bit 2 và bit 3 của config.

**Bài học về delay:** khi test waveform, LUÔN có `R_BSP_SoftwareDelay()` sau mỗi thay đổi duty. Register đã update ngay lập tức, nhưng **waveform cần 1-2 cycle để apply** do buffer chain (GTCCRD → Temp A → GTCCRA).

### 5.5. Verify tại 60% duty — đo waveform bằng input capture

```c
    /* Set 60% duty */
    TEST_ASSERT_EQUAL(FSP_SUCCESS,
                      R_GPT_DutyCycleSet(&g_gpt_ctrl, duty_cycle_counts, GPT_IO_PIN_GTIOCA_AND_GTIOCB));
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

    /* Chờ counter vào "OFF portion" để đo từ edge đầu tiên */
    uint32_t gtcnt = g_gpt_ctrl.p_reg->GTCNT;
    while (gtcnt > (duty_cycle_counts / 4))
    {
        gtcnt = g_gpt_ctrl.p_reg->GTCNT;
    }

    /* Verify pin complementary (A = !B) */
    TEST_ASSERT_EQUAL(TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A),
                     !TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));

    /* Đo pulse width GTIOCA bằng input capture */
    timer_test_capture_cfg(p_timer_extend_cfg, gtioca_capture, GPT_IO_PIN_GTIOCA);
    capture = r_timer_capture_output(p_timer_cfg, p_timer_api,
                                     TIMER_TEST_NUM_CAPTURES,
                                     TIMER_TEST_CAPTURE_AVG_VERIFY);
    TEST_ASSERT_UINT_WITHIN(2,
                            g_timer_test_ram_cfg.period_counts - duty_cycle_counts,
                            capture);
```

**Phân tích:**
- **Polling `GTCNT`** để biết counter đang ở đâu trong chu kỳ — đảm bảo capture bắt đầu từ OFF portion, không phải giữa pulse.
- `timer_test_capture_cfg()` và `r_timer_capture_output()` là helper của **input capture test timer** (timer thứ 2). Chúng không có trong file này — định nghĩa ở `r_timer_test_port.h`.
- `TEST_ASSERT_UINT_WITHIN(2, expected, actual)` — `2` là sai số chấp nhận được (đơn vị count). Đây là cần thiết vì:
  - Capture timer và PWM timer chạy độc lập → có jitter sampling vài count.
  - Comment dòng 501 của file: "Increased from 1 to 2. See FSPRA-2975" — ban đầu sai số là 1, tăng lên 2 sau khi tìm ra bug FSPRA-2975.

### 5.6. Verify tại 100% duty

```c
    /* Set 100% duty */
    fsp_err_t err = R_GPT_DutyCycleSet(&g_gpt_ctrl,
                                       g_timer_test_ram_cfg.period_counts,
                                       GPT_IO_PIN_GTIOCA_AND_GTIOCB);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);

    /* Verify pin level ngược lại so với 0% duty */
    timeout = TIMER_TEST_WAVEFORM_TIMEOUT / TIMER_TEST_NUM_CAPTURES;
    while ((timeout--) > 0)
    {
        TEST_ASSERT_EQUAL(!((gtioa & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_A));
        TEST_ASSERT_EQUAL(!((gtiob & 0xC) == 0x4), TIMER_TEST_PIN_READ(TIMER_TEST_OUTPUT_PIN_B));
    }

    R_GPT_Close(&g_gpt_ctrl);   // đóng để iteration sau Open lại được
}
#endif
}
```

**Phân tích cuối:**
- Verify 100% dùng **phủ định** của expression 0% duty (`!(... == 0x4)`) — logic ngắn gọn, không cần tính lại.
- `R_GPT_Close()` ở CUỐI loop — bắt buộc trước khi Open lại cho iteration sau. Nếu không có, `R_GPT_Open()` sẽ trả về error vì driver đã mở.
- Ngoài vòng loop, `TEST_TEAR_DOWN` vẫn gọi `R_GPT_Close()` một lần nữa cho an toàn (có check `if (0U != g_gpt_ctrl.open)`).

---

## 6. Checklist khi viết test case mới

Khi thêm một test case mới vào `r_gpt_tests_tg2_pwm.c`:

1. **Khai báo `TEST(R_GPT_TG2, TC_YourTestName)`** trong file này.
2. **Thêm `RUN_TEST_CASE(R_GPT_TG2, TC_YourTestName)`** vào `r_gpt_tests_runner.c` — quên bước này test sẽ không chạy.
3. **Viết doxygen `@req{}` và `@verify{}`** trước `TEST()` — bắt buộc cho traceability.
4. **Gọi `r_gpt_tests_restore_clock_settings()`** nếu test đụng đến clock.
5. **Copy config từ default template** thay vì sửa trực tiếp — `TEST_SETUP` đã làm việc này cho `g_timer_test_ram_cfg`, nhưng nếu tạo struct mới, phải tự copy.
6. **KHÔNG gọi `R_GPT_Close()` ở cuối** trừ khi có nhiều `R_GPT_Open()` trong cùng test — `TEST_TEAR_DOWN` đã lo.
7. **Đặt `R_BSP_SoftwareDelay()`** sau mỗi lần thay đổi duty / mode nếu test có đo waveform.
8. **Dùng `TEST_ASSERT_UINT_WITHIN(delta, ...)`** thay vì `TEST_ASSERT_EQUAL` cho các giá trị đo waveform — luôn có jitter.
9. **Skip bằng `TEST_IGNORE_MESSAGE()` + macro `_TODO`** cho MCU chưa support — đừng xoá test hay comment code.
10. **Nếu test fail** — lỗi đầu tiên báo sẽ dừng test đó, các assertion sau KHÔNG chạy. Đặt assertion quan trọng nhất lên trước.

---

## 7. Tham khảo thêm

| File | Nội dung |
|---|---|
| `compl_pwm/!test/r_gpt_test_data.h/.c` | Shared config template (`g_gpt_timer_ext`, `g_gpt_timer_ext_off`) |
| `compl_pwm/!test/r_gpt_tests_runner.c` | Danh sách tất cả test case, `RunAllR_GPTTests()` entry point |
| `compl_pwm/!test/r_gpt_tests_tg1.c` | Test group 1 — event counting, non-PWM. Cấu trúc tương tự file này |
| `compl_pwm/src/r_gpt_test_tg3_comp_pwm.c` | Test group 3 (Complementary PWM) — KHÔNG dùng Unity Fixture, tự implement test harness qua `comp_pwm_run_all_tests()` |
| RA2T1 User's Manual §20.3 | Chi tiết GPT peripheral — register, timing, PWM mode |
| Unity Framework docs | https://github.com/ThrowTheSwitch/Unity — chi tiết tất cả `TEST_ASSERT_*` macro |

---

**Ghi chú về test group 3 (Complementary PWM):**
File `r_gpt_test_tg3_comp_pwm.c` trong `compl_pwm/src/` (không phải `!test/`) **không dùng Unity Fixture**. Thay vào đó có harness riêng (`comp_pwm_run_all_tests()`, `test_report()`, `test_setup_config()`...) in kết quả qua SEGGER RTT. Lý do: TG3 chạy trên board thật với oscilloscope để verify waveform, trong khi TG2 dùng input capture timer để verify tự động — hai mô hình test khác nhau, framework cũng khác nhau.
