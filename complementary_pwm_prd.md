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

![alt text](image-2.png)

Complementary PWM support through GPT will be implemented as driver, reusing the existing driver (`r_gpt_three_phase.c`).

### 1.1 e2studio layer

![alt text](image-3.png)

- `g_timer_comp_pwm_master`, `g_timer_comp_pwm_slave1`, and `g_timer_comp_pwm_slave2` are the GPT module dependencies required by the Complementary PWM driver.
- `g_timer_comp_pwm_master` is used to configure runtime parameters and to synchronize the two upper channels.
- `g_timer_comp_pwm_slave1` is used to generate the PWM waveform with counter = master counter + GTDVU.
- `g_timer_comp_pwm_slave2` is used to take the compare match at the crest section or the trough section.

In the e2studio layer, `g_timer_comp_pwm_master`, `g_timer_comp_pwm_slave1`, and `g_timer_comp_pwm_slave2` share the following property configuration:

![alt text](image-4.png)

### 1.2 Driver layer
At the driver layer, `r_gpt_three_phase.c` will be modified to support Complementary PWM Mode 1, 2, 3, 4 with single or double buffer operation.

#### 1.2.1 Single Buffer Transfer Mechanism for Complementary PWM (Modes 1, 2, 3, 4)
All three devices (RA2T1, RA6T2, and RA8T2) use the same single buffer transfer chain: `GTCCRD → Temp_A → GTCCRC → GTCCRA`.
- Complementary PWM mode 1 (transfer GTCCRC → GTCCRA at crests).
- Complementary PWM mode 2 (transfer GTCCRC → GTCCRA at troughs).
- Complementary PWM mode 3 (transfer GTCCRC → GTCCRA at crests and troughs).
- Complementary PWM mode 4 (immediate GTCCRC → GTCCRA transfer).

#### 1.2.2 Double Buffer Transfer Mechanism for Complementary PWM (Mode 3, 4)

All three devices (RA2T1, RA6T2, and RA8T2) use the same double buffer transfer chain. When `GTBER2.CP3DB = 1`, a second transfer path must be added:
```
GTCCRF → Temp_B → GTCCRE → GTCCRA (In Mode 3, transfer at troughs, but in Mode 4, transfer immediate)
GTCCRD → Temp_A → GTCCRC → GTCCRA (In Mode 3, transfer at crests, but in Mode 4, transfer immediate)
```
> **Note:** In Mode 3, 4 with double buffer enabled, the GTCCRF → GTCCRD → GTCCRA transfer chain introduces a two-cycle delay before a newly written duty value takes effect on the output.

## 2. Instance

Add a new instance to define enable extra feature of r_gpt_three_phase
```c
/* GPT_CFG_OUTPUT_SUPPORT_ENABLE is set to 2 to enable extra features. */
#define GPT_THREE_PHASE_PRV_EXTRA_FEATURES_ENABLED                   (2U)
```

Add Configuration of complementary PWM buffer chain of r_gpt_three_phase
```c
#if (GPT_THREE_PHASE_PRV_EXTRA_FEATURES_ENABLED == GPT_CFG_OUTPUT_SUPPORT_ENABLE)

        /* Configure complementary PWM buffer chain for this channel.
         * R_GPT_Open() already configured single buffer (GTBER2: CMTCA=1, CP3DB=0, CPBTD=0).
         * Here we override for double buffer when applicable, and set initial duty values.
         */
        timer_mode_t mode = p_cfg->p_timer_instance[0]->p_cfg->mode;

        if (mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE1 ||
            mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE2 ||
            mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE3 ||
            mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE4)
        {
            uint32_t duty_initial = p_cfg->p_timer_instance[ch]->p_cfg->duty_cycle_counts;

            if ((THREE_PHASE_BUFFER_MODE_DOUBLE == p_cfg->buffer_mode) &&
                (mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE3 ||
                 mode == TIMER_MODE_COMPLEMENTARY_PWM_MODE4))
            {
                /* Double buffer: GTCCRF -> Temp B -> GTCCRE -> GTCCRA */
                p_instance_ctrl->p_reg[ch]->GTBER2_b.CMTCA = 0x1U;
                p_instance_ctrl->p_reg[ch]->GTBER2_b.CP3DB = 1U;   /* double buffer ON */
                p_instance_ctrl->p_reg[ch]->GTBER2_b.CPBTD = 0U;   /* buffer transfer ENABLED */

                /* Initialize active register + both buffer stages */
                p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRA] = duty_initial;
                p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = duty_initial;
                p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRF] = duty_initial;
            }
            else
            {
                /* Single buffer (Modes 1-4): GTCCRD -> Temp A -> GTCCRA
                 * GTBER2 already configured by R_GPT_Open; set initial duty values */
                p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRA] = duty_initial;
                p_instance_ctrl->p_reg[ch]->GTCCR[GPT_THREE_PHASE_PRV_GTCCRD] = duty_initial;
            }
        }
#endif
```

