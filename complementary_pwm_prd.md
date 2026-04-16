---
title: GPT Complementary PWM Mode Support PRD
description: Support Complementary PWM mode for GPT feature
published: true
date: 2026-04-16
tags: 
editor: markdown
dateCreated: 
---

# Overview
Target release: v6.5.0 rc0
Epic: https://jira.eng.renesas.com/browse/FSPRA-5725

This document describes the proposal for supporting Complementary PWM as an extended feature of the GPT peripheral module on RA MCUs.

# Goals
Add GPT Complementary PWM support for RA MCUs that expose the Complementary PWM capability of the GPT peripheral.

# Strategic fit
The GPT Complementary PWM implementation provides three-phase PWM waveform generation with hardware-inserted dead time, targeting motor control and power conversion applications. GPT Complementary PWM support is requested by Renesas business development and management.
  
# Background
The GPT peripheral on RA MCUs provides exactly four Complementary PWM modes. In Complementary PWM mode, three consecutive GPT channels (one master + two slaves) are used together to generate a three-phase PWM waveform with dead time, ensuring duty-cycle linearity in the vicinity of 0% and 100%. The four modes differ only in the timing of the buffer transfer:

- Complementary PWM mode 1 (transfer at crests).
- Complementary PWM mode 2 (transfer at troughs).
- Complementary PWM mode 3 (transfer at crests and troughs).
- Complementary PWM mode 4 (immediate transfer).

![alt text](image-1.png)

<u>**List of supported MCUs:**</u>
|No|MCU|Core|Test|Note|
|:-:|:-:|:----:|:-:|:----:|
|1|RA2T1|CM23|Yes|-|
|2|RA6T2|CM33|Yes|-|
|3|RA8T2|CM85 & CM33|No|Current released pack has not supported yet|

# Requirements 
|Requirement ID|Title|Use case|Importance|Notes|
|----|----|--------------------|---|----|
|REQ-OM-01|Operating mode 1|GTCCRC transfers to GTCCRA at the end of the crest section|Must have|Transfer timing at crest boundary|
|REQ-OM-02|Operating mode 2|GTCCRC transfers to GTCCRA at the end of the trough section|Must have|Transfer timing at trough boundary|
|REQ-OM-03|Operating mode 3|GTCCRC transfers at both boundaries. Supports single buffer (GTCCRD only) and double buffer (GTCCRD + GTCCRF via GTBER2.CP3DB)|Must have|Transfer timing at both crest and trough boundaries|
|REQ-OM-04|Operating mode 4|Values written to GTCCRD/GTCCRF bypass the normal buffer chain and transfer directly to GTCCRA|Must have|Immediate transfer; required for motor control|
|REQ-DT-05|Configurable dead time|The system shall allow configuration of dead time in timer count units via the GTDVU register of the master channel|Must have|-|
|REQ-DT-06|Valid range of dead time value|Dead time value shall satisfy: 0 < GTDVU < GTPR|Must have|-|
|REQ-DT-07|No buffer operation for dead time register|Buffer operation shall NOT be used for the GTDVU register in any Complementary PWM mode|Must have|-|
|REQ-DT-08|Non-overlapping guard provided by dead time|Dead time shall create a non-overlapping guard between positive-phase OFF and negative-phase ON transitions to prevent shoot-through in power-stage drivers|Must have|Prevent shoot-through|
|REQ-DC-09|Independent duty cycle control across three GPT channels|The system shall support independent duty cycle control for three phases (U, V, W) in the range 0% to 100%|Must have|-|
|REQ-DC-10|Compare match reaches GTPR|When GTCCRA ≥ GTPR, duty shall be 0% (positive-phase OFF, negative-phase ON)|Must have|-|
|REQ-DC-11|Compare match equals zero|When GTCCRA = 0, duty shall be 100% (positive-phase ON, negative-phase OFF)|Must have|-|
|REQ-DC-12|Prevent 16-bit overflow on compare match value|The duty-to-count conversion shall use uint32_t intermediate arithmetic to prevent 16-bit overflow|Must have|RA2T1 only|
|REQ-BUF-13|Single buffer chain for Modes 1–3|For Modes 1–3, the normal buffer chain shall be: GTCCRD → Temp Register A → GTCCRC → GTCCRA|Must have|-|
|REQ-BUF-14|Double buffer chain for Mode 3 only|For Mode 3 with double buffer enabled, an additional path shall be provided: GTCCRF → Temp Register B → GTCCRE → GTCCRA|Must have|-|
|REQ-BUF-15|Buffer chain for Mode 4|For Mode 4, an immediate bypass path shall transfer GTCCRD directly to GTCCRA (and GTCCRF directly to GTCCRA for double buffer)|Must have|-|
|REQ-BUF-16|Slave channel 2 write ordering|Writing to slave channel 2 (GPT16n+2) GTCCRD shall always be performed LAST, as it triggers simultaneous temporary-register transfer across all three channels|Must have|Ensures phase-aligned update|
|REQ-SEC-17|Support for five counting operation sections|The system shall recognize five operation sections based on the master counter position|Must have|-|

