# Comparative Analysis: Complementary PWM Mode on RA2T1 vs RA6T2 vs RA8T2

---

## 1. Hardware Architecture Overview

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **CPU Core** | Arm Cortex-M23 | Arm Cortex-M33 | Arm Cortex-M85 + Cortex-M33 |
| **Max Frequency** | 64 MHz | 240 MHz | 1 GHz (M85) + 250 MHz (M33) |
| **GPT Timer Width** | **16-bit** (GPT16) | **32-bit** (GPT32) | **32-bit** (GPT32) |
| **Total GPT Channels** | 4 (GPT160–GPT163) | 10 (GPT320–GPT329) | 14 (GPT320–GPT3213) |
| **GPT Core Clock Source** | PCLKD only (synchronous) | PCLKD (sync) or GPTCLK (async) | PCLKD (sync) or GPTCLK (async) |
| **Max PCLKD** | 64 MHz | 120 MHz | 200 MHz |
| **GPTCLK (async)** | Not supported | Yes (up to 200 MHz) | Yes (up to 160 MHz+) |
| **POEG Groups** | 2 (A, B) | 4 (A–D) | 4 (A–D) |
| **PDG (PWM Delay Generation)** | Yes (4 channels, CH0–3) | Yes (4 channels, CH0–3) | Yes (4 channels, CH0–3) |

---

## 2. Channel Assignment for Complementary PWM Mode

This is **the most important difference** when porting code between the 3 devices.

### RA2T1: Channel 0 is Master

| Role | Channel | Module Name |
|---|---|---|
| Master | CH0 | GPT160 |
| Slave 1 | CH1 | GPT161 |
| Slave 2 | CH2 | GPT162 |

- Only **1 complementary PWM channel group** (CH0, CH1, CH2).
- CH3 (GPT163) does **NOT** support complementary PWM — only supports phase counting and external pulse width measuring.
- Saw-wave PWM mode 2 is also only supported on CH0–CH2 (not on CH3).
- Asymmetric automatic dead time: CH0–CH2; Symmetric automatic dead time: CH3 only.

### RA6T2: Channel 4 or 7 is Master

| Group | Master | Slave 1 | Slave 2 |
|---|---|---|---|
| Group 1 | CH4 (GPT324) | CH5 (GPT325) | CH6 (GPT326) |
| Group 2 | CH7 (GPT327) | CH8 (GPT328) | CH9 (GPT329) |

- **2 complementary PWM channel groups** available.
- CH0–CH3 (GPT320–GPT323): do **NOT** support complementary PWM (MD[3] bit not available). Only support Saw-wave PWM mode 1, Triangle-wave PWM mode. In exchange, CH0–CH3 have **High Resolution PWM**, phase counting, and external pulse width measuring.
- CH4–CH9: Full support for complementary PWM mode 1/2/3/4 (MD[3] bit available).

### RA8T2: Channel 4 or 7 is Master (same as RA6T2)

| Group | Master | Slave 1 | Slave 2 |
|---|---|---|---|
| Group 1 | CH4 (GPT324) | CH5 (GPT325) | CH6 (GPT326) |
| Group 2 | CH7 (GPT327) | CH8 (GPT328) | CH9 (GPT329) |

- **2 complementary PWM channel groups** (same as RA6T2).
- CH0–CH3 and **CH10–CH13** (GPT3210–GPT3213): do **NOT** support complementary PWM (MD[3] bit reads as 0). Only support Saw-wave PWM mode 1 and Triangle-wave PWM mode.
- CH4–CH9: Full support for complementary PWM mode 1/2/3/4.

**Porting constraint:**
- RA2T1 code using CH0 (master), CH1 (slave1), CH2 (slave2) → **must change to CH4/CH5/CH6 or CH7/CH8/CH9** when porting to RA6T2/RA8T2.
- RA2T1 uses base address `GPT16n`, RA6T2/RA8T2 use `GPT32n` → different register offsets and sizes.

---

## 3. Timer Counter Width — Direct Impact on Resolution

