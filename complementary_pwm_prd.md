---
title: GPT complementary PWM mode Support PRD
description: Support Complementary PWM mode for GPT feature
published: true
date: 2026-04-13
tags: 
editor: markdown
dateCreated: 
---

# Overview
Target release: v6.5.0 rc0
Epic: https://jira.eng.renesas.com/browse/FSPRA-5725

The document describes the proposal for supporting Complementary PWM as an extended feature of GPT peripheral module on RA MCUs.

# Goals
Add GPT Complementary PWM support for RA MCUs with GPT Complementary PWM capabilities.

# Strategic fit
GPT Complementary PWM implementation provides three-phase PWM waveform generation with hardware dead time insertion. it targets motor control and power conversion applications. GPT Complementary PWM support is requested by Renesas business development and management.
  
# Background
GPT Complementary PWM on RA MCUs is has exactly four modes. In Complementary PWM mode, a three-phase PWM waveform with dead time that ensures the linearity in the vicinity of duty 0% and 100% can be output using the GTCNT counter of consecutive three channels. There are four modes depending on the differences in buffer operation:

- Complementary PWM mode 1 (transfer at crests).
- Complementary PWM mode 2 (transfer at troughs).
- Complementary PWM mode 3 (transfer at crests and troughs).
- Complementary PWM mode 4 (immediate transfer).

![alt text](image-1.png)

<u>**List of supported MCUs:**</u>
|No|MCU|Core|Test|Note
|:-:|:-:|:----:|:-:|:----:|
|1|RA2T1|CM23|Yes|-|
|2|RA6T2|CM33|Yes|-|
|3|RA8T2|CM85 & CM33|No|Current released packet has not supported yet|

# Requirements 
|Requirement ID|Title|Use case|Important|Notes|
|----|----|--------------------|---|----|
|REQ-OM-01|Operating mode 1|GTCCRC transfers to GTCCRA at the end of crest section|Must have|Transfer timing at crest boundary|
|REQ-OM-02|Operating mode 2|GTCCRC transfers to GTCCRA at the end of trough section|Must have|Transfer timing at trough boundary|
|REQ-OM-03|Operating mode 3|TCCRC transfers at both boundaries. Supports single buffer (GTCCRD only) and double buffer (GTCCRD + GTCCRF via GTBER2.CP3DB)|Must have|Transfer timing at crest and trough boundary|
|REQ-OM-04|Operating mode 4|Values written to GTCCRD/GTCCRF bypass the normal buffer chain and transfer directly to GTCCRA|Must have|Required for motor control|Transfer timing is immediate
|REQ-DT-05|Configurable dead time|The system shall allow configuration of dead time in timer count units via the GTDVU register of the master channel|Must have|-|
|REQ-DT-06|Valid range of dead time value|Dead time value shall satisfy: 0 < GTDVU < GTPR|Must have|-|
|REQ-DT-07|Not support buffer operation for dead time register |Buffer operation shall NOT be used for the GTDVU register in any Complementary PWM mode|Must have|-|
|REQ-DT-08|Support a non-overlapping guard for dead time register|Dead time shall create a non-overlapping guard between positive-phase OFF and negative-phase ON transitions to prevent shoot-through in power stage drivers|Must have|Prevent shoot-through|
|REQ-DC-09|Independent duty cycle control on Three GPT channels|The system shall support independent duty cycle control for three phases (U, V, W) in the range 0% to 100%|Must have|-|
|REQ-DC-10|Compare match point Reachs GTPR|When GTCCRA ≥ GTPR, duty shall be 0% (positive-phase OFF, negative-phase ON)|Must have|-|
|REQ-DC-11|Compare match point equals zero|When GTCCRA = 0, duty shall be 100% (positive-phase ON, negative-phase OFF)|Must have|-|
|REQ-DC-12|Prevent 16-bit overflow for compare match value|The conversion shall use uint32_t intermediate arithmetic to prevent 16-bit overflow|Must have|RAT21 only|
|REQ-BUF-13|Transfer single buffer chain for Modes 1–3 |For Modes 1–3, the normal buffer chain shall be: GTCCRD → Temp Register A → GTCCRC → GTCCRA|Must have|-|
|REQ-BUF-14|Transfer double buffer chain for Modes 3 only|For Mode 3 double buffer, an additional path shall be: GTCCRF → Temp Register B → GTCCRE → GTCCRA|Must have|-|
|REQ-BUF-15|Transfer buffer chain for Modes 4|For Mode 4, an immediate bypass path shall transfer GTCCRD directly to GTCCRA (and GTCCRF to GTCCRA for double buffer)|Must have|-|
|REQ-BUF-16|Writting slave 2 is performed lastly|Writing to slave channel 2 (GPT16n+2) GTCCRD shall always be performed LAST, as it triggers simultaneous temporary register transfer across all three channels|Must have|-|
|REQ-SEC-17|Support on five counting operation sections|The system shall recognize five operation sections based on master counter position|Must have|-|