# User interaction and design
## 1. Design Idea
Complementary PWM support through GPT will be implemented as middleware, reusing the existing driver (`r_gpt.c`).

### 1.1 e2studio layer
![alt text](image-2.png)
- `g_timer_master`, `g_timer_slave1`, and `g_timer_slave2` are the GPT module dependencies required by the Complementary PWM middleware.
- `g_timer_master` is used to configure runtime parameters and to synchronize the two upper channels.
- `g_timer_slave1` is used to generate the PWM waveform with counter = master counter + GTDVU.
- `g_timer_slave2` is used to take the compare match at the crest section or the trough section.

In the e2studio layer, `g_timer_master`, `g_timer_slave1`, and `g_timer_slave2` share the following property configuration:
![alt text](image-3.png)

### 1.2 Middleware layer
New APIs will be created to handle GPT Complementary PWM Modes 1, 2, 3, and 4. Refer to section "3. APIs" for the full list of APIs.

The middleware is responsible only for the following tasks:
- Open/Close the middleware and the GPT module (the Open/Close APIs open/close all three GPT channels used for Complementary PWM).
- Start/Stop the middleware and the GPT module (the Start/Stop APIs act on the master channel; the slave channels follow automatically).
- Configure the dead time register (GTDVU) on the master channel.
- Set the three-phase duty cycles.


### 1.3 Driver layer
At the driver layer, `r_gpt.c` will be modified to support Complementary PWM Mode 3 with double buffer operation. Refer to section "3. APIs" for the full list of APIs.
![alt text](image-4.png)

#### 1.3.1 Single Buffer Transfer Mechanism for Complementary PWM (Modes 1, 2, 3)
All three devices (RA2T1, RA6T2, and RA8T2) use the same single buffer transfer chain: `GTCCRD → Temp_A → GTCCRC → GTCCRA`.

#### 1.3.2 Double Buffer Transfer Mechanism for Complementary PWM (Mode 3 only)

All three devices (RA2T1, RA6T2, and RA8T2) use the same double buffer transfer chain. When `GTBER2.CP3DB = 1`, a second transfer path must be added:
```
GTCCRF → Temp_B → GTCCRE → GTCCRA (at trough)
GTCCRD → Temp_A → GTCCRC → GTCCRA (at crest)
```
> **Note:** In Mode 3 with double buffer enabled, the GTCCRF → GTCCRD → GTCCRA transfer chain introduces a two-cycle delay before a newly written duty value takes effect on the output.

#### 1.3.3 Buffer Transfer Mechanism for Complementary PWM (Mode 4)
Mode 4 adds a second transfer path with immediate timing from GTCCRD/GTCCRF directly to GTCCRA.