Add new four GPT driver timer operational modes  :
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
     * not need to be updated at each tough/crest interrupt. Instead, the trough and crest duty cycle values can be
     * set once and only need to be updated when the application needs to change the duty cycle.
     */
    TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3 = 6U,

    TIMER_MODE_COMPLEMENTARY_PWM_MODE1 = 12U, ///< Timer generates Symmetric Triangle-wave PWM output. In Complementary PWM Mode 1, buffer transfer at crest (GTCR.MD = 0xC).
    TIMER_MODE_COMPLEMENTARY_PWM_MODE2 = 13U, ///< Timer generates Symmetric Triangle-wave PWM output. In Complementary PWM Mode 2, buffer transfer at trough (GTCR.MD = 0xD).
    TIMER_MODE_COMPLEMENTARY_PWM_MODE3 = 14U, ///< Timer generates Symmetric Triangle-wave PWM output. In Complementary PWM Mode 3, buffer transfer at crest and trough (GTCR.MD = 0xE).
    TIMER_MODE_COMPLEMENTARY_PWM_MODE4 = 15U  ///< Timer generates Symmetric Triangle-wave PWM output. In Complementary PWM Mode 4, immediate transfer (GTCR.MD = 0xF).
} timer_mode_t;
```
## 3. APIs
This feature will reuse the existing `r_gpt_three_phase.c` driver.

## 4. Module XMLs
This feature will be integrated into the existing GPT driver module using XML definitions consistent with current FSP GPT configurations.

the GPT mdoule XML will be created called
```c
Renesas##HAL Drivers##all##r_gpt####6.2.0.xml
Renesas##HAL Drivers##all##r_gpt_three_phase####6.2.0.xml
```
it will expose the extended GPT implementation of the interface with following

`<provides interface="interface.driver.gpt" />`

Add four new options for complementary pwm mode of r_gpt
```c
        </property>
        <property default="module.driver.timer.mode.mode_periodic" display="General|Mode" id="module.driver.timer.mode" description="Mode selection.\nPeriodic: Generates periodic interrupts or square waves.\nOne-shot: Generate a single interrupt or a pulse wave. Note: One-shot mode is implemented in software. ISRs must be enabled for one-shot even if callback is unused.\nOne-Shot Pulse: Counter performs saw-wave operation with fixed buffer operation.\nSaw-wave PWM: Generates basic saw-wave PWM waveforms.\nTriangle-wave PWM (symmetric, Mode 1): Generates symmetric PWM waveforms with duty cycle determined by compare match set with 32-bit transfer during a crest event and updated at the next trough with single or double buffer operation.\nTriangle-wave PWM (asymmetric, Mode 2): Generates asymmetric PWM waveforms with duty cycle determined by compare match set with 32-bit transfer during a crest/trough event and updated at the next trough/crest.\nTriangle-wave PWM (asymmetric, Mode 3): Generates PWM waveforms with duty cycle determined by compare match set with 64-bit transfer during a crest interrupt and updated at the next trough with fixed buffer operation.">
            <option display="Periodic" id="module.driver.timer.mode.mode_periodic" value="TIMER_MODE_PERIODIC"/>
            <option display="One-Shot" id="module.driver.timer.mode.mode_one_shot" value="TIMER_MODE_ONE_SHOT"/>
            <option display="One-Shot Pulse" id="module.driver.timer.mode_one_shot_pulse" value="TIMER_MODE_ONE_SHOT_PULSE" />
            <option display="Saw-wave PWM" id="module.driver.timer.mode.mode_pwm" value="TIMER_MODE_PWM"/>
            <option display="Triangle-wave PWM (symmetric, Mode 1)" id="module.driver.timer.mode.mode_symmetric_pwm" value="TIMER_MODE_TRIANGLE_WAVE_SYMMETRIC_PWM"/>
            <option display="Triangle-wave PWM (asymmetric, Mode 2)" id="module.driver.timer.mode.mode_asymmetric_pwm" value="TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM"/>
            <option display="Triangle-wave PWM (asymmetric, Mode 3)" id="module.driver.timer.mode.mode_asymmetric_pwm_mode3" value="TIMER_MODE_TRIANGLE_WAVE_ASYMMETRIC_PWM_MODE3"/>
            <option display="Triangle-wave Complementary PWM (symmetric, Mode 1)" id="module.driver.timer.mode.mode_complementary_pwm_mode1" value="TIMER_MODE_COMPLEMENTARY_PWM_MODE1"/>
            <option display="Triangle-wave Complementary PWM (symmetric, Mode 2)" id="module.driver.timer.mode.mode_complementary_pwm_mode2" value="TIMER_MODE_COMPLEMENTARY_PWM_MODE2"/>
            <option display="Triangle-wave Complementary PWM (symmetric, Mode 3)" id="module.driver.timer.mode.mode_complementary_pwm_mode3" value="TIMER_MODE_COMPLEMENTARY_PWM_MODE3"/>
            <option display="Triangle-wave Complementary PWM (symmetric, Mode 4)" id="module.driver.timer.mode.mode_complementary_pwm_mode4" value="TIMER_MODE_COMPLEMENTARY_PWM_MODE4"/>
        </property>