| Property | RA2T1 (16-bit) | RA6T2 (32-bit) | RA8T2 (32-bit) |
|---|---|---|---|
| **Max counter value (GTPR)** | 0xFFFF (65,535) | 0xFFFFFFFF (~4.29 billion) | 0xFFFFFFFF (~4.29 billion) |
| **PWM frequency resolution** | Lower (fewer steps) | Very high | Very high |
| **Duty cycle resolution** | Max 65,535 steps | Max ~4.29 billion steps | Max ~4.29 billion steps |
| **Register size** | 16-bit registers | 32-bit registers | 32-bit registers |
| **GTDVU + GTPR constraint** | GTDVU + GTPR ≤ 0xFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF |

**Real-world example:**
- At 64 MHz clock, 20 kHz PWM → RA2T1: GTPR = 3200 (only 3200 duty steps, ~0.03% resolution).
- At 200 MHz clock, 20 kHz PWM → RA6T2/RA8T2: GTPR = 10000 (10000 steps, ~0.01% resolution), and can be increased significantly with lower prescaler.

**Constraint:** When porting from RA2T1, GTPR, GTCCRA, GTDVU values need to be scaled for 32-bit range.

---

## 4. Clock Prescaler and External Trigger

| Clock Source | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| GTCLK/1 | ✓ | ✓ | ✓ |
| GTCLK/2 | ✓ | ✓ | ✓ |
| GTCLK/4 | ✓ | ✓ | ✓ |
| GTCLK/8 | ✓ | ✓ | ✓ |
| GTCLK/16 | ✓ | ✓ | ✓ |
| GTCLK/32 | ✓ | ✓ | ✓ |
| GTCLK/64 | ✓ | ✓ | ✓ |
| **GTCLK/128** | **✗** | ✓ | ✓ |
| GTCLK/256 | ✓ | ✓ | ✓ |
| **GTCLK/512** | **✗** | ✓ | ✓ |
| GTCLK/1024 | ✓ | ✓ | ✓ |
| GTETRGA | ✓ | ✓ | ✓ |
| GTETRGB | ✓ | ✓ | ✓ |
| **GTETRGC** | **✗** | ✓ | ✓ |
| **GTETRGD** | **✗** | ✓ | ✓ |

**Constraint:** RA2T1 lacks GTCLK/128 and GTCLK/512. If RA6T2/RA8T2 firmware uses these prescalers → need to select different prescaler when porting to RA2T1.

---

## 5. Dead Time Configuration

All 3 devices have the same basic dead time structure:

| Register | Function | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| **GTDVU** | Dead time value (up-counting direction) | ✓ | ✓ | ✓ |
| **GTDVD** | Dead time value (down-counting direction) | ✓ | ✓ | ✓ |
| **GTDBU** | Dead time buffer (up) | ✓ | ✓ | ✓ |
| **GTDBD** | Dead time buffer (down) | ✓ | ✓ | ✓ |
| **GTDTCR** | Dead time control register | ✓ | ✓ | ✓ |

### Automatic Dead Time Setting Function

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Asymmetric auto dead time** | CH0–CH2 only | All complementary channels (CH4–CH9) | All complementary channels (CH4–CH9) |
| **Symmetric auto dead time** | CH3 only | Via GTDTCR.TDFER bit (all channels) | Via GTDTCR.TDFER bit (all channels) |
| **TDFER bit** | Yes | Yes | Yes |
| **Prohibited: write GTCCRB** | When TDE=1 | When TDE=1 | When TDE=1 |

**TDFER bit (Same Dead Time for First/Second Half):** When TDFER=1, GTDVD value is ignored and GTDVU is used for both up-counting and down-counting, creating symmetric dead time. Available on all 3 devices.

---

## 6. Dead Time Error Handling (Detailed)

### 6.1 Detection Mechanism

Dead time error occurs when the waveform change point (after adding dead time) exceeds the count period. Mechanism is **identical across all 3 devices**:

**Setting conditions for DTEF (Dead Time Error Flag):**

