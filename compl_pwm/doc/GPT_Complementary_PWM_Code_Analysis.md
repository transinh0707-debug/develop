# GPT Complementary PWM Code Analysis
**FSPRA-5725 | FSP v6.5.0 rc0**

## Document Overview

This document provides line-by-line analysis of the GPT complementary PWM implementation across two source files:
- `r_gpt.c` — Single-channel GPT driver (base layer)
- `r_gpt_three_phase.c` — Three-phase wrapper for motor control (aggregation layer)

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Key Data Structures](#key-data-structures)
3. [r_gpt.c — Line-by-Line Analysis](#r_gptc--line-by-line-analysis)
4. [r_gpt_three_phase.c — Line-by-Line Analysis](#r_gpt_three_phasec--line-by-line-analysis)
5. [Buffer Chain Mechanics](#buffer-chain-mechanics)
6. [Usage Examples](#usage-examples)
7. [Common Pitfalls](#common-pitfalls)

---

## Architecture Overview

### Layered Design

```
┌─────────────────────────────────────────────┐
│   Application Code (user)                   │
└─────────────────┬───────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────┐
│   r_gpt_three_phase.c                       │
│   • Aggregates 3 GPT channels (U, V, W)     │
│   • Handles three-phase buffer modes        │
│   • Enforces W-channel-last ordering        │
└─────────────────┬───────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────┐
│   r_gpt.c (called 3 times, once per phase)  │
│   • Single GPT channel driver               │
│   • Configures GTBER2, GTCCR, GTDVU         │
│   • Sets up complementary PWM mode          │
└─────────────────┬───────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────┐
│   Hardware (GPT peripheral registers)       │
└─────────────────────────────────────────────┘
```

### Initialization Flow

```
User calls R_GPT_THREE_PHASE_Open()
  │
  ├─→ Loop: for each channel (U, V, W)
  │     │
  │     ├─→ Call R_GPT_Open(channel)  [r_gpt.c]
  │     │     │
  │     │     ├─→ Configure GTCR.MD (mode 12-15 for comp PWM)
  │     │     ├─→ Set GTBER2.CMTCA=1, CP3DB=0, CPBTD=0 (single buffer)
  │     │     ├─→ Initialize GTCCRA, GTCCRD with duty_cycle_counts
  │     │     └─→ Configure GTDVU (dead time) if p_pwm_cfg provided
  │     │
  │     ├─→ Save register pointer: p_instance_ctrl->p_reg[ch]
  │     │
  │     └─→ [three_phase override for double buffer]
  │           │
  │           ├─→ If Mode 3/4 AND buffer_mode==DOUBLE:
  │           │     ├─→ Override GTBER2.CP3DB=1 (double buffer ON)
  │           │     └─→ Initialize GTCCRF alongside GTCCRD
  │           │
  │           └─→ Else: single buffer already set by r_gpt.c
  │
  └─→ Store buffer_mode, channel_mask in instance control
```

---

## Key Data Structures

### 1. GTCCR Register Array (Compare Match Registers)

```c
/* r_gpt.c line 60-65 */
#define GPT_PRV_GTCCRA  (0U)  // Active compare match register
#define GPT_PRV_GTCCRB  (1U)  // Compare match B (not used in comp PWM)
#define GPT_PRV_GTCCRC  (2U)  // Buffer register (standard triangle PWM)
#define GPT_PRV_GTCCRE  (3U)  // Buffer register (standard triangle PWM)
#define GPT_PRV_GTCCRD  (4U)  // Buffer register (complementary PWM single)
#define GPT_PRV_GTCCRF  (5U)  // Buffer register (complementary PWM double)
```

**Access pattern:**
```c
p_instance_ctrl->p_reg->GTCCR[GPT_PRV_GTCCRA] = value;  // Array index
```

### 2. GTBER2 Register (Buffer Enable Register 2)

| Bit Field | Name  | Purpose | Complementary PWM Setting |
|-----------|-------|---------|---------------------------|
| CMTCA     | Compare Match Transfer A | Enable buffer transfer at compare match | `1` = Enabled |
| CP3DB     | Complementary PWM Double Buffer | Enable second buffer stage | `0` = Single, `1` = Double |
| CPBTD     | Complementary PWM Buffer Transfer Disable | Bypass buffer (Mode 4 only) | `0` = Normal transfer |

**Single buffer configuration:**
```c
p_instance_ctrl->p_reg->GTBER2_b.CMTCA = 0x1U;  // Transfer enabled
p_instance_ctrl->p_reg->GTBER2_b.CP3DB = 0U;    // Single buffer
p_instance_ctrl->p_reg->GTBER2_b.CPBTD = 0U;    // Normal transfer
```

**Double buffer configuration (Mode 3/4):**
```c
p_instance_ctrl->p_reg->GTBER2_b.CMTCA = 0x1U;  // Transfer enabled
p_instance_ctrl->p_reg->GTBER2_b.CP3DB = 1U;    // Double buffer ON
p_instance_ctrl->p_reg->GTBER2_b.CPBTD = 0U;    // Normal transfer
```

### 3. Timer Modes (Complementary PWM)

| Enum Value | Mode | GTCR.MD | Transfer Timing |
|------------|------|---------|-----------------|
| `TIMER_MODE_COMPLEMENTARY_PWM_MODE1` | Mode 1 | `0xC` | Crest only |
| `TIMER_MODE_COMPLEMENTARY_PWM_MODE2` | Mode 2 | `0xD` | Trough only |
| `TIMER_MODE_COMPLEMENTARY_PWM_MODE3` | Mode 3 | `0xE` | Both crest and trough |
| `TIMER_MODE_COMPLEMENTARY_PWM_MODE4` | Mode 4 | `0xF` | Immediate (bypass) |

### 4. gpt_prv_duty_registers_t Structure

```c
/* r_gpt.c line 94-98 */
typedef struct st_gpt_prv_duty_registers
{
    uint32_t gtccr_buffer;  // Computed duty cycle value
    uint32_t omdty;         // Output mode duty (0%, 100%, or register)
} gpt_prv_duty_registers_t;
```

**Purpose:** Intermediate structure used by `gpt_calculate_duty_cycle()` to compute the compare match value and handle 0%/100% boundary cases.

---

## r_gpt.c — Line-by-Line Analysis

### Complementary PWM Initialization (lines 1419-1434)

This section runs during `R_GPT_Open()` for **each individual GPT channel**.

```c
1419  #if GPT_PRV_EXTRA_FEATURES_ENABLED == GPT_CFG_OUTPUT_SUPPORT_ENABLE
```
**Purpose:** Conditional compilation check. Complementary PWM is only available when "extra features" are enabled (GPT_CFG_OUTPUT_SUPPORT_ENABLE=2).

---

```c
1420          if (p_cfg->mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE1 ||
1421                  p_cfg->mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE2 ||
1422                  p_cfg->mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE3 ||
1423                  p_cfg->mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE4)
```
**Purpose:** Check if the timer is configured for any of the 4 complementary PWM modes.

**Why check all 4 modes:** Even though the modes have different transfer timings (crest, trough, both, immediate), the *initial buffer setup* is identical for single-buffer operation.

---

```c
1424          {
1425              /* Enable buffer transfer GTCCRD→GTCCRA at compare match */
1426              p_instance_ctrl->p_reg->GTBER2_b.CMTCA  = 0x1U;
```
**Line 1426 — GTBER2.CMTCA = 0x1**

| Field | Value | Meaning |
|-------|-------|---------|
| CMTCA | `0x1U` | Enable automatic buffer transfer from GTCCRD to GTCCRA at compare match events |

**Hardware behavior:** When the counter matches GTPR (crest) or reaches 0 (trough), the hardware automatically copies `GTCCRD` → `GTCCRA` if CMTCA=1.

**Critical note:** The *timing* of the transfer (crest vs trough) is controlled by `GTCR.MD` (set earlier at line 1342), not by GTBER2.

---

```c
1427              p_instance_ctrl->p_reg->GTBER2_b.CP3DB  = 0U;   /* double buffer OFF */
```
**Line 1427 — GTBER2.CP3DB = 0**

| Field | Value | Meaning |
|-------|-------|---------|
| CP3DB | `0U` | Disable second buffer stage (GTCCRF). Single buffer mode: GTCCRD → GTCCRA |

**Single buffer chain:**
```
User writes GTCCRD
       ↓ (at compare match event)
  Temp Register A
       ↓ (next PWM cycle)
     GTCCRA (active)
```

**Why default to single buffer:** The r_gpt.c layer handles *individual channels* and doesn't know about three-phase coordination. The three_phase layer will override this to `CP3DB=1` for double-buffer modes.

---

```c
1428              p_instance_ctrl->p_reg->GTBER2_b.CPBTD  = 0U;   /* buffer transfer ENABLED */
```
**Line 1428 — GTBER2.CPBTD = 0**

| Field | Value | Meaning |
|-------|-------|---------|
| CPBTD | `0U` | Buffer transfer enabled (normal operation). GTCCRD transfers to GTCCRA via temp register. |

**Mode 4 clarification:** Despite Mode 4 being called "immediate transfer", `CPBTD=0` is still correct. Mode 4's immediacy is a *hardware behavior* of GTCR.MD=0xF, not controlled by CPBTD. Setting CPBTD=1 would *disable* the buffer chain entirely, which is not what we want.

---

```c
1430              /* Set single buffer registers */
1431              p_instance_ctrl->p_reg->GTCCR[GPT_PRV_GTCCRA] = duty_regs.gtccr_buffer;
```
**Line 1431 — Initialize GTCCRA (Active Register)**

**Purpose:** Set the initial duty cycle in the *active* compare match register so the PWM output starts immediately upon `R_GPT_Start()`.

**Value source:** `duty_regs.gtccr_buffer` was computed earlier (line 1383) by `gpt_calculate_duty_cycle()` from `p_cfg->duty_cycle_counts`.

**Formula (from gpt_calculate_duty_cycle):**
```c
// For triangle wave PWM:
gtccr_buffer = period_counts - duty_cycle_counts;
```

**Example:**
```
period_counts = 1000
duty_cycle_counts = 400  (40% duty)
→ gtccr_buffer = 1000 - 400 = 600
```

---

```c
1432              p_instance_ctrl->p_reg->GTCCR[GPT_PRV_GTCCRD] = duty_regs.gtccr_buffer;
```
**Line 1432 — Initialize GTCCRD (Buffer Register)**

**Purpose:** Pre-load the buffer register with the same value as GTCCRA. This ensures the first buffer transfer doesn't cause a glitch.

**Why both GTCCRA and GTCCRD:** 
- GTCCRA controls the *current* PWM output
- GTCCRD holds the *next* value to be loaded (on the next compare match)

**Startup sequence:**
```
Time 0:  GTCCRA = 600, GTCCRD = 600  ← Both initialized
Time 1:  Counter reaches crest/trough
         Hardware: GTCCRD (600) → Temp A → (next cycle) → GTCCRA (600)
         Result: No change (smooth start)
```

---

```c
1433          }
1434  #endif
```
**End of complementary PWM setup in r_gpt.c.**

At this point, the single GPT channel is configured for complementary PWM with:
- ✅ Mode set (GTCR.MD = 12, 13, 14, or 15)
- ✅ Single buffer enabled (GTBER2: CMTCA=1, CP3DB=0, CPBTD=0)
- ✅ Initial duty loaded (GTCCRA and GTCCRD both set)
- ✅ Dead time configured (GTDVU set later at line 1466 if p_pwm_cfg provided)

---

## r_gpt_three_phase.c — Line-by-Line Analysis

### File Header and Macros

```c
/* Lines 1-6: Copyright header */
```

---

```c
10  #include "r_gpt_three_phase.h"
11  #include "r_gpt_three_phase_cfg.h"
12  
13  #include "r_gpt.h"
14  #include "r_gpt_cfg.h"
```
**Lines 10-14 — Header Includes**

| Header | Purpose |
|--------|---------|
| `r_gpt_three_phase.h` | Three-phase API declarations |
| `r_gpt_three_phase_cfg.h` | Configuration (param checking enable, etc.) |
| `r_gpt.h` | Single-channel GPT API (to call `R_GPT_Open()`) |
| `r_gpt_cfg.h` | GPT configuration (GPT_CFG_OUTPUT_SUPPORT_ENABLE) |

---

```c
20  /* "GPT3" in ASCII, used to determine if channel is open. */
21  #define GPT_THREE_PHASE_OPEN  (0x47505433ULL)
```
**Line 21 — Open Flag Magic Number**

**Purpose:** Stored in `p_instance_ctrl->open` to detect if the module has been initialized.

**Value breakdown:**
```
0x47505433 = "GPT3" in ASCII
  0x47 = 'G'
  0x50 = 'P'
  0x54 = 'T'
  0x33 = '3'
```

**Usage:**
```c
FSP_ERROR_RETURN(GPT_THREE_PHASE_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
```

---

```c
23  #define GPT_THREE_PHASE_PRV_GTWP_RESET_VALUE    (0xA500U)
24  #define GPT_THREE_PHASE_PRV_GTWP_WRITE_PROTECT  (0xA501U)
```
**Lines 23-24 — Write Protection Values**

| Macro | Value | GTWP Register Effect |
|-------|-------|----------------------|
| `RESET_VALUE` | `0xA500U` | Disable write protection (allow register writes) |
| `WRITE_PROTECT` | `0xA501U` | Enable write protection (block register writes) |

**Purpose:** Protect GPT registers from accidental modification. Must disable (0xA500) before writing, then re-enable (0xA501) after.

**Pattern:**
```c
p_reg->GTWP = 0xA500U;      // Unlock
p_reg->GTCCR[...] = value;  // Modify
p_reg->GTWP = 0xA501U;      // Lock
```

---

```c
26  #define GPT_THREE_PHASE_PRV_GTBER_SINGLE_BUFFER  (0x50000U)
27  #define GPT_THREE_PHASE_PRV_GTBER_DOUBLE_BUFFER  (0xA0000U)
```
**Lines 26-27 — GTBER Register Preset Values**

⚠️ **WARNING:** These are GTBER register values, **NOT** the `three_phase_buffer_mode_t` enum values.

| Macro | Value | Meaning |
|-------|-------|---------|
| `SINGLE_BUFFER` | `0x50000U` | GTBER bits for single-buffer triangle PWM (legacy, not used in fixed code) |
| `DOUBLE_BUFFER` | `0xA0000U` | GTBER bits for double-buffer triangle PWM |

**Bug history (FSPRA-5725):** The old code incorrectly compared `buffer_mode` (enum = 1 or 2) against these register values (0x50000). Fixed code uses the enum directly.

---

```c
35  enum e_gpt_three_phase_prv_gtccr
36  {
37      GPT_THREE_PHASE_PRV_GTCCRA = 0U,
38      GPT_THREE_PHASE_PRV_GTCCRB,
39      GPT_THREE_PHASE_PRV_GTCCRC,
40      GPT_THREE_PHASE_PRV_GTCCRE,
41      GPT_THREE_PHASE_PRV_GTCCRD,
42      GPT_THREE_PHASE_PRV_GTCCRF
43  };
```
**Lines 35-43 — GTCCR Array Index Enum**

**Purpose:** Same as `GPT_PRV_GTCCR*` in r_gpt.c, but scoped to this file.

**Why duplicate:** Avoids cross-file dependencies. Each file defines its own enum for accessing `p_reg->GTCCR[index]`.

---

### R_GPT_THREE_PHASE_Open() Function

```c
95  fsp_err_t R_GPT_THREE_PHASE_Open (three_phase_ctrl_t * const p_ctrl, 
                                      three_phase_cfg_t const * const p_cfg)
96  {
97      gpt_three_phase_instance_ctrl_t * p_instance_ctrl = (gpt_three_phase_instance_ctrl_t *) p_ctrl;
```
**Line 97 — Type Cast**

**Purpose:** Convert the generic `void*` control pointer to the concrete type.

**Pattern:** All FSP drivers use void pointers in the API to maintain ABI compatibility, then cast internally.

---

```c
98  #if GPT_THREE_PHASE_CFG_PARAM_CHECKING_ENABLE
99      FSP_ASSERT(NULL != p_cfg);
100     FSP_ASSERT(NULL != p_instance_ctrl);
101     FSP_ERROR_RETURN(GPT_THREE_PHASE_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
102 #endif
```
**Lines 98-102 — Parameter Validation**

| Check | Error Condition |
|-------|----------------|
| Line 99 | `p_cfg == NULL` → FSP_ERR_ASSERTION |
| Line 100 | `p_instance_ctrl == NULL` → FSP_ERR_ASSERTION |
| Line 101 | `p_instance_ctrl->open == GPT_THREE_PHASE_OPEN` → FSP_ERR_ALREADY_OPEN |

**Conditional compilation:** Only active if `GPT_THREE_PHASE_CFG_PARAM_CHECKING_ENABLE` is defined.

---

```c
107     for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
108     {
109         err = R_GPT_Open(p_cfg->p_timer_instance[ch]->p_ctrl, 
                            p_cfg->p_timer_instance[ch]->p_cfg);
```
**Lines 107-109 — Loop: Initialize Each GPT Channel**

**Iteration order:**
```
ch = THREE_PHASE_CHANNEL_U (0)  → U phase (master)
ch = THREE_PHASE_CHANNEL_V (1)  → V phase (slave 1)
ch = THREE_PHASE_CHANNEL_W (2)  → W phase (slave 2)
```

**Line 109 — Call R_GPT_Open() [r_gpt.c]**

This triggers the entire single-channel initialization sequence in r_gpt.c, including:
- Setting GTCR.MD (complementary PWM mode)
- Configuring GTBER2 (single buffer by default)
- Initializing GTCCRA, GTCCRD
- Setting up dead time (GTDVU)

---

```c
110 #if GPT_CFG_PARAM_CHECKING_ENABLE
111         if (err)
112         {
113             /* In case of an error on GPT open, close all opened instances and return the error */
114             for (three_phase_channel_t close_ch = THREE_PHASE_CHANNEL_U; close_ch < ch; close_ch++)
115             {
116                 R_GPT_Close(p_cfg->p_timer_instance[close_ch]->p_ctrl);
117             }
118
119             return err;
120         }
121 #endif
```
**Lines 110-121 — Error Handling with Rollback**

**Purpose:** If opening channel V or W fails, close any previously-opened channels (U, or U+V) to avoid resource leaks.

**Example scenario:**
```
Opening U: ✅ Success
Opening V: ✅ Success
Opening W: ❌ Fail (err = FSP_ERR_IN_USE)
→ Rollback: Close V, Close U
→ Return FSP_ERR_IN_USE to caller
```

---

```c
127         p_instance_ctrl->p_reg[ch] = ((gpt_instance_ctrl_t *) 
                                          (p_cfg->p_timer_instance[ch]->p_ctrl))->p_reg;
```
**Line 127 — Save Register Base Pointer**

**Purpose:** Cache the hardware register base address for fast access.

**Pointer chain:**
```
p_cfg->p_timer_instance[ch]       → timer_instance_t (FSP structure)
  ->p_ctrl                         → gpt_instance_ctrl_t* (driver state)
    ->p_reg                        → R_GPT0_Type* (hardware registers)
```

**Result:** `p_instance_ctrl->p_reg[ch]` directly accesses GPT channel hardware (e.g., `p_reg[0]` = GPT0, `p_reg[1]` = GPT1).

---

```c
129 #if GPT_CFG_WRITE_PROTECT_ENABLE
131         p_instance_ctrl->p_reg[ch]->GTWP = GPT_THREE_PHASE_PRV_GTWP_RESET_VALUE;
132 #endif
```
**Line 131 — Disable Write Protection**

**Purpose:** Unlock GPT registers so subsequent writes (GTBER, GTCCR) succeed.

**Value:** `0xA500U` = "allow writes"

---

```c
136         if (THREE_PHASE_BUFFER_MODE_DOUBLE == p_cfg->buffer_mode)
137         {
138             p_instance_ctrl->p_reg[ch]->GTBER |= GPT_THREE_PHASE_PRV_GTBER_DOUBLE_BUFFER;
139         }
```
**Lines 136-139 — Enable Double-Buffer for Standard Triangle PWM**

⚠️ **Note:** This is for **standard triangle-wave PWM** (TIMER_MODE_TRIANGLE_WAVE_*), not complementary PWM.

**Purpose:** Sets GTBER bits to enable GTCCRC/GTCCRE double-buffering for triangle wave modes.

**Not used in complementary PWM:** Complementary PWM uses GTBER2 (configured next), not GTBER.

---

### Complementary PWM Buffer Override (Lines 141-185)

This is the **critical section** fixed in FSPRA-5725.

```c
141 #if (1 == GPT_CFG_OUTPUT_SUPPORT_ENABLE)
142
143         /* Configure complementary PWM buffer chain for this channel.
144          * R_GPT_Open() already configured single buffer (GTBER2: CMTCA=1, CP3DB=0, CPBTD=0).
145          * Here we override for double buffer when applicable, and set initial duty values.
146          *
147          * Bug fixes applied (FSPRA-5725):
148          *  - Removed misplaced gpt_prv_duty_registers_t struct (belongs in r_gpt.c only)
149          *  - Removed wrong cast of p_cfg->p_extend to gpt_extended_cfg_t*
150          *  - Removed call to static gpt_gtior_calculate() (GTIOR is r_gpt.c's responsibility)
151          *  - Fixed use of p_instance_ctrl->buffer_mode before assignment (now uses p_cfg->buffer_mode)
152          *  - Fixed buffer_mode comparison against GTBER register value (0x50000) instead of enum
153          */
```
**Lines 141-153 — Documentation Header**

**Purpose:** Explains what this block does and documents the bugs that were fixed.

**Key insight:** r_gpt.c already set up single buffer. This block *overrides* to double buffer if needed.

---

```c
154         timer_mode_t mode = p_cfg->p_timer_instance[0]->p_cfg->mode;
```
**Line 154 — Read Timer Mode from Master Channel**

**Why index [0]:** All three channels (U, V, W) must use the same mode. We read from channel 0 (master) as the canonical source.

**Assumption:** FSP configuration generator ensures all three `p_timer_instance[]` have identical `mode` values.

---

```c
156         if (mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE1 ||
157             mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE2 ||
158             mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE3 ||
159             mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE4)
160         {
```
**Lines 156-160 — Check for Complementary PWM Mode**

**Purpose:** Only execute the override logic if the timer is in a complementary PWM mode.

**Why check here:** The three_phase layer supports both:
- Standard triangle-wave 3-phase PWM (TIMER_MODE_TRIANGLE_WAVE_*)
- Complementary PWM 3-phase (TIMER_MODE_COMPLEMENTARY_PWM_*)

This `if` block isolates the complementary-specific logic.

---

```c
161             uint32_t duty_initial = p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts;
```
**Line 161 — Read Initial Duty Cycle for This Channel**

**Purpose:** Each of the three phases (U, V, W) can have a different initial duty cycle.

**Value source:** Comes from FSP configuration (typically set to period/2 for 50% duty at startup).

**Why per-channel:** Allows U, V, W to start with different phase shifts if needed (though typically all start at the same duty).

---

```c
163             if ((THREE_PHASE_BUFFER_MODE_DOUBLE == p_cfg->buffer_mode) &&
164                 (mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE3 ||
165                  mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE4))
166             {
```
**Lines 163-166 — Check for Double Buffer Eligibility**

**Condition:** Both of the following must be true:
1. `buffer_mode == THREE_PHASE_BUFFER_MODE_DOUBLE` (user selected double buffer)
2. Mode is 3 or 4 (only these support double buffer in complementary PWM)

**Why only Mode 3/4:** Modes 1 and 2 transfer at only one boundary (crest or trough), so a second buffer stage provides no benefit. Mode 3/4 transfer at both boundaries (or immediately), benefiting from the extra buffer.

---

```c
167                 /* Double buffer: GTCCRF -> Temp B -> GTCCRE -> GTCCRA */
168                 p_instance_ctrl->p_reg[ch]->GTBER2_b.CMTCA = 0x1U;
169                 p_instance_ctrl->p_reg[ch]->GTBER2_b.CP3DB = 1U;   /* double buffer ON */
170                 p_instance_ctrl->p_reg[ch]->GTBER2_b.CPBTD = 0U;   /* buffer transfer ENABLED */
```
**Lines 167-170 — Configure Double Buffer Chain**

| Line | Register Field | Value | Meaning |
|------|----------------|-------|---------|
| 168 | GTBER2.CMTCA | `0x1U` | Enable automatic buffer transfer |
| 169 | **GTBER2.CP3DB** | `1U` | **Enable second buffer stage (GTCCRF)** |
| 170 | GTBER2.CPBTD | `0U` | Normal transfer (not disabled) |

**Line 169 is the override:** r_gpt.c set `CP3DB=0`. This line changes it to `CP3DB=1`.

**Double buffer chain:**
```
User writes GTCCRF
       ↓ (at crest)
  Temp Register B
       ↓ (at trough)
     GTCCRE
       ↓ (next cycle)
     GTCCRA (active)
```

---

```c
172                 /* Initialize active register + both buffer stages */
173                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRA] = duty_initial;
174                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = duty_initial;
175                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRF] = duty_initial;
```
**Lines 173-175 — Initialize All Three Registers**

| Register | Role | Why Initialize |
|----------|------|----------------|
| GTCCRA (line 173) | Active compare match | Controls current PWM output |
| GTCCRD (line 174) | First buffer stage | Transfers to GTCCRA on next match |
| GTCCRF (line 175) | Second buffer stage | Transfers to GTCCRE, then to GTCCRA |

**Purpose:** Pre-load all registers with the same value to avoid glitches during the first few PWM cycles.

**Note:** r_gpt.c already initialized GTCCRA and GTCCRD (lines 1431-1432). This code *overwrites* them with `duty_initial` from the three_phase config and adds GTCCRF.

---

```c
176             }
177             else
178             {
179                 /* Single buffer (Modes 1-4): GTCCRD -> Temp A -> GTCCRA
180                  * GTBER2 already configured by R_GPT_Open; set initial duty values */
181                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRA] = duty_initial;
182                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = duty_initial;
183             }
```
**Lines 177-183 — Single Buffer Path**

**Triggered when:**
- `buffer_mode == SINGLE`, OR
- Mode is 1 or 2 (don't support double buffer)

**Lines 181-182:** Re-initialize GTCCRA and GTCCRD with `duty_initial`.

**Why re-initialize:** r_gpt.c used `duty_regs.gtccr_buffer` (computed from its own config). The three_phase layer may have a different `duty_initial` value per channel.

**No GTCCRF initialization:** Single buffer doesn't use GTCCRF.

---

```c
184         }
185 #endif
```
**End of complementary PWM override block.**

---

```c
187 #if GPT_CFG_WRITE_PROTECT_ENABLE
189         p_instance_ctrl->p_reg[ch]->GTWP = GPT_THREE_PHASE_PRV_GTWP_WRITE_PROTECT;
190 #endif
```
**Line 189 — Re-Enable Write Protection**

**Purpose:** Lock the GPT registers now that initialization is complete.

**Value:** `0xA501U` = "block writes"

---

```c
192     }  // End of for loop (ch = U, V, W)
194     /* Get copy of GPT channel mask and buffer mode */
195     p_instance_ctrl->channel_mask = p_cfg->channel_mask;
196     p_instance_ctrl->buffer_mode  = p_cfg->buffer_mode;
```
**Lines 195-196 — Store Configuration in Instance Control**

| Field | Purpose |
|-------|---------|
| `channel_mask` | Bitmask of which GPT channels are used (e.g., 0x07 = channels 0,1,2) |
| `buffer_mode` | Single (1) or Double (2) buffer mode |

**Important:** Line 196 is the **first assignment** of `p_instance_ctrl->buffer_mode`. This is why lines 163-165 use `p_cfg->buffer_mode` instead.

---

```c
198     /* Save pointer to config struct */
199     p_instance_ctrl->p_cfg = p_cfg;
200
201     p_instance_ctrl->open = GPT_THREE_PHASE_OPEN;
202
203     return FSP_SUCCESS;
```
**Lines 198-203 — Finalize Initialization**

| Line | Action |
|------|--------|
| 199 | Store config pointer for later reference |
| 201 | Mark module as open (magic number `0x47505433`) |
| 203 | Return success |

---

### R_GPT_THREE_PHASE_DutyCycleSet() Function

```c
295 fsp_err_t R_GPT_THREE_PHASE_DutyCycleSet (three_phase_ctrl_t * const p_ctrl,
296                                           three_phase_duty_cycle_t * const p_duty_cycle)
297 {
298     gpt_three_phase_instance_ctrl_t * p_instance_ctrl = (gpt_three_phase_instance_ctrl_t *) p_ctrl;
```

---

```c
299 #if GPT_THREE_PHASE_CFG_PARAM_CHECKING_ENABLE
300     FSP_ASSERT(NULL != p_instance_ctrl);
301     FSP_ASSERT(NULL != p_duty_cycle);
302     FSP_ERROR_RETURN(GPT_THREE_PHASE_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
303
304     /* Check that duty cycle values are in-range (less than period) */
305     for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
306     {
307         uint32_t gtpr = p_instance_ctrl->p_reg[THREE_PHASE_CHANNEL_U]->GTPR;
308         FSP_ERROR_RETURN((p_duty_cycle->duty[ch] < gtpr) && (p_duty_cycle->duty[ch] > 0), 
                           FSP_ERR_INVALID_ARGUMENT);
309
310         /* In double-buffer mode also check double buffer */
311         if (THREE_PHASE_BUFFER_MODE_DOUBLE == p_instance_ctrl->buffer_mode)
312         {
313             FSP_ERROR_RETURN((p_duty_cycle->duty_buffer[ch] < gtpr) && 
                                (p_duty_cycle->duty_buffer[ch] > 0),
314                              FSP_ERR_INVALID_ARGUMENT);
315         }
316     }
317 #endif
```
**Lines 304-316 — Validate Duty Cycle Values**

| Check | Condition |
|-------|-----------|
| Line 308 | `0 < duty[ch] < GTPR` for all channels |
| Line 313 | `0 < duty_buffer[ch] < GTPR` (if double buffer mode) |

**Why exclude 0 and GTPR:** These are boundary cases handled separately via the GTUDDTYC register (not shown in this function).

---

```c
319     r_gpt_write_protect_disable_all(p_instance_ctrl);
```
**Line 319 — Unlock All Three Channels**

**Purpose:** Call helper function to write `GTWP = 0xA500` for U, V, W channels.

---

```c
321     /* Determine operating mode from the master (U) channel */
322     timer_mode_t mode = p_instance_ctrl->p_cfg->p_timer_instance[0]->p_cfg->mode;
```
**Line 322 — Read Operating Mode**

**Why needed:** Complementary PWM and standard triangle PWM use different buffer registers (GTCCRD/GTCCRF vs GTCCRC/GTCCRE).

---

```c
324     if (mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE1 ||
325         mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE2 ||
326         mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE3 ||
327         mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE4)
328     {
```
**Lines 324-328 — Branch: Complementary PWM**

---

```c
329         /* Complementary PWM: write duty to GTCCRD buffer register.
330          * REQ-BUF-16: Write W channel (slave 2, index 2) LAST to trigger
331          * simultaneous temporary-register transfer across all three channels. */
332         static const three_phase_channel_t write_order[3] =
333         {
334             THREE_PHASE_CHANNEL_U, THREE_PHASE_CHANNEL_V, THREE_PHASE_CHANNEL_W
335         };
```
**Lines 329-335 — Define Write Order**

**Purpose:** Enforce that W channel (slave 2) is written last.

**Hardware behavior:** Writing to the *second slave channel's* GTCCRD triggers all three temp registers to latch their new values simultaneously. This ensures phase-aligned updates.

**Array order:**
```
write_order[0] = U (master)
write_order[1] = V (slave 1)
write_order[2] = W (slave 2) ← written last
```

---

```c
337         for (uint32_t i = 0U; i < 3U; i++)
338         {
339             three_phase_channel_t ch = write_order[i];
340
341             /* Single buffer: GTCCRD -> Temp A -> GTCCRA */
342             p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = p_duty_cycle->duty[ch];
```
**Lines 337-342 — Write GTCCRD for Each Channel**

**Line 342 — Update Buffer Register**

**Buffer chain:**
```
User: GTCCRD = new_duty
       ↓ (at next compare match)
  Temp Register A = new_duty
       ↓ (at following PWM cycle)
     GTCCRA = new_duty (output changes)
```

**Delay:** 2 PWM cycles from write to output change (due to double buffer in Mode 3).

---

```c
344             /* Double buffer: additionally write GTCCRF -> Temp B -> GTCCRE -> GTCCRA */
345             if (THREE_PHASE_BUFFER_MODE_DOUBLE == p_instance_ctrl->buffer_mode)
346             {
347                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRF] = p_duty_cycle->duty_buffer[ch];
348             }
```
**Lines 344-348 — Write GTCCRF for Double Buffer**

**Line 347 — Update Second Buffer Stage**

**Purpose:** In double buffer mode, `duty[]` transfers at one boundary (e.g., crest) and `duty_buffer[]` transfers at the other (e.g., trough).

**Example use case (asymmetric PWM):**
```
duty[U] = 300          → Transfers at crest
duty_buffer[U] = 700   → Transfers at trough
→ Duty cycle changes twice per PWM period
```

---

```c
349         }
350     }
351     else
352     {
353         /* Standard triangle-wave PWM: write to GTCCRC/GTCCRE (original behavior) */
354         for (three_phase_channel_t ch = THREE_PHASE_CHANNEL_U; ch <= THREE_PHASE_CHANNEL_W; ch++)
355         {
356             p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRC] = p_duty_cycle->duty[ch];
357             p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRE] = p_duty_cycle->duty[ch];
```
**Lines 351-357 — Branch: Standard Triangle PWM**

**Lines 356-357:** Write to GTCCRC and GTCCRE (the buffer registers used by standard triangle modes).

**No write ordering:** Standard triangle PWM doesn't have the W-channel-last constraint.

---

```c
359             if ((THREE_PHASE_BUFFER_MODE_DOUBLE == p_instance_ctrl->buffer_mode) ||
360                 (TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3 == mode))
361             {
362                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = p_duty_cycle->duty_buffer[ch];
363                 p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRF] = p_duty_cycle->duty_buffer[ch];
364             }
```
**Lines 359-364 — Standard Triangle Double Buffer**

**Purpose:** For asymmetric triangle wave mode 3, also write the double buffer registers (GTCCRD, GTCCRF).

---

```c
366         }
367     }
368
369     r_gpt_write_protect_enable_all(p_instance_ctrl);
370
371     return FSP_SUCCESS;
```
**Lines 369-371 — Lock and Return**

**Line 369:** Re-enable write protection on all three channels.

---

## Buffer Chain Mechanics

### Single Buffer Chain (Modes 1-4 Default)

```
┌──────────────────────────────────────────────────────────┐
│  Application Layer                                       │
│  p_reg->GTCCR[GTCCRD] = new_duty;                        │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  GTCCRD Register  (buffer, writable anytime)             │
│  Value: new_duty                                         │
└────────────────────┬─────────────────────────────────────┘
                     │
                     │ (At compare match: crest or trough
                     │  depending on GTCR.MD)
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  Temporary Register A  (hardware internal, not visible)  │
│  Value: new_duty                                         │
└────────────────────┬─────────────────────────────────────┘
                     │
                     │ (At next PWM cycle start)
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  GTCCRA Register  (active compare match)                 │
│  Value: new_duty → PWM output changes                    │
└──────────────────────────────────────────────────────────┘
```

**Delay from write to output:** 1-2 PWM cycles (depends on when compare match occurs).

---

### Double Buffer Chain (Modes 3-4 with buffer_mode=DOUBLE)

```
┌──────────────────────────────────────────────────────────┐
│  Application Layer                                       │
│  p_reg->GTCCR[GTCCRF] = duty_buffer;                     │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  GTCCRF Register  (second buffer stage)                  │
│  Value: duty_buffer                                      │
└────────────────────┬─────────────────────────────────────┘
                     │
                     │ (At compare match: crest)
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  Temporary Register B                                    │
│  Value: duty_buffer                                      │
└────────────────────┬─────────────────────────────────────┘
                     │
                     │ (At next compare match: trough)
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  GTCCRE Register  (first buffer stage)                   │
│  Value: duty_buffer                                      │
└────────────────────┬─────────────────────────────────────┘
                     │
                     │ (At next PWM cycle)
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  GTCCRA Register  (active)                               │
│  Value: duty_buffer → PWM output changes                 │
└──────────────────────────────────────────────────────────┘
```

**Delay from write to output:** 2-3 PWM cycles.

**Purpose of double buffer:** Allows updating both crest and trough duty values independently in asymmetric PWM.

---

## Usage Examples

### Example 1: Initialize Complementary PWM (Single Buffer, Mode 3)

```c
/* FSP Configuration (hal_data.c generated) */
const gpt_extended_pwm_cfg_t g_comp_pwm_cfg =
{
    .dead_time_count_up = 64,   // 64 timer counts dead time
    .poeg_link          = GPT_POEG_LINK_POEG0,
    .output_disable     = GPT_OUTPUT_DISABLE_NONE
};

const gpt_extended_cfg_t g_comp_pwm_extend =
{
    .p_pwm_cfg = &g_comp_pwm_cfg,
    /* ... other fields ... */
};

const timer_cfg_t g_timer_comp_pwm_master_cfg =
{
    .mode            = TIMER_MODE_COMPLEMENTARY_PWM_MODE3,  // Both boundaries
    .period_counts   = 2000,
    .duty_cycle_counts = 1000,  // 50% initial duty
    .channel         = 0,
    .p_extend        = &g_comp_pwm_extend
};

/* Three-phase configuration */
const timer_instance_t * g_timer_instances[3] =
{
    &g_timer_comp_pwm_master,
    &g_timer_comp_pwm_slave1,
    &g_timer_comp_pwm_slave2
};

const three_phase_cfg_t g_three_phase_cfg =
{
    .buffer_mode        = THREE_PHASE_BUFFER_MODE_SINGLE,
    .p_timer_instance   = g_timer_instances,
    .channel_mask       = 0x07,  // GPT0, GPT1, GPT2
    .callback_ch        = THREE_PHASE_CHANNEL_U
};

/* Application code */
gpt_three_phase_instance_ctrl_t g_three_phase_ctrl;

void motor_init(void)
{
    fsp_err_t err;
    
    // Open three-phase complementary PWM
    err = R_GPT_THREE_PHASE_Open(&g_three_phase_ctrl, &g_three_phase_cfg);
    assert(FSP_SUCCESS == err);
    
    // Start PWM output
    err = R_GPT_THREE_PHASE_Start(&g_three_phase_ctrl);
    assert(FSP_SUCCESS == err);
}
```

**What happens internally:**

1. `R_GPT_THREE_PHASE_Open()` calls `R_GPT_Open()` three times (U, V, W)
2. Each `R_GPT_Open()` sets:
   - GTCR.MD = 0xE (Mode 3)
   - GTBER2 = {CMTCA=1, CP3DB=0, CPBTD=0}  ← Single buffer
   - GTCCRA = GTCCRD = 1000
   - GTDVU = 64
3. Three-phase layer doesn't override (single buffer requested)
4. `R_GPT_THREE_PHASE_Start()` sets GTSTR to start all three channels

---

### Example 2: Update Duty Cycle at Runtime

```c
void motor_set_duty(uint32_t duty_u, uint32_t duty_v, uint32_t duty_w)
{
    three_phase_duty_cycle_t duty;
    
    duty.duty[THREE_PHASE_CHANNEL_U] = duty_u;
    duty.duty[THREE_PHASE_CHANNEL_V] = duty_v;
    duty.duty[THREE_PHASE_CHANNEL_W] = duty_w;
    
    // Not used in single buffer mode
    duty.duty_buffer[0] = 0;
    duty.duty_buffer[1] = 0;
    duty.duty_buffer[2] = 0;
    
    fsp_err_t err = R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_ctrl, &duty);
    assert(FSP_SUCCESS == err);
}

/* Call from ISR or main loop */
motor_set_duty(800, 1000, 1200);  // U=40%, V=50%, W=60%
```

**What happens internally:**

1. `DutyCycleSet()` detects complementary PWM mode
2. Writes in order: U → V → W (W last triggers simultaneous latch)
3. For each channel:
   ```c
   GTCCRD[U] = 800;
   GTCCRD[V] = 1000;
   GTCCRD[W] = 1200;  ← All temp registers latch on this write
   ```
4. At next compare match (crest or trough):
   - Hardware: GTCCRD → Temp A
5. At following PWM cycle:
   - Hardware: Temp A → GTCCRA
   - **PWM output changes**

---

### Example 3: Double Buffer Mode (Asymmetric Control)

```c
const three_phase_cfg_t g_three_phase_cfg_double =
{
    .buffer_mode = THREE_PHASE_BUFFER_MODE_DOUBLE,  // Enable double buffer
    /* ... rest same as Example 1 ... */
};

void motor_init_asymmetric(void)
{
    R_GPT_THREE_PHASE_Open(&g_three_phase_ctrl, &g_three_phase_cfg_double);
    R_GPT_THREE_PHASE_Start(&g_three_phase_ctrl);
}

void motor_set_asymmetric_duty(uint32_t crest_duty, uint32_t trough_duty)
{
    three_phase_duty_cycle_t duty;
    
    // All three phases use same asymmetric pattern
    duty.duty[0] = duty.duty[1] = duty.duty[2] = crest_duty;
    duty.duty_buffer[0] = duty.duty_buffer[1] = duty.duty_buffer[2] = trough_duty;
    
    R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase_ctrl, &duty);
}

motor_set_asymmetric_duty(600, 1400);  // 30% at crest, 70% at trough
```

**What happens internally:**

1. `R_GPT_THREE_PHASE_Open()`:
   - Calls `R_GPT_Open()` three times → single buffer set
   - **Overrides:** Sets GTBER2.CP3DB = 1 (double buffer ON)
   - Initializes GTCCRF alongside GTCCRD
2. `DutyCycleSet()`:
   ```c
   GTCCRD[ch] = 600;   // Transfers at one boundary
   GTCCRF[ch] = 1400;  // Transfers at other boundary
   ```
3. PWM output alternates between 30% and 70% duty each half-cycle

---

## Common Pitfalls

### ❌ Pitfall 1: Using GTBER Instead of GTBER2

```c
// WRONG - GTBER is for standard triangle PWM
p_instance_ctrl->p_reg->GTBER = 0x50000U;
```

**Fix:** Use GTBER2 for complementary PWM:
```c
// CORRECT
p_instance_ctrl->p_reg->GTBER2_b.CMTCA = 0x1U;
p_instance_ctrl->p_reg->GTBER2_b.CP3DB = 0U;
```

---

### ❌ Pitfall 2: Writing to Wrong GTCCR Register

```c
// WRONG - GTCCRC is for standard triangle PWM
p_reg->GTCCR[GPT_THREE_PHASE_PRV_GTCCRC] = new_duty;
```

**Fix:** Use GTCCRD for complementary PWM:
```c
// CORRECT
p_reg->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = new_duty;
```

---

### ❌ Pitfall 3: Comparing buffer_mode Against Register Value

```c
// WRONG - Comparing enum (1 or 2) against GTBER register value (0x50000)
if (GPT_THREE_PHASE_PRV_GTBER_SINGLE_BUFFER == p_instance_ctrl->buffer_mode)
```

**Fix:** Compare against enum values:
```c
// CORRECT
if (THREE_PHASE_BUFFER_MODE_SINGLE == p_cfg->buffer_mode)
```

---

### ❌ Pitfall 4: Not Writing W Channel Last

```c
// WRONG - Random order
for (ch = 0; ch < 3; ch++)
{
    p_reg[ch]->GTCCR[GTCCRD] = duty[ch];
}
```

**Fix:** Enforce W-last ordering:
```c
// CORRECT
static const three_phase_channel_t order[] = {U, V, W};
for (i = 0; i < 3; i++)
{
    ch = order[i];  // W written last
    p_reg[ch]->GTCCR[GTCCRD] = duty[ch];
}
```

---

### ❌ Pitfall 5: Forgetting Write Protection

```c
// WRONG - Writes may be blocked
p_reg->GTCCR[GTCCRD] = value;
```

**Fix:** Unlock → Write → Lock:
```c
// CORRECT
p_reg->GTWP = 0xA500U;              // Unlock
p_reg->GTCCR[GTCCRD] = value;       // Write
p_reg->GTWP = 0xA501U;              // Lock
```

---

## Summary

### Key Takeaways

1. **Layered architecture:** `r_gpt.c` handles single channels, `r_gpt_three_phase.c` aggregates three channels
2. **Initialization flow:** Three_phase calls `R_GPT_Open()` 3 times, then overrides for double buffer
3. **Buffer chain:** Single = GTCCRD → Temp A → GTCCRA; Double = GTCCRF → Temp B → GTCCRE → GTCCRA
4. **W-channel-last:** Writing W channel's GTCCRD triggers simultaneous phase-aligned update
5. **Mode-specific logic:** Only Mode 3/4 support double buffer; all modes use GTBER2 for buffer control

### Quick Reference

| Task | Register/API |
|------|--------------|
| Initialize complementary PWM | `R_GPT_THREE_PHASE_Open()` |
| Update duty cycle | `R_GPT_THREE_PHASE_DutyCycleSet()` |
| Enable single buffer | GTBER2.CP3DB = 0 |
| Enable double buffer | GTBER2.CP3DB = 1 |
| Set duty (single buffer) | Write GTCCRD |
| Set duty (double buffer) | Write GTCCRD + GTCCRF |
| Configure dead time | GTDVU register (in r_gpt.c) |
| Unlock registers | GTWP = 0xA500 |
| Lock registers | GTWP = 0xA501 |

---

**Document Version:** 1.0  
**Last Updated:** 2025-01-20  
**Related:** FSPRA-5725, FSP v6.5.0 rc0