```

Add a new instance support Pin Output of r_gpt_three_phase
```c
        </property>
            <property default="config.driver.gpt_three_phase.output_support_enable.disabled" display="Pin Output Support" id="config.driver.gpt_three_phase.output_support_enable" description="Enables or disables support for outputting PWM waveforms on GTIOCx pins. The &quot;Enabled with Extra Features&quot; option enables support for Triangle wave modes and also enables the features located in the &quot;Extra Features&quot; section of each module instance.">
            <option display="Enabled" id="config.driver.gpt_three_phase.output_support_enable.enabled" value="(1)"/>
            <option display="Disabled" id="config.driver.gpt_three_phase.output_support_enable.disabled" value="(0)"/>
            <option display="Enabled with Extra Features" id="config.driver.gpt_three_phase.output_support_enable.enabled_extra" value="(2)"/>
        </property>
```

Add four new options for complementary pwm mode of r_gpt_three_phase
```c
        <property default="module.driver.three_phase.mode.mode_symmetric_pwm" display="General|Mode" id="module.driver.three_phase.mode" description="Mode selection.\nTriangle-Wave Symmetric PWM: Generates symmetric PWM waveforms with duty cycle determined by compare match set during a crest interrupt and updated at the next trough.\nTriangle-Wave Asymmetric PWM: Generates asymmetric PWM waveforms with duty cycle determined by compare match set during a crest/trough interrupt and updated at the next trough/crest.">
            <option display="Triangle-Wave Symmetric PWM" id="module.driver.three_phase.mode.mode_symmetric_pwm" value="mode_symmetric_pwm"/>
            <option display="Triangle-Wave Asymmetric PWM" id="module.driver.three_phase.mode.mode_asymmetric_pwm" value="mode_asymmetric_pwm"/>
            <option display="Triangle-Wave Asymmetric PWM (Mode 3)" id="module.driver.three_phase.mode.mode_asymmetric_pwm_mode3" value="mode_asymmetric_pwm_mode3"/>
            <option display="Triangle-Wave Symmetric Complementary PWM (Mode 1)" id="module.driver.three_phase.mode.mode_complementary_pwm_mode1" value="mode_complementary_pwm_mode1"/>
            <option display="Triangle-Wave Symmetric Complementary PWM (Mode 2)" id="module.driver.three_phase.mode.mode_complementary_pwm_mode2" value="mode_complementary_pwm_mode2"/>
            <option display="Triangle-Wave Symmetric Complementary PWM (Mode 3)" id="module.driver.three_phase.mode.mode_complementary_pwm_mode3" value="mode_complementary_pwm_mode3"/>
            <option display="Triangle-Wave Symmetric Complementary PWM (Mode 4)" id="module.driver.three_phase.mode.mode_complementary_pwm_mode4" value="mode_complementary_pwm_mode4"/>
        </property>