## 2. Instance
Add a new instance for the new APIs `comp_pwm_set_duty_3phase` and `comp_pwm_configure_dead_time`:
```c
static comp_pwm_cfg_t g_comp_pwm_cfg =
{
    .mode          = RESET_VALUE,
    .dead_time     = COMP_PWM_DEFAULT_DEAD_TIME,
    .period        = COMP_PWM_DEFAULT_PERIOD,
    .duty_u        = 0U,
    .duty_v        = 0U,
    .duty_w        = 0U,
    .double_buffer = false,
    .is_running    = false,
};
```

Add four new GPT driver timer operational modes:
```c
/** Timer operational modes */
typedef enum e_timer_mode
{
    TIMER_MODE_PERIODIC,                          ///< Timer restarts after period elapses.
    TIMER_MODE_ONE_SHOT,                          ///< Timer stops after period elapses.
    TIMER_MODE_PWM,                               ///< Timer generates saw-wave PWM output.
    TIMER_MODE_ONE_SHOT_PULSE,                    ///< Saw-wave one-shot pulse mode (fixed buffer operation).
    TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM  = 4U, ///< Timer generates symmetric triangle-wave PWM output.
    TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM = 5U, ///< Timer generates asymmetric triangle-wave PWM output.

    /**
     * Timer generates Asymmetric Triangle-wave PWM output. In PWM mode 3, the duty cycle does
     * not need to be updated at each trough/crest interrupt. Instead, the trough and crest duty
     * cycle values can be set once and only need to be updated when the application needs to
     * change the duty cycle.
     */
    TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3 = 6U,

    TIMER_MODE_COMPLEMENTARY_PWM_MODE1 = 12U, ///< Transfer at crest
    TIMER_MODE_COMPLEMENTARY_PWM_MODE2 = 13U, ///< Transfer at trough
    TIMER_MODE_COMPLEMENTARY_PWM_MODE3 = 14U, ///< Transfer at crest and trough
    TIMER_MODE_COMPLEMENTARY_PWM_MODE4 = 15U  ///< Immediate transfer

} timer_mode_t;
```
## 3. APIs
This feature will extend the existing `r_gpt.c` driver.
### 3.1 APIs and internal functions supporting Complementary PWM
|No| Name | Requirement ID | Reason | Classification | Note |
| ----------|---------- | ----------- | ----- |----------- | ----- |
|1| comp_pwm_init | REQ-OM-01 … REQ-OM-04 | Initialize GPT channels for Complementary PWM operation | New | Opens all 3 channels, applies mode selection |
|2| comp_pwm_start | N/A | Start Complementary PWM output on all 3 channels | Modify | Starts master only; slaves follow |
|3| comp_pwm_set_dead_time | REQ-DT-05, REQ-DT-06 | Set dead time value for Complementary PWM | New | Writes GTDVU on master channel |
|4| comp_pwm_set_duty_3phase | REQ-DC-09 … REQ-DC-12 | Set three-phase duty cycles for U, V, W phases | New | Slave channel 2 written last (REQ-BUF-16) |
|5| comp_pwm_stop | N/A | Stop Complementary PWM output on all 3 channels | Modify | Stops master; closes all channels |

## 4. Module XMLs
This feature will be integrated into the existing GPT driver module using XML definitions consistent with current FSP GPT configurations.

The Complementary PWM functionality will be added as an extension of the existing GPT interface, without introducing a new standalone driver module.

No changes to the existing `<provides>` interface are required, as the feature will be included within the current GPT driver:
`<provides interface="interface.driver.gpt" />`

## 5. Constraint
### Setting Range of GTPBR and GTPDBR in Complementary PWM Mode
|Restriction|RA2T1|RA6T2|RA8T2|
|---|---|---|---|
|**GTPBR ≥ GTPR − GTDVU**|Modes 1, 3, 4 — at crest transfer|Modes 1, 3, 4 — at crest transfer|N/A|
|**GTPDBR ≥ GTPR − GTDVU**|Modes 1, 3, 4 — at crest transfer|Modes 1, 3, 4 — at crest transfer|N/A|