| Mode | Condition | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| Triangle-wave, Up-counting | GTCCRA − GTDVU ≤ 0 | ✓ | ✓ | ✓ |
| Triangle-wave, Down-counting | GTCCRA − GTDVD < 0 | ✓ | ✓ | ✓ |
| Saw-wave one-shot, Up-counting | GTCCRA − GTDVU < 0, OR GTCCRA + GTDVD > GTPR | ✓ | ✓ | ✓ |
| Saw-wave one-shot, Down-counting | GTCCRA + GTDVU > GTPR, OR GTCCRA − GTDVD < 0 | ✓ | ✓ | ✓ |

**Clearing condition:** When change point returns within count period.

### 6.2 Error Response — POEG Integration

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **GTINTAD.GRPDTE bit** | Yes | Yes | Yes |
| **DTEF → POEG output disable** | Yes (when GRPDTE=1) | Yes (when GRPDTE=1) | Yes (when GRPDTE=1) |
| **GPT dead time error interrupt** | **No** (use POEG interrupt instead) | **No** (use POEG interrupt instead) | **No** (use POEG interrupt instead) |
| **DTEF flag type** | Read-only (auto clear) | Read-only (auto clear) | Read-only (auto clear) |

### 6.3 Short-Circuit Protection Flags

| Flag | Description | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| **OABHF** | GTIOCnA and GTIOCnB both output HIGH | ✓ | ✓ | ✓ |
| **OABLF** | GTIOCnA and GTIOCnB both output LOW | ✓ | ✓ | ✓ |
| **GRPABH bit** | Enable OABHF → POEG disable request | ✓ | ✓ | ✓ |
| **GRPABL bit** | Enable OABLF → POEG disable request | ✓ | ✓ | ✓ |

### 6.4 Waveform Adjustment When Dead-Time Error Occurs

When dead-time error occurs, all 3 devices **automatically adjust the change point** to ensure dead time. The adjusted value is automatically set in the GTCCRB register. Mechanism is identical across all 3 devices, but note:

- In **saw-wave one-shot pulse mode**, if the change point order is reversed after adjustment, complementary relation **is not guaranteed**.
- In **triangle-wave PWM mode**, if GTCCRA = 0x00000000 or ≥ GTPR, output is controlled by the output protection function.
- When GTCCRA ≥ [GTPR + GTDV(U/D)], GTCCRB is set to upper limit = [GTPR − 1].

---

## 7. POEG (Port Output Enable for GPT)

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **POEG groups** | 2 (Group A, B) | 4 (Group A–D) | 4 (Group A–D) |
| **Disable sources** | Software, GTETRGA pin, GPT output-level, Dead time error, Output short-circuit | Same | Same |
| **POEG reset** | Software write to POEGG[n].POEGGn=0 | Same | Same |
| **Group-channel binding** | Fixed per channel | Fixed per channel | Fixed per channel |

**Example mapping:**
- **RA2T1:** CH0–CH2 → Group A; CH3 → Group B
- **RA6T2/RA8T2:** CH0–CH1 → Group A; CH2–CH3 → Group B; CH4–CH5 → Group C; CH6–CH9 → Group D

---

## 8. Software Trigger — Group Synchronization

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **SSCGRP bit** | Yes (GTCR bit 29) | Yes (GTCR bit 29) | Yes (GTCR bit 29) |
| **Software start sync** | SSCGRP=1 → all channels in same group start together | Same | Same |
| **Software stop sync** | SSCGRP=1 → all channels in same group stop together | Same | Same |
| **Software clear sync** | SSCGRP=1 → all channels in same group clear together | Same | Same |

**Critical:** In complementary PWM mode, **slave channels ignore their own GTSTR/GTSTP/GTCLR** — only master channel's register is effective. This is identical across all 3 devices.

---

## 9. Buffer Transfer Mechanism

### 9.1 GTCCRA Buffer Chain

All 3 devices use the same buffer transfer chain:

```
GTCCRF → GTCCRE → GTCCRD → GTCCRC → GTCCRA
```

| Buffer | Transfer Condition | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| GTCCRF → GTCCRE | Write to GTCCRF | ✓ | ✓ | ✓ |
| GTCCRE → GTCCRD | Write to GTCCRE | ✓ | ✓ | ✓ |
| **GTCCRD → GTCCRC** | **Write to slave2.GTCCRD** (trigger) | ✓ | ✓ | ✓ |
| GTCCRC → GTCCRA | Trough or Crest (mode-dependent) | ✓ | ✓ | ✓ |

### 9.2 Slave-Channel-2-Last Write Ordering Rule

**Critical rule for synchronized duty cycle update:**

To ensure all 3 phases update simultaneously, the write sequence **must be**:
1. Write master.GTCCRD
2. Write slave1.GTCCRD
3. **Write slave2.GTCCRD LAST** ← This triggers GTCCRD → GTCCRC transfer for all 3 channels

This ordering rule is **identical** across all 3 devices.

### 9.3 GTCCRA Buffer Transfer Timing

| Mode | Transfer Condition | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| **Complementary PWM Mode 1** | Trough | ✓ | ✓ | ✓ |
| **Complementary PWM Mode 2** | Crest | ✓ | ✓ | ✓ |
| **Complementary PWM Mode 3** | Trough | ✓ | ✓ | ✓ |
| **Complementary PWM Mode 4** | Crest | ✓ | ✓ | ✓ |
| **Prohibited values** | ≤ dead time value, ≥ count cycle | ✓ | ✓ | ✓ |

**Mechanism is identical across all 3 devices.**

### 9.4 GTPR Buffer Transfer Timing

| Buffer | Saw-wave | Triangle-wave (Mode 1/3) | Triangle-wave (Mode 2/4) |
|---|---|---|---|
| GTPDBR → GTPBR | Overflow/underflow/clear | Trough | Crest + Trough |
| GTPBR → GTPR | Overflow/underflow/clear | Trough | Crest + Trough |

**Note in complementary PWM mode:** Buffer operation for GTDVU register is **prohibited** — must set directly.

---

## 10. CPSCD Bit — Complementary PWM Mode Synchronous Clear Disable

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **CPSCD bit** | **Not available** | **Not available** | **YES** (RA8T2 only) |
| **Bit position** | N/A | N/A | Bit 12 in GTCR |
| **Available channels** | N/A | N/A | GPT324–GPT329 |

**CPSCD bit** on RA8T2:
- `CPSCD = 0`: Enable synchronous counter clear from other channels (except trough section) in complementary PWM mode.
- `CPSCD = 1`: Disable synchronous counter clear from other channels (except trough section).
- Slave channels are also controlled by the CPSCD bit of the master channel.

This feature makes RA8T2 more flexible when multiple channel groups need to operate **independently** — avoids unwanted synchronous clear between 2 motor control groups.

---

## 11. CPSCIR Bit — Initial Output at Synchronous Clear Disable

| Property | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **CPSCIR bit** | Yes | Yes | Yes |
| **Related section** | Section 20.2.14 | Section 21.10.10 | Section 22.10.8 |
| **Compare match range restriction** | Yes | Yes (section 21.10.10) | Yes (section 22.10.8) |

When `CPSCIR = 1` and synchronous clear occurs, the initial output of pins is **not** reset. All 3 devices support this, but RA6T2 and RA8T2 have additional sections describing restrictions for compare match register setting range when CPSCIR=1.

---

## 12. Number of Complementary PWM Channel Groups

| Device | Max Groups | Available Channels | Motor Control Capability |
|---|---|---|---|
| **RA2T1** | **1 group** | CH0–CH2 | 1 motor (3-phase) |
| **RA6T2** | **2 groups** | CH4–CH6, CH7–CH9 | 2 motors (3-phase each) |
| **RA8T2** | **2 groups** | CH4–CH6, CH7–CH9 | 2 motors (3-phase each) |

---

## 13. Register Address Map (Detailed)