```

## 5. Constraint
### Setting Range of GTPBR and GTPDBR in Complementary PWM Mode
|Restriction|RA2T1|RA6T2|RA8T2|
|---|---|---|---|
|**GTPBR ≥ GTPR − GTDVU**|Modes 1, 3, 4 — at crest transfer|Modes 1, 3, 4 — at crest transfer|N/A|
|**GTPDBR ≥ GTPR − GTDVU**|Modes 1, 3, 4 — at crest transfer|Modes 1, 3, 4 — at crest transfer|N/A|

**Rationale:** This constraint ensures that period buffer transfers (GTPBR/GTPDBR → GTPR) complete before the counter reaches the peak value. When GTPBR or GTPDBR is set too close to GTPR, the buffer transfer may not complete in time, causing timing violations or missed updates. The margin of GTDVU (dead time) provides a safe window for the hardware to complete the transfer operation during modes with crest-aligned transfers (Modes 1, 3, 4).

### Setting Timer Counter Width in Complementary PWM Mode
| Property | RA2T1 (16-bit) | RA6T2 (32-bit) | RA8T2 (32-bit) |
|---|---|---|---|
| **Max counter value (GTPR)** | 0xFFFF (65,535) | 0xFFFFFFFF (~4.29 billion) | 0xFFFFFFFF (~4.29 billion) |
| **Duty cycle resolution** | Up to 65,535 steps | Up to ~4.29 billion steps | Up to ~4.29 billion steps |
| **Register size** | 16-bit registers | 32-bit registers | 32-bit registers |
| **GTDVU + GTPR constraint** | GTDVU + GTPR ≤ 0xFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF |

### Channel Continuity Requirement
**GPT channels used for Complementary PWM must be consecutive.** According to the RA MCU User's Manual, "among consecutive three channels, the lowest channel is referred to as master channel, and the adjacent upper two channels are referred to as slave channel 1 (lower) and slave channel 2 (upper)."

Valid configurations:
- ✅ Channels 1-2-3 (master=1, slave1=2, slave2=3)
- ✅ Channels 4-5-6 (master=4, slave1=5, slave2=6)

Invalid configurations:
- ❌ Channels 1-4-6 (non-consecutive)
- ❌ Channels 2-3-5 (gap between 3 and 5)

This hardware requirement ensures proper synchronization and buffer transfer across the three-phase PWM output.

# Configuration for Stack
The following options shall be configurable for r_gpt_three_phase driver.

## Build Options - Common
g_three_phase
| Property | Options | Description |
| :----: | -------------------- | --- |
|Common > Parameter Checking| Enabled/Disabled/Default (BSP)|Select whether to include parameter checking in the build|
|Common > Pin Output Support| Enabled/Disabled/Enabled with Extra Features| Select whether to enable pin output support|

g_timer
| Property | Options | Description |
| :----: | -------------------- | --- |
|Common > Parameter Checking| Enabled/Disabled/Default (BSP)|Select whether to include parameter checking in the build|
|Common > Pin Output Support| Enabled/Disabled/Enabled with Extra Features| Select whether to enable pin output support|
|Common > Write Protect Enable| Enabled/Disabled| Select whether to enable Write Protect|

## Build Options - General
| Property | Options | Description |
| :----: | -------------------- | --- |
|General > Name| g_three_phase, or any valid C variable name|Module name|
|General > Mode|- Triangle-Wave Symmetric Complementary PWM (Mode 1)<br>- Triangle-Wave Symmetric Complementary PWM (Mode 2)<br>- Triangle-Wave Symmetric Complementary PWM (Mode 3)<br>- Triangle-Wave Symmetric Complementary PWM (Mode 4)| Select Complementary PWM mode 1, 2, 3, or 4|
|General > Period| 0 < Integer <= MCU GPT Max Timer | Slect timer Period value. both RA6T2 and RA8T2 are support 32-bit timer, but 16-bit timer only for RA2T1|
|General > GPT U-Channel| 0 < Integer <= MCU GPT Max duty cycle | Select duty cycle value for master channel|
|General > GPT V-Channel| 0 < Integer <= MCU GPT Max duty cycle | Select duty cycle value for slave1 channel|
|General > GPT W-Channel| 0 < Integer <= MCU GPT Max duty cycle | Select duty cycle value for slave2 channel|
|General > Buffer Mode| Single Buffer/ Double Buffer| Select enable single or double buffer|

g_timer
| Property | Options | Description |
| :----: | -------------------- | --- |
|General > Name| g_timer0, or any valid C variable name|Module name of master, slave1, or slave 2|
|General > Mode|- Triangle-Wave Complementary PWM (Symmetric, Mode 1)<br>- Triangle-Wave Complementary PWM (Symmetric, Mode 2)<br>- Triangle-Wave Complementary PWM (Symmetric, Mode 3)<br>- Triangle-Wave Complementary PWM (Symmetric, Mode 4)| Select Complementary PWM mode 1, 2, 3, or 4|
|General > Duty Cycle Pecent| 0 < Integer <= 100 | Select duty cycle value for master, slave1, or slave2 channel|

## Build Options - Pins
g_timer
| Property | Options | Description |
| :----: | -------------------- | --- |
|Pins > GTIOC0A|-|select up to Enable pin with operation Mode GTIOCA and GTIOCB for master, slave1, or slave2 channel|
|Pins > GTIOC0B|-|select up to Enable pin with operation Mode GTIOCA and GTIOCB for master, slave1, or slave2 channel|

# Testing

## HW Configuration
- Three GPT channels must be available and routed to output pins for three-phase PWM generation.
- Complementary PWM output pins (GTIOCA/GTIOCB) must be connected to external headers.
- A gate driver or inverter circuit is recommended for validating Complementary PWM signals.
- An oscilloscope or logic analyzer is required to observe PWM waveform, dead time, and phase alignment.

### Channel Assignment for Complementary PWM Mode
The following channel configurations are used for testing Complementary PWM functionality. Multiple configurations per MCU are tested to validate that the implementation works correctly across different consecutive channel sets and to ensure portability.

|No|MCU|Module Name|Master|Slave 1|Slave 2|Test Purpose|
|:-:|:-:|:-:|:-:|:----:|:-:|---|
|1| RA2T1| GPT16|CH0 (GPT160)|CH1 (GPT161)|CH2 (GPT162)|Primary test configuration|
|2| RA6T2| GPT32|CH4 (GPT324)|CH5 (GPT325)|CH6 (GPT326)|Test alternate channel set|
|3| RA6T2| GPT32|CH7 (GPT327)|CH8 (GPT328)|CH9 (GPT329)|Validate multiple configurations|
|4| RA8T2| GPT32|CH4 (GPT324)|CH5 (GPT325)|CH6 (GPT326)|Test alternate channel set|
|5| RA8T2| GPT32|CH7 (GPT327)|CH8 (GPT328)|CH9 (GPT329)|Validate multiple configurations|

**Note:** Testing multiple consecutive channel sets ensures the driver implementation is not hardcoded to specific channel numbers and validates proper operation across different hardware configurations.

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
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_OM_01 | Verify GTCCRD transfers to GTCCRA at end of crest section |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_OM_02 |  Verify GTCCRD transfers to GTCCRA at end of trough section. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_OM_03 | Verify transfer at both boundaries, single and double buffer. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_OM_04 | Verify immediate transfer (GTCR.MD = 0xF) from GTCCRD/GTCCRF to GTCCRA |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_OM_04 | Verify immediate transfer, bypass buffer chain. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DT_05 | Verify Configurable dead time via GTDVU register. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DT_06 | Verify Valid range - 0 < GTDVU < GTPR. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DT_07 | Verify No buffer operation for dead time register. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DT_08 | Verify Non-overlapping guard - dead time prevents shoot-through. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DC_09 | Verify Independent duty cycle control across three channels (U, V, W). |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DC_10 | Verify GTCCRA >= GTPR means 0% duty (positive OFF, negative ON). |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DC_11 | Verify GTCCRA = 0 means 100% duty (positive ON, negative OFF). |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_DC_12 | Verify Prevent 16-bit overflow on compare match value (RA2T1 only). |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_BUF_13 | Verify Single buffer chain for Modes 1-3 (GTCCRD -> Temp A -> GTCCRA). |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_BUF_14 | Verify Double buffer chain for Mode 3 (GTCCRF -> Temp B -> GTCCRE -> GTCCRA). |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_BUF_15 | Verify Mode 4 immediate bypass - GTCCRD transfers directly to GTCCRA. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_BUF_16 | Verify Slave channel 2 (W phase) write ordering - W must be written LAST. |-|
|r_gpt_test_tg3_comp_pwm| comp_pwm_test_REQ_SEC_17 | Verify Support for five counting operation sections. |-|

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

|                           Task                                   | Story Point Estimate | Note |
| ---------------------------------------------------------------- | :------------------: | :-------------: |
| Investigate Complementary PWM (Modes 1-4) for GPT                |         1            |         -       |
| Update xml for Complementary PWM (Modes 1-4)                     |         0.5          |         -       |
| Create API for Complementary PWM (Modes 1-4)                     |         0.5          |         -       |
| Update HAL driver of GPT to support Complementary PWM (Modes 1-4)|         2            |         -       |
| Testing for updated source code                                  |         3            |         -       |
| Testing for xml                                                  |         0.5          |         -       |
| Add example for Complementary PWM (Modes 1-4)                    |         1            |         -       |
| Create usage note                                                |         0.5          |         -       |