### Setting Timer Counter Width in Complementary PWM Mode
| Property | RA2T1 (16-bit) | RA6T2 (32-bit) | RA8T2 (32-bit) |
|---|---|---|---|
| **Max counter value (GTPR)** | 0xFFFF (65,535) | 0xFFFFFFFF (~4.29 billion) | 0xFFFFFFFF (~4.29 billion) |
| **Duty cycle resolution** | Up to 65,535 steps | Up to ~4.29 billion steps | Up to ~4.29 billion steps |
| **Register size** | 16-bit registers | 32-bit registers | 32-bit registers |
| **GTDVU + GTPR constraint** | GTDVU + GTPR ≤ 0xFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF |

### Add **property**
T.B.U

# Configuration for Stack
The following build-time configurations will be provided:
| Property | Options | Description |
| :----: | -------------------- | --- |
|General\|Mode|- Complementary PWM (asymmetric, Mode 1)<br>- Complementary PWM (symmetric, Mode 2)<br>- Complementary PWM (symmetric, Mode 3)<br>- Complementary PWM (symmetric, Mode 4)| Property needs to be updated|
|T.B.U|||

# Testing

## HW Configuration
- Three GPT channels must be available and routed to output pins for three-phase PWM generation.
- Complementary PWM output pins (GTIOCA/GTIOCB) must be connected to external headers.
- A gate driver or inverter circuit is recommended for validating Complementary PWM signals.
- An oscilloscope or logic analyzer is required to observe PWM waveform, dead time, and phase alignment.

### Channel Assignment for Complementary PWM Mode
|No|MCU|Group|Module Name|Master|Slave 1|Slave 2|
|:-:|:-:|:----:|:-:|:-:|:----:|:-:|
|1| RA2T1|Group 1| GPT16|CH0 (GPT160)|CH1 (GPT161)|CH2 (GPT162)|
|2| RA6T2|Group 1| GPT32|CH4 (GPT324)|CH5 (GPT325)|CH7 (GPT327)|
|| RA6T2|Group 2| GPT32|CH7 (GPT327)|CH8 (GPT328)|CH9 (GPT329)|
|3| RA8T2|Group 1| GPT32|CH4 (GPT324)|CH5 (GPT325)|CH7 (GPT327)|
|| RA8T2|Group 2| GPT32|CH7 (GPT327)|CH8 (GPT328)|CH9 (GPT329)|

### Register Address Map
#### Base Address
|Device|Formula| 
|---|---|
|**RA2T1**|GPT16n = `0x4008_9000 + 0x0100 × n` (n=0–2)|
|**RA6T2**|GPT32n = `0x4016_9000 + 0x0100 × n` (n=0–9)|
|**RA8T2**|GPT32n = `0x4032_2000 + 0x0100 × n` (n=0–13)|

#### Register Offset Map (Complementary PWM-related)
|Offset|Register|Operation | Data Width |
|---|---|---|---|
|0x00|**GTWP**|Write Protection|32-bit (all)|
|0x04|**GTSTR**|Software Start (master only valid)|32-bit (all)|
|0x08|**GTSTP**|Software Stop (master only valid)|32-bit (all)|
|0x0C|**GTCLR**|Software Clear (master only valid)|32-bit (all)|
|0x30|**GTCR**|Control: MD[3:0]=0xC/D/E/F, TPCS, SSCGRP, CPSCD*|32-bit (all)|
|0x34|**GTUDDTYC**|Count direction and duty setting|32-bit (all)|
|0x38|**GTIOR**|I/O control: pin output settings, CPSCIR|32-bit (all)|
|0x3C|**GTINTAD**|Interrupt/AD: GRPDTE, GRPABH, GRPABL|32-bit (all)|
|0x40|**GTST**|Status: DTEF, OABHF, OABLF flags|32-bit (all)|
|0x44|**GTBER**|Buffer enable: CCRA, PR buffer control|32-bit (all)|
|0x48|**GTITC**|Interrupt skipping|32-bit (all)|
|0x4C|**GTCNT**|Counter value|**16-bit (RA2T1)** / 32-bit|
|0x4C+offset|**GTCCRA**|Compare match A (duty cycle)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRB**|Compare match B (auto dead-time target)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRC**|Buffer for GTCCRA (single buffer)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRD**|Buffer for GTCCRC (write trigger)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRE**|Buffer for GTCCRA (double buffer)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRF**|Buffer for GTCCRE (write source)|**16-bit (RA2T1)** / 32-bit|
|0x64|**GTPR**|Cycle setting (period)|**16-bit (RA2T1)** / 32-bit|
|0x68|**GTPBR**|Cycle setting buffer|**16-bit (RA2T1)** / 32-bit|
|0x6C|**GTPDBR**|Cycle setting double buffer|**16-bit (RA2T1)** / 32-bit|
|0x8C|**GTDVU**|Dead time value, up-counting|**16-bit (RA2T1)** / 32-bit|
|0x8C+4 |**GTDVD**|Dead time value, down-counting|**16-bit (RA2T1)** / 32-bit|
|0x94|**GTDBU**|Dead time buffer, up|**16-bit (RA2T1)** / 32-bit|
|0x94+4|**GTDBD**|Dead time buffer, down|**16-bit (RA2T1)** / 32-bit|
|0x88|**GTDTCR**|Dead time control: TDE, TDFER|32-bit (all)|