### 13.1 Base Address

| Device | Formula | Example Master Channel |
|---|---|---|
| **RA2T1** | GPT16n = `0x4008_9000 + 0x0100 × n` (n=0–2); GPT163 = `0x4008_9300` | GPT160 (master) = `0x4008_9000` |
| **RA6T2** | GPT32n = `0x4016_9000 + 0x0100 × n` (n=0–9) | GPT324 (master) = `0x4016_9400` |
| **RA8T2** | GPT32n = `0x4032_2000 + 0x0100 × n` (n=0–13) | GPT324 (master) = `0x4032_2400` |

### 13.2 Register Offset Map (Complementary PWM Related)

Register offsets are **identical** across all 3 devices. Only base address and register width differ.

| Offset | Register | Function in Complementary PWM | Data Width |
|---|---|---|---|
| 0x00 | **GTWP** | Write Protection | 32-bit (all) |
| 0x04 | **GTSTR** | Software Start (master only valid) | 32-bit (all) |
| 0x08 | **GTSTP** | Software Stop (master only valid) | 32-bit (all) |
| 0x0C | **GTCLR** | Software Clear (master only valid) | 32-bit (all) |
| 0x30 | **GTCR** | Control: MD[3:0]=0xC/D/E/F, TPCS, SSCGRP, CPSCD* | 32-bit (all) |
| 0x34 | **GTUDDTYC** | Count Direction and Duty Setting | 32-bit (all) |
| 0x38 | **GTIOR** | I/O Control: pin output settings, CPSCIR | 32-bit (all) |
| 0x3C | **GTINTAD** | Interrupt/AD: GRPDTE, GRPABH, GRPABL | 32-bit (all) |
| 0x40 | **GTST** | Status: DTEF, OABHF, OABLF flags | 32-bit (all) |
| 0x44 | **GTBER** | Buffer Enable: CCRA, PR buffer control | 32-bit (all) |
| 0x48 | **GTITC** | Interrupt Skipping | 32-bit (all) |
| 0x4C | **GTCNT** | Counter value | **16-bit (RA2T1)** / 32-bit |
| 0x4C+offset | **GTCCRA** | Compare match A (duty cycle) | **16-bit (RA2T1)** / 32-bit |
| | **GTCCRB** | Compare match B (auto dead time target) | **16-bit (RA2T1)** / 32-bit |
| | **GTCCRC** | Buffer for GTCCRA (single buffer) | **16-bit (RA2T1)** / 32-bit |
| | **GTCCRD** | Buffer for GTCCRC (write trigger) | **16-bit (RA2T1)** / 32-bit |
| | **GTCCRE** | Buffer for GTCCRA (double buffer) | **16-bit (RA2T1)** / 32-bit |
| | **GTCCRF** | Buffer for GTCCRE (write source) | **16-bit (RA2T1)** / 32-bit |
| 0x64 | **GTPR** | Cycle Setting (period) | **16-bit (RA2T1)** / 32-bit |
| 0x68 | **GTPBR** | Cycle Setting Buffer | **16-bit (RA2T1)** / 32-bit |
| 0x6C | **GTPDBR** | Cycle Setting Double Buffer | **16-bit (RA2T1)** / 32-bit |
| 0x8C | **GTDVU** | Dead Time Value Up-counting | **16-bit (RA2T1)** / 32-bit |
| 0x8C+4 | **GTDVD** | Dead Time Value Down-counting | **16-bit (RA2T1)** / 32-bit |
| 0x94 | **GTDBU** | Dead Time Buffer Up | **16-bit (RA2T1)** / 32-bit |
| 0x94+4 | **GTDBD** | Dead Time Buffer Down | **16-bit (RA2T1)** / 32-bit |
| 0x88 | **GTDTCR** | Dead Time Control: TDE, TDFER | 32-bit (all) |

*\*CPSCD bit at GTCR offset 0x30 only available on RA8T2*

### 13.3 Additional Registers (RA6T2/RA8T2 only)