# User interaction and design
## 1. Design Idea
The development of Complementary PWM support via GPT will be implement as middleware, we will re-uese driver (existing `r_gpt.c`)

### 1.1 e2studio layer
![alt text](image-2.png)
- g_timer_master, g_timer_slave1, and g_timer_slave2 are dependencies in GPT module when using Complementary PWM middleware.
- g_timer_master will be used to congirure runtime parameters and synchronize two upper channels.
- g_timer_slave1 will be used to generate PWM waveform with counter = Master counter + GTDVU.
- g_timer_slave1 will be used to take compare match at crest section or trough section.

At e2studion layer, g_timer_master, g_timer_slave1, and g_timer_slave2 will have same property as below: 
![alt text](image-3.png)

### 1.2 Middleware layer
We will create new API for handle GPT Complementary PWM mode 1, 2, 3, 4. please refer section "3. APIs" for list of APIs to be created

Basically, Middleware only handle for below task:
- Open/Close Middleware and GPT module (Open/ close APIs via Open/Close all 3 GPT channels for Complementary PWM)
- Start/Stop Middle ware and GPT module (Start/Stop APIs via Start/Stop Master channel - Slave channels follow automatically )
- Configure dead time register (GTDVU) on master channel 
- Set three-phase duty cycles.


### 1.3 Driver layer
At the driver layer, we will modify `r_gpt.c` to support Complementary PWM mode 3 with double buffer operation 
please refer to section "3. APIs" for list of APIs to be created.
![alt text](image-4.png)

#### 1.3.1 Single Buffer Transfer Mechanism for Complementary PWM (Mode 1, 2, 3)
All 3 devices (RA2T1, RA6T2, and RA8T2) use the same single buffer transfer chain: `GTCCRD → Temp_A → GTCCRC → GTCCRA`

#### 1.3.2 Double Buffer Transfer Mechanism for Complementary PWM (Mode 3 only)

All 3 devices (RA2T1, RA6T2, and RA8T2) use the same double buffer transfer chain When `GTBER2.CP3DB = 1`, need to add the second transfer path :
```
GTCCRF → Temp_B → GTCCRE → GTCCRA (at trough)
GTCCRD → Temp_A → GTCCRC → GTCCRA (at crest)
```
#### 1.3.3 Buffer Transfer Mechanism for Complementary PWM (Mode 4)
Mode 4 adds the second transfer path with immediate timing from GTCCRD/GTCCRF to GTCCRA

## 2. Instance
Add new instance for new API of comp_pwm_set_duty_3phase & comp_pwm_configure_dead_time
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

Add new 4 GPT driver timer operational modes
```C
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
     * not need to be updated at each tough/crest interrupt. Instead, the trough and crest duty cycle values can be
     * set once and only need to be updated when the application needs to change the duty cycle.
     */
    TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3 = 6U

    TIMER_MODE_COMPLEMENTARY_PWM_MODE1 = 12U ///< Transfer at crest
    TIMER_MODE_COMPLEMENTARY_PWM_MODE2 = 13U ///< Transfer at trough 
    TIMER_MODE_COMPLEMENTARY_PWM_MODE3 = 14U ///< Transfer at crest and trough 
    TIMER_MODE_COMPLEMENTARY_PWM_MODE4 = 15U ///< Immediate transfer

} timer_mode_t;
```
## 3. APIs
This featurn  will implement the existing `r_gpt.c`
### 3.1 APIs and internal functions support the using of Complementary PWM
|No| Name | Requirement ID | Reason | Classify | Note |
| ----------|---------- | ----------- | ----- |----------- | ----- |
|1| comp_pwm_init | N/A | Initialize GPT channels for complementary PWM operation | New | - |
|2| comp_pwm_start | N/A | Start Complementary PWM output on all 3 channels | Modify | - |
|3| comp_pwm_set_dead_time | N/A | Set dead time value for complementary PWM | New | - |
|4| comp_pwm_set_duty_3phase | N/A | Set three-phase duty cycles for U, V, W phases | New | - |
|5| comp_pwm_stop | N/A | Stop Complementary PWM output on all 3 channels | Modify | - |

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
| **Duty cycle resolution** | Max 65,535 steps | Max ~4.29 billion steps | Max ~4.29 billion steps |
| **Register size** | 16-bit registers | 32-bit registers | 32-bit registers |
| **GTDVU + GTPR constraint** | GTDVU + GTPR ≤ 0xFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF |

### Add **property**
T.B.U

