# Product Requirement Document

## GPT Complementary PWM Modes 1, 2, 3, 4

**FPB-RA2T1 Board | Renesas RA2T1 MCU | FSP + e2studio**

| Field | Value |
|---|---|
| **Document ID:** | PRD-COMP-PWM-001 |
| **Version:** | 1.0 |
| **Date:** | 2026-03-31 |
| **Author:** | transinh0707-debug |
| **Status:** | Released |
| **Reference:** | RA2T1 User Manual Sec 20.3.3.7 / 20.3.3.8 |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Product Overview](#2-product-overview)
3. [Functional Requirements](#3-functional-requirements)
4. [Non-Functional Requirements](#4-non-functional-requirements)
5. [Design Constraints](#5-design-constraints)
6. [API Specification](#6-api-specification)
7. [Acceptance Criteria](#7-acceptance-criteria)
8. [Deliverables](#8-deliverables)
9. [Revision History](#9-revision-history)

---

## 1. Introduction

### 1.1 Purpose

This Product Requirement Document (PRD) defines the functional and non-functional requirements for implementing GPT Complementary PWM Modes 1, 2, 3, and 4 on the Renesas RA2T1 microcontroller (FPB-RA2T1 evaluation board). The implementation provides three-phase PWM waveform generation with hardware dead time insertion, targeting motor control and power conversion applications.

### 1.2 Scope

This document covers:

- Four complementary PWM operating modes (crest, trough, crest+trough, and immediate buffer transfer)
- Three-phase output using three consecutive GPT16 timer channels (ch0, ch1, ch2)
- Configurable dead time for shoot-through protection
- Interactive runtime configuration via SEGGER J-Link RTT Viewer
- Integration with the existing GPT timer example project (Periodic, PWM, One-Shot modes)

### 1.3 Definitions and Acronyms

| Term | Definition |
|---|---|
| **GPT** | General PWM Timer module in Renesas RA MCU family |
| **FSP** | Flexible Software Package — Renesas HAL/driver framework |
| **GTCR.MD** | GPT Control Register Mode Select bits [3:0] |
| **GTCCRA** | GPT Compare Capture Register A — active compare match register |
| **GTCCRD/F** | GPT Compare Capture Registers D and F — buffer source registers |
| **GTDVU** | GPT Dead Time Value Up register — sets dead time in timer counts |
| **GTPR** | GPT Period Register — defines PWM cycle (triangle wave crest) |
| **Dead Time** | Non-overlapping guard interval between complementary output transitions |
| **RTT** | Real-Time Transfer — SEGGER debug communication channel |
| **Positive-phase** | High-side PWM output (GTIOCnA pin) |
| **Negative-phase** | Low-side PWM output (GTIOCnB pin) |

### 1.4 References

1. RA2T1 Group User's Manual: Hardware (R01UH1089EJ0100), Renesas Electronics, Rev.1.00, Jun 2025
2. Section 20.3.3.7 — Complementary PWM Modes 1, 2, 3
3. Section 20.3.3.8 — Complementary PWM Mode 4
4. Renesas FSP (Flexible Software Package) Documentation
5. SW Detailed Design: Complementary PWM (.pptx / .vsdx) in project repository

---

## 2. Product Overview

### 2.1 Product Description

The Complementary PWM feature extends the existing GPT Timer example project to support three-phase PWM waveform generation with automatic dead time insertion. It uses three consecutive GPT16 channels operating as a coordinated group: a master channel drives the triangle wave timing, and two slave channels provide offset counting and linearity correction at extreme duty cycles (0% and 100%).

This feature is designed for applications requiring precise three-phase motor control, such as BLDC motor drives, inverter power stages, and servo control systems.

### 2.2 Target Hardware

| Parameter | Value |
|---|---|
| **Board** | FPB-RA2T1 (Renesas RA2T1 Fast Prototyping Board) |
| **MCU** | R7FA2T1A93CFM (Arm Cortex-M23, 64 MHz, 64KB Flash, 8KB SRAM) |
| **Timer** | GPT16 (16-bit General PWM Timer), channels 0–2 |
| **Timer Resolution** | 16-bit (0x0000 to 0xFFFF max period count) |
| **Output Pins** | 6 pins: GTIOCnA/B for each of 3 channels (positive + negative phase) |
| **Debug Interface** | SEGGER J-Link with RTT Viewer for user interaction |
| **IDE** | e2studio with Renesas FSP (Flexible Software Package) |

### 2.3 Channel Architecture

The complementary PWM feature requires exactly three consecutive GPT16 channels configured as a channel group:

| Channel | Role | Function |
|---|---|---|
| **GPT16n (ch0)** | Master | Triangle wave counter (0 to GTPR). Drives PWM cycle. Holds dead time (GTDVU) and period (GTPR). Controls start/stop for all channels. |
| **GPT16n+1 (ch1)** | Slave 1 | Offset counter (master GTCNT + GTDVU). Used for negative-phase compare match operations in middle sections. |
| **GPT16n+2 (ch2)** | Slave 2 | Linearity counter. Active in crest and trough sections only. Ensures accurate duty cycle at 0% and 100% boundaries. GTCCRD write triggers temp register transfer. |

---

## 3. Functional Requirements

### 3.1 Operating Modes

The system shall support four complementary PWM operating modes, each differing in how buffer register values are transferred to the active compare match register (GTCCRA):

| Mode | GTCR.MD | Transfer Timing | Description |
|---|---|---|---|
| **Mode 1** | 0xC | At crest boundary | GTCCRC transfers to GTCCRA at the end of crest section. Best for applications where duty updates should take effect at the top of the triangle wave. |
| **Mode 2** | 0xD | At trough boundary | GTCCRC transfers to GTCCRA at the end of trough section. Suitable when updates should take effect at the bottom of the triangle wave. |
| **Mode 3** | 0xE | At crest and trough | GTCCRC transfers at both boundaries. Supports single buffer (GTCCRD only) and double buffer (GTCCRD + GTCCRF via GTBER2.CP3DB). Enables asymmetric PWM per half-cycle. |
| **Mode 4** | 0xF | Immediate | Values written to GTCCRD/GTCCRF bypass the normal buffer chain and transfer directly to GTCCRA. Provides lowest latency duty cycle updates. Immediate transfer guaranteed only in middle sections. |

### 3.2 Initialization Sequence

The system shall implement the following 12-step initialization sequence as defined in RA2T1 Manual Table 20.35 (Modes 1–3) and Table 20.40 (Mode 4):

1. **Set operating mode:** Write GTCR.MD[3:0] on master channel (0xC/0xD/0xE/0xF)
2. **Select count clock:** Configure GTCR.TPCS[3:0] prescaler on master channel
3. **Set cycle:** Write period to GTPR, GTPBR, GTPDBR on master channel
4. **Set pin function:** Configure GTIOR (GTIOA/GTIOB) on all three channels
5. **Enable GTCPPOn output:** Set GTIOR.PSYE on master channel
6. **Enable pin output:** Set GTIOR.OAE and GTIOR.OBE on all three channels
7. **Set buffer operation:** Configure GTBER2.CP3DB (Mode 3 double buffer only)
8. **Set compare match value:** Write initial duty to GTCCRA on all three channels
9. **Set buffer value:** Write to GTCCRD (single) or GTCCRD+GTCCRF (double) on all channels
10. **Set dead time:** Write GTDVU on master channel (no buffer operation permitted)
11. **Start count:** Set GTCR.CST = 1 on master channel (slaves auto-follow)
12. **Runtime updates:** Write GTCCRD to ch0, ch1, then ch2 (ch2 must be written last to trigger transfer)

### 3.3 Dead Time Management

- **REQ-DT-01:** The system shall allow configuration of dead time in timer count units via the GTDVU register of the master channel.
- **REQ-DT-02:** Dead time value shall satisfy: 0 < GTDVU < GTPR (dead time must be positive and less than the period).
- **REQ-DT-03:** Buffer operation shall NOT be used for the GTDVU register in any complementary PWM mode.
- **REQ-DT-04:** Dead time shall create a non-overlapping guard between positive-phase OFF and negative-phase ON transitions to prevent shoot-through in power stage drivers.
- **REQ-DT-05:** The default dead time shall be 0x0100 timer counts, adjustable at runtime via RTT Viewer.

### 3.4 Duty Cycle Control

- **REQ-DC-01:** The system shall support independent duty cycle control for three phases (U, V, W) in the range 0% to 100%.
- **REQ-DC-02:** Duty cycle shall be converted to compare match counts using: `counts = GTPR - (duty_percent × GTPR / 100)`.
- **REQ-DC-03:** When GTCCRA ≥ GTPR, duty shall be 0% (positive-phase OFF, negative-phase ON).
- **REQ-DC-04:** When GTCCRA = 0, duty shall be 100% (positive-phase ON, negative-phase OFF).
- **REQ-DC-05:** The conversion shall use uint32_t intermediate arithmetic to prevent 16-bit overflow.

### 3.5 Buffer Transfer Chain

- **REQ-BUF-01:** For Modes 1–3, the normal buffer chain shall be: GTCCRD → Temp Register A → GTCCRC → GTCCRA.
- **REQ-BUF-02:** For Mode 3 double buffer, an additional path shall be: GTCCRF → Temp Register B → GTCCRE → GTCCRA.
- **REQ-BUF-03:** For Mode 4, an immediate bypass path shall transfer GTCCRD directly to GTCCRA (and GTCCRF to GTCCRA for double buffer).
- **REQ-BUF-04:** Writing to slave channel 2 (GPT16n+2) GTCCRD shall always be performed LAST, as it triggers simultaneous temporary register transfer across all three channels.

### 3.6 Counter Operation Sections

**REQ-SEC-01:** The system shall recognize five operation sections based on master counter position:

| Section | Counter Range | Behavior |
|---|---|---|
| **Initial Output** | Master: 0 to GTDVU (after start) | Retains GTIOR initial output state |
| **Up-Counting Middle** | Master: GTDVU+1 to GTPR-GTDVU | Normal compare match with GTCCRA |
| **Crest** | Master: GTPR-GTDVU+1 to GTPR and back | Slave 2 active for 100% linearity |
| **Down-Counting Middle** | Master: GTPR-GTDVU-1 to GTDVU | Mirror of up-counting middle |
| **Trough** | Master: GTDVU-1 to 0 and back to GTDVU | Slave 2 active for 0% linearity |

### 3.7 User Interface (RTT Viewer)

- **REQ-UI-01:** The main menu shall present options 1–7: Periodic (1), PWM (2), One-Shot (3), Comp PWM Mode 1 (4), Mode 2 (5), Mode 3 (6), Mode 4 (7).
- **REQ-UI-02:** Selecting a complementary PWM mode shall display a submenu with: (1) Set Dead Time, (2) Set 3-Phase Duty, (3) Start, (4) Stop, (5) Show Status, (0) Return to Main Menu.
- **REQ-UI-03:** The system shall validate all user inputs and display error messages for out-of-range values.
- **REQ-UI-04:** The status display shall show: current mode, running state, period, dead time, U/V/W duty percentages, and double buffer state.

---

## 4. Non-Functional Requirements

### 4.1 Performance

- **REQ-PERF-01:** Timer period range for FPB-RA2T1 shall be 0 to 1048 ms (per readme.txt specification).
- **REQ-PERF-02:** Dead time resolution shall be 1 timer count (minimum granularity of the 16-bit GPT16 timer).
- **REQ-PERF-03:** Mode 4 immediate transfer shall take effect within the current counter section (no full-cycle delay).

### 4.2 Reliability

- **REQ-REL-01:** The system shall prevent opening a new timer mode without first closing any previously open mode.
- **REQ-REL-02:** All FSP API calls shall be checked for error return codes, with cleanup (R_GPT_Close) on failure.
- **REQ-REL-03:** Zero-value inputs for period and dead time shall be rejected with an error message.
- **REQ-REL-04:** The one-shot timeout counter shall be reset before each one-shot operation to prevent stale timeout (bug fix applied).

### 4.3 Maintainability

- **REQ-MAIN-01:** Complementary PWM code shall be in separate source files (complementary_pwm.c / .h) from existing GPT timer code.
- **REQ-MAIN-02:** All functions shall include Doxygen-compatible documentation with parameter descriptions and return values.
- **REQ-MAIN-03:** Code shall reference specific RA2T1 manual sections and table numbers in comments.

### 4.4 Portability

- **REQ-PORT-01:** The implementation shall use FSP HAL APIs (R_GPT_Open, R_GPT_Start, R_GPT_DutyCycleSet, etc.) rather than direct register access where possible.
- **REQ-PORT-02:** Board-specific pin definitions shall be isolated using preprocessor conditionals (BOARD_RA2T1_FPB).
- **REQ-PORT-03:** Timer count width (16-bit vs 32-bit) shall be handled via GPT_MAX_PERIOD_COUNT macro.

---

## 5. Design Constraints

### 5.1 Hardware Constraints

- Complementary PWM mode is only available on GPT16 channels 0–2 (not GPT163)
- FPB-RA2T1 board requires: Close E8, Close E15, Cut E10 to use P213 for LED1
- 16-bit timer limits maximum period count to 0xFFFF
- Dead time value must satisfy: GTCCRA - GTDVU > 0

### 5.2 Software Constraints

- Master channel GTCR.MD must be set while GTCNT is stopped
- Only master channel start/stop/clear bits control all 3 channels
- GTDVU register must not use buffer operation in complementary PWM mode
- Slave channel 2 GTCCRD must be written last to trigger temporary register transfer
- Mode 4 double buffer immediate transfer is guaranteed only in up-counting and down-counting middle sections
- Input data from RTT Viewer must not exceed 15 bytes

---

## 6. API Specification

The following public API functions are defined in `complementary_pwm.h`:

| Function | Parameters | Description |
|---|---|---|
| **comp_pwm_init()** | `uint8_t comp_mode` | Initialize 3 GPT channels for specified complementary PWM mode (4–7) |
| **comp_pwm_start()** | `void` | Start PWM output on master channel (slaves follow) |
| **comp_pwm_stop()** | `void` | Stop PWM and close all 3 channels |
| **comp_pwm_set_dead_time()** | `uint16_t counts` | Set dead time value in GTDVU register |
| **comp_pwm_set_duty_3phase()** | `uint8_t u, v, w` | Set 3-phase duty cycles (0–100%) with ch2-last ordering |
| **comp_pwm_get_config()** | `void` | Return pointer to current configuration structure |
| **comp_pwm_process_input()** | `void` | Interactive RTT submenu for runtime configuration |

---

## 7. Acceptance Criteria

| ID | Criterion | Verification |
|---|---|---|
| **AC-01** | All 4 complementary PWM modes can be selected and initialized without error | RTT Viewer test |
| **AC-02** | Three-phase PWM output observed on GTIOCnA/B pins with oscilloscope | Hardware measurement |
| **AC-03** | Dead time gap visible between positive-phase OFF and negative-phase ON transitions | Oscilloscope verification |
| **AC-04** | Duty cycle of 0%, 50%, and 100% produce correct waveforms with linear transitions | Oscilloscope + RTT |
| **AC-05** | Changing dead time at runtime updates output without glitch or restart | RTT + oscilloscope |
| **AC-06** | Mode switching (e.g., Mode 1 to Mode 3) correctly closes and reopens channels | RTT Viewer test |
| **AC-07** | Invalid inputs (out-of-range duty, zero dead time, etc.) produce error messages without crash | RTT Viewer test |
| **AC-08** | Existing Periodic, PWM, and One-Shot modes continue to function correctly | Regression test |
| **AC-09** | Mode 4 immediate duty update takes effect within the current PWM cycle | Oscilloscope timing |
| **AC-10** | Project compiles without warnings on e2studio with FSP toolchain | Build verification |

---

## 8. Deliverables

| Deliverable | Location | Status |
|---|---|---|
| Source code: complementary_pwm.c | e2studio/src/ | Delivered |
| Header file: complementary_pwm.h | e2studio/src/ | Delivered |
| Updated: hal_entry.c (bug fixes) | e2studio/src/ | Delivered |
| Updated: gpt_timer.c/h (menu + fixes) | e2studio/src/ | Delivered |
| SW Detailed Design (.vsdx) | e2studio/doc/ | Delivered |
| SW Detailed Design (.pptx) | e2studio/doc/ | Delivered |
| Product Requirement Document (.docx) | e2studio/doc/ | Delivered |
| Product Requirement Document (.md) | e2studio/doc/ | Delivered |

---

## 9. Revision History

| Version | Date | Author | Changes |
|---|---|---|---|
| 1.0 | 2026-03-31 | transinh0707-debug | Initial release: Complementary PWM Modes 1–4 implementation, SW detailed design, and PRD |
| 1.1 | 2026-04-02 | transinh0707-debug | Added Markdown (.md) format of PRD |