## Unit tests
A new set of unit tests will be added at `peaks/ra/fsp/src/r_gpt/!test`. Additional tests will be written to cover the new features.

- Verify correct generation of Complementary PWM signals on all three phases.
- Validate dead-time insertion between high-side and low-side signals.
- Confirm synchronization across multiple GPT channels (no phase shift).
- Verify correct behavior of buffer transfer modes (crest, trough, both, immediate).
- Validate runtime update of duty cycle and dead time without glitches.
- Measure waveform accuracy using an oscilloscope.

|Test Group| Test Name | Description | Notes |
| ----------|---------- | ----------- | ----- |
|R_GPT_TG2| TC_ComplementaryPWMMode1 | Verify buffer transfer at crest (GTCR.MD = 0xC) and phase-aligned three-phase output | |
|R_GPT_TG2| TC_ComplementaryPWMMode2 | Verify buffer transfer at trough (GTCR.MD = 0xD) and phase-aligned three-phase output | |
|R_GPT_TG2| TC_ComplementaryPWMMode3 | Verify buffer transfer at both crest and trough (GTCR.MD = 0xE); validate single- and double-buffer paths | |
|R_GPT_TG2| TC_ComplementaryPWMMode4 | Verify immediate transfer (GTCR.MD = 0xF) from GTCCRD/GTCCRF to GTCCRA | |
T.B.U

## XML Tests
Add cases in the `yml.j2` file to test XML using existing boards that are equivalent (RA2T1, RA6T2).

T.B.U

## Verification
- Ensure all functional requirements are satisfied as defined in this document.
- Confirm correct operation of Complementary PWM across all supported modes (Modes 1–4).
- Ensure the configured dead time is applied within acceptable tolerance.
- Verify phase alignment across all three channels with zero deviation.
- Ensure buffer transfer behavior complies with the selected mode (crest, trough, both, or immediate).
- Confirm that runtime updates do not introduce glitches or timing violations.
- Ensure compatibility across supported MCU series (RA2T1, RA6T2, RA8T2).


# Question
| Question | Outcome |
| -------- | ------- |
|  |  |

# Usage Notes and other Documentation
Usage notes and documentation will be created for these new modules.

# Submodules & Pack Creation
Pack files will be created/updated for GPT modules.

# Effort estimate

|                           Task                            | Story Point Estimate |
| --------------------------------------------------------- | :------------------: |
| Investigate Complementary PWM Mode for GPT                |         T.B.U        |
| Update MDF                                                |         T.B.U        |
| Add feature for GPT driver (r_gpt)                        |         T.B.U        |
| Test Case                                                 |         T.B.U        |
| MDF Test                                                  |         T.B.U        |
| Finish tests                                              |         T.B.U        |