# Configuration for Stack
Following Build-Time configurations will be provided:
| Property | Options | Description |
| :----: | -------------------- | --- |
|General\|Mode|- Complementary PWM (asymmetric, Mode 1)<br>- Complementary PWM (symmetric, Mode 2)<br>- Complementary PWM (symmetric, Mode 3)<br>- Complementary PWM (symmetric, Mode 4)| Property needs to be updated|
|T.B.U|||

# Testing

## HW Configuration
- Three GPT channels must be available and routed to output pins for three-phase PWM generation.
- Complementary PWM output pins (GTIOCA/GTIOCB) must be connected to external headers.
- A gate driver or inverter circuit is recommended for validating Complementary PWM signals.
- Oscilloscope or logic analyzer is required to observe PWM waveform, dead time, and phase alignment.

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

#### Register offset Map (Related Complementary PWM)
|Offset|Register|Operation | Data Width |
|---|---|---|---|
|0x00|**GTWP**|Write Protection|32-bit (all)|
|0x04|**GTSTR**|Software Start (master only valid)|32-bit (all)|
|0x08|**GTSTP**|Software Stop (master only valid)|32-bit (all)|
|0x0C|**GTCLR**|Software Clear (master only valid)|32-bit (all)|
|0x30|**GTCR**|Control: MD[3:0]=0xC/D/E/F, TPCS, SSCGRP, CPSCD*|32-bit (all)|
|0x34|**GTUDDTYC**|Count Direction and Duty Setting|32-bit (all)|
|0x38|**GTIOR**|I/O Control: pin output settings, CPSCIR|32-bit (all)|
|0x3C|**GTINTAD**|Interrupt/AD: GRPDTE, GRPABH, GRPABL |32-bit (all)|
|0x40|**GTST**|Status: DTEF, OABHF, OABLF flags|32-bit (all)|
|0x44|**GTBER**|Buffer Enable: CCRA, PR buffer control | 32-bit (all)|
|0x48|**GTITC**|Interrupt Skipping |32-bit (all)|
|0x4C|**GTCNT**|Counter value |**16-bit (RA2T1)** / 32-bit|
|0x4C+offset|**GTCCRA**|Compare match A (duty cycle)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRB**|Compare match B (auto dead time target) |**16-bit (RA2T1)** / 32-bit|
| |**GTCCRC**|Buffer for GTCCRA (single buffer)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRD**|Buffer for GTCCRC (write trigger)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRE**|Buffer for GTCCRA (double buffer)|**16-bit (RA2T1)** / 32-bit|
| |**GTCCRF**|Buffer for GTCCRE (write source)|**16-bit (RA2T1)** / 32-bit|
|0x64|**GTPR**|Cycle Setting (period)|**16-bit (RA2T1)** / 32-bit|
|0x68|**GTPBR**|Cycle Setting Buffer|**16-bit (RA2T1)** / 32-bit|
|0x6C|**GTPDBR**|Cycle Setting Double Buffer|**16-bit (RA2T1)** / 32-bit|
|0x8C|**GTDVU**|Dead Time Value Up-counting|**16-bit (RA2T1)** / 32-bit|
|0x8C+4 |**GTDVD**|Dead Time Value Down-counting|**16-bit (RA2T1)** / 32-bit|
|0x94|**GTDBU**|Dead Time Buffer Up|**16-bit (RA2T1)** / 32-bit|
|0x94+4|**GTDBD**|Dead Time Buffer Down|**16-bit (RA2T1)** / 32-bit|
|0x88|**GTDTCR**|Dead Time Control: TDE, TDFER|32-bit (all)|

## Unit tests
New set of Unit tests will be added at `peaks/ra/fsp/src/r_gpt/!test`. Additional tests will be written to test the new features.

- Verify correct generation of Complementary PWM signals on all three phases.
- Validate dead-time insertion between high-side and low-side signals.
- Confirm synchronization across multiple GPT channels (no phase shift).
- Verify correct behavior of buffer transfer modes (crest, trough, both).
- Validate runtime update of duty cycle and dead time without glitches.
- Measure waveform accuracy using oscilloscope.

|Tesst Group| Test Name | Description | Notes |
| ----------|---------- | ----------- | ----- |
|R_GPT_TG2| TC_ComplementaryPWMMode1 | | |
|R_GPT_TG2| TC_ComplementaryPWMMode2 | | |
|R_GPT_TG2| TC_ComplementaryPWMMode3 | | |
|R_GPT_TG2| TC_ComplementaryPWMMode4 | | |
T.B.U

## XML Tests
Implement adding cases in yml.j2 file to test xml using existing boards that are equivalent (RA2T1, RA6T2)

T.B.U

## Verification
- Ensure all functional requirements are satisfied as defined in this document.
- Confirm correct operation of Complementary PWM across all supported modes (Modes 1–4).
- Ensure configured dead time is applied within acceptable tolerance.
- Verify phase alignment across all three channels with zero deviation.
- Ensure buffer transfer behavior complies with the selected mode (crest, trough, or both).
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