| Register | Offset | Function | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|---|
| **GTCLKCR** | varies | Clock Control: BPEN (sync/async select) | **✗** | ✓ | ✓ |
| **GTICLF** | varies | Inter Channel Logical Operation Function | **✗** | ✓ | ✓ |

### 13.4 Complementary PWM Channel Address Examples

| Register | RA2T1 (Master=GPT160) | RA6T2 (Master=GPT324) | RA8T2 (Master=GPT324) |
|---|---|---|---|
| Master.GTCR | `0x4008_9030` | `0x4016_9430` | `0x4032_2430` |
| Master.GTPR | `0x4008_9064` | `0x4016_9464` | `0x4032_2464` |
| Master.GTDVU | `0x4008_908C` | `0x4016_948C` | `0x4032_248C` |
| Slave1.GTCCRD | `0x4008_915C` | `0x4016_955C` | `0x4032_255C` |
| Slave2.GTCCRD | `0x4008_925C` | `0x4016_965C` | `0x4032_265C` |

**Slave2.GTCCRD** is the register that must be written **LAST** to trigger simultaneous buffer transfer for all 3 channels.

---

## 14. GPT Register Count Comparison

| Metric | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Total GPT registers** | 82 | 86 | 86 |
| **GTBER2** | ✓ | ✓ | ✓ |
| **GTCLKCR** | ✗ | ✓ | ✓ |
| **GTICLF** | ✗ | ✓ | ✓ |
| **CPSCD bit in GTCR** | ✗ | ✗ | ✓ |
| **Register offsets** | Same | Same | Same |

---

## 15. Summary of Porting Constraints

### Porting from RA2T1 → RA6T2/RA8T2

| Category | Required Changes |
|---|---|
| **Channel mapping** | CH0/1/2 → CH4/5/6 (or CH7/8/9) |
| **Register prefix** | `GPT16n` → `GPT32n` |
| **Register width** | 16-bit → 32-bit (all GPT data registers) |
| **Base address** | `0x4008_9000` → `0x4016_9000` (RA6T2) / `0x4032_2000` (RA8T2) |
| **GTPR value** | Rescale according to new clock (can be much larger) |
| **GTDVU value** | Rescale according to new clock |
| **GTCCRA values** | Rescale (larger duty cycle range) |
| **Clock source** | PCLKD only → can select GPTCLK (async), need to config GTCLKCR |
| **Prescaler** | Additional options GTCLK/128, GTCLK/512 |
| **POEG** | 2 groups → 4 groups |
| **Module stop bit** | Different MSTPCRx bit numbers |
| **Interrupt vector** | IELSRn.IELS[8:0] (RA2T1) vs IELS[8:0] (RA6T2) vs IELS[9:0] (RA8T2) |

### Porting from RA6T2 → RA8T2

| Category | Required Changes |
|---|---|
| **Channel mapping** | Same (CH4–9) |
| **Register structure** | Nearly identical |
| **CPSCD bit** | RA8T2 only — adds synchronous clear disable option |
| **Extra channels** | RA8T2 has CH10–CH13 (but no complementary PWM support) |
| **Base address** | `0x4016_9000` → `0x4032_2000` |
| **Interrupt vector** | IELS[8:0] → IELS[9:0] (wider) |
| **GTPBR/GTPDBR range note** | RA8T2 does not document separately (should still follow) |
| **Counter clear in comp. PWM** | RA8T2 does not document separately |

### Porting from RA6T2 ↔ RA8T2 (Minimal Changes)

These two devices are **nearly identical** in GPT complementary PWM behavior. Main changes: base address, CPSCD bit (RA8T2 only), and IELS width.

---

*Document generated from analysis of:*
- *RA2T1 User's Manual: r01uh1089ej0100 Rev.1.00 (Jun 25, 2025)*
- *RA6T2 User's Manual: r01uh0951ej0150 Rev.1.50 (Jul 2, 2025)*
- *RA8T2 User's Manual: r01uh1067ej0130 Rev.1.30 (Feb 27, 2026)*
