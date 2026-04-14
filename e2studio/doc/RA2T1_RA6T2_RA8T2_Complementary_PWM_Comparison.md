# Phân Tích Sự Khác Biệt: Complementary PWM Mode trên RA2T1 vs RA6T2 vs RA8T2

---

## 1. Tổng Quan Kiến Trúc Phần Cứng

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **CPU Core** | Arm Cortex-M23 | Arm Cortex-M33 | Arm Cortex-M85 + Cortex-M33 |
| **Max Frequency** | 64 MHz | 240 MHz | 1 GHz (M85) + 250 MHz (M33) |
| **GPT Timer Width** | **16-bit** (GPT16) | **32-bit** (GPT32) | **32-bit** (GPT32) |
| **Tổng GPT Channels** | 4 (GPT160–GPT163) | 10 (GPT320–GPT329) | 14 (GPT320–GPT3213) |
| **GPT Core Clock Source** | PCLKD only (synchronous) | PCLKD (sync) hoặc GPTCLK (async) | PCLKD (sync) hoặc GPTCLK (async) |
| **Max PCLKD** | 64 MHz | 120 MHz | 200 MHz |
| **GPTCLK (async)** | Không hỗ trợ | Có (lên đến 200 MHz) | Có (lên đến 160 MHz+) |
| **POEG Groups** | 2 (A, B) | 4 (A–D) | 4 (A–D) |
| **PDG (PWM Delay Generation)** | Có (4 channels, CH0–3) | Có (4 channels, CH0–3) | Có (4 channels, CH0–3) |

---

## 2. Channel Assignment cho Complementary PWM Mode

Đây là **sự khác biệt quan trọng nhất** khi porting code giữa 3 devices.

### RA2T1: Channel 0 là Master

| Role | Channel | Module Name |
|---|---|---|
| Master | CH0 | GPT160 |
| Slave 1 | CH1 | GPT161 |
| Slave 2 | CH2 | GPT162 |

- Chỉ **1 complementary PWM channel group** duy nhất (CH0, CH1, CH2).
- CH3 (GPT163) **KHÔNG** hỗ trợ complementary PWM — chỉ hỗ trợ phase counting và external pulse width measuring.
- Saw-wave PWM mode 2 cũng chỉ hỗ trợ trên CH0–CH2 (không có trên CH3).
- Asymmetric automatic dead time: CH0–CH2; Symmetric automatic dead time: CH3 only.

### RA6T2: Channel 4 hoặc 7 là Master

| Group | Master | Slave 1 | Slave 2 |
|---|---|---|---|
| Group 1 | CH4 (GPT324) | CH5 (GPT325) | CH6 (GPT326) |
| Group 2 | CH7 (GPT327) | CH8 (GPT328) | CH9 (GPT329) |

- **2 complementary PWM channel groups** khả dụng.
- CH0–CH3 (GPT320–GPT323): **KHÔNG** hỗ trợ complementary PWM (MD[3] bit không available). Chỉ hỗ trợ Saw-wave PWM mode 1, Triangle-wave PWM mode. Đổi lại, CH0–CH3 có **High Resolution PWM**, phase counting, và external pulse width measuring.
- CH4–CH9: Hỗ trợ đầy đủ complementary PWM mode 1/2/3/4 (MD[3] bit available).

### RA8T2: Channel 4 hoặc 7 là Master (giống RA6T2)

| Group | Master | Slave 1 | Slave 2 |
|---|---|---|---|
| Group 1 | CH4 (GPT324) | CH5 (GPT325) | CH6 (GPT326) |
| Group 2 | CH7 (GPT327) | CH8 (GPT328) | CH9 (GPT329) |

- **2 complementary PWM channel groups** (giống RA6T2).
- CH0–CH3 và **CH10–CH13** (GPT3210–GPT3213): **KHÔNG** hỗ trợ complementary PWM (MD[3] bit read as 0). Chỉ hỗ trợ Saw-wave PWM mode 1 và Triangle-wave PWM mode.
- CH4–CH9: Hỗ trợ đầy đủ complementary PWM mode 1/2/3/4.

**Constraint khi porting:**
- RA2T1 code dùng CH0 (master), CH1 (slave1), CH2 (slave2) → **phải đổi sang CH4/CH5/CH6 hoặc CH7/CH8/CH9** khi port sang RA6T2/RA8T2.
- RA2T1 dùng base address `GPT16n`, RA6T2/RA8T2 dùng `GPT32n` → register offset và size khác nhau.

---

## 3. Timer Counter Width — Ảnh Hưởng Trực Tiếp đến Resolution

| Thuộc Tính | RA2T1 (16-bit) | RA6T2 (32-bit) | RA8T2 (32-bit) |
|---|---|---|---|
| **Max counter value (GTPR)** | 0xFFFF (65,535) | 0xFFFFFFFF (~4.29 tỷ) | 0xFFFFFFFF (~4.29 tỷ) |
| **PWM frequency resolution** | Thấp hơn (ít steps hơn) | Rất cao | Rất cao |
| **Duty cycle resolution** | Max 65,535 steps | Max ~4.29 billion steps | Max ~4.29 billion steps |
| **Register size** | 16-bit registers | 32-bit registers | 32-bit registers |
| **GTDVU + GTPR constraint** | GTDVU + GTPR ≤ 0xFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF | GTDVU + GTPR ≤ 0xFFFFFFFF |

**Ví dụ thực tế:**
- Ở 64 MHz clock, PWM 20 kHz → RA2T1: GTPR = 3200 (chỉ 3200 duty steps, ~0.03% resolution).
- Ở 200 MHz clock, PWM 20 kHz → RA6T2/RA8T2: GTPR = 10000 (10000 steps, ~0.01% resolution), và có thể tăng lên rất nhiều nếu dùng prescaler thấp hơn.

**Constraint:** Khi port từ RA2T1, các giá trị GTPR, GTCCRA, GTDVU cần được scale lại cho 32-bit range.

---

## 4. Clock Prescaler và External Trigger

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

**Constraint:** RA2T1 thiếu GTCLK/128 và GTCLK/512. Nếu firmware RA6T2/RA8T2 dùng prescaler này → cần chọn prescaler khác khi port sang RA2T1.

---

## 5. Dead Time Configuration

Cả 3 devices đều có cùng cấu trúc dead time cơ bản:

| Register | Chức Năng | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| **GTDVU** | Dead time value (up-counting direction) | ✓ | ✓ | ✓ |
| **GTDVD** | Dead time value (down-counting direction) | ✓ | ✓ | ✓ |
| **GTDBU** | Dead time buffer (up) | ✓ | ✓ | ✓ |
| **GTDBD** | Dead time buffer (down) | ✓ | ✓ | ✓ |
| **GTDTCR** | Dead time control register | ✓ | ✓ | ✓ |

### Automatic Dead Time Setting Function

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Asymmetric auto dead time** | CH0–CH2 only | All complementary channels (CH4–CH9) | All complementary channels (CH4–CH9) |
| **Symmetric auto dead time** | CH3 only | Via GTDTCR.TDFER bit (all channels) | Via GTDTCR.TDFER bit (all channels) |
| **TDFER bit** | Có | Có | Có |
| **Prohibited: write GTCCRB** | Khi TDE=1 | Khi TDE=1 | Khi TDE=1 |

**TDFER bit (Same Dead Time for First/Second Half):** Khi TDFER=1, GTDVD value bị bỏ qua và GTDVU được dùng cho cả up-counting và down-counting, tạo symmetric dead time. Có trên cả 3 devices.

---

## 6. Dead Time Error Handling (Chi Tiết)

### 6.1 Detection Mechanism

Dead time error xảy ra khi change point của waveform (sau khi thêm dead time) vượt quá count period. Mechanism **giống nhau trên cả 3 devices**:

**Setting conditions cho DTEF (Dead Time Error Flag):**

| Chế Độ | Điều Kiện | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| Triangle-wave, Up-counting | GTCCRA − GTDVU ≤ 0 | ✓ | ✓ | ✓ |
| Triangle-wave, Down-counting | GTCCRA − GTDVD < 0 | ✓ | ✓ | ✓ |
| Saw-wave one-shot, Up-counting | GTCCRA − GTDVU < 0, HOẶC GTCCRA + GTDVD > GTPR | ✓ | ✓ | ✓ |
| Saw-wave one-shot, Down-counting | GTCCRA + GTDVU > GTPR, HOẶC GTCCRA − GTDVD < 0 | ✓ | ✓ | ✓ |

**Clearing condition:** Khi change point trở lại trong count period.

### 6.2 Error Response — POEG Integration

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **GTINTAD.GRPDTE bit** | Có | Có | Có |
| **DTEF → POEG output disable** | Có (khi GRPDTE=1) | Có (khi GRPDTE=1) | Có (khi GRPDTE=1) |
| **GPT dead time error interrupt** | **Không** (dùng POEG interrupt thay thế) | **Không** (dùng POEG interrupt thay thế) | **Không** (dùng POEG interrupt thay thế) |
| **DTEF flag type** | Read-only (auto clear) | Read-only (auto clear) | Read-only (auto clear) |

### 6.3 Short-Circuit Protection Flags

| Flag | Mô Tả | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|---|
| **OABHF** | GTIOCnA và GTIOCnB cùng output HIGH | ✓ | ✓ | ✓ |
| **OABLF** | GTIOCnA và GTIOCnB cùng output LOW | ✓ | ✓ | ✓ |
| **GRPABH bit** | Enable OABHF → POEG disable request | ✓ | ✓ | ✓ |
| **GRPABL bit** | Enable OABLF → POEG disable request | ✓ | ✓ | ✓ |

### 6.4 Waveform Adjustment When Dead-Time Error Occurs

Khi dead-time error xảy ra, cả 3 devices đều **tự động điều chỉnh change point** để bảo đảm dead time. Giá trị điều chỉnh được set tự động vào GTCCRB register. Cơ chế giống nhau trên cả 3 devices, nhưng lưu ý:

- Trong **saw-wave one-shot pulse mode**, nếu thứ tự change point bị đảo sau khi adjust, complementary relation **không được đảm bảo**.
- Trong **triangle-wave PWM mode**, nếu GTCCRA = 0x00000000 hoặc ≥ GTPR, output được control bởi output protection function.
- Khi GTCCRA ≥ [GTPR + GTDV(U/D)], GTCCRB được set tới upper limit = [GTPR − 1].

---

## 7. High Resolution PWM

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **High Resolution PWM** | **Không hỗ trợ** | CH0–CH3 (GPT320–323) | CH0–CH3 (GPT320–323) |
| **Channels hỗ trợ** | N/A | 4 channels with HR | 4 channels with HR |

**Quan trọng:** Trên RA6T2 và RA8T2, high resolution chỉ có trên CH0–CH3, nhưng complementary PWM chỉ có trên CH4–CH9. **Hai tính năng này nằm trên nhóm channel khác nhau — không thể dùng đồng thời high resolution và complementary PWM trên cùng channel.**

---

## 8. GTPR Buffer Operation (Cycle Setting)

Cả 3 devices đều có cùng buffer chain: **GTPDBR → GTPBR → GTPR** (double buffer).

### Setting Range of GTPBR and GTPDBR in Complementary PWM Mode

| Restriction | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Documented section** | Section 20.10.4 | Section 21.10.4 | **Không có section tương đương** |
| **GTPBR ≥ GTPR − GTDVU** | Có (modes 1, 3, 4 — crest transfer) | Có (modes 1, 3, 4 — crest transfer) | Không document riêng |
| **GTPDBR ≥ GTPR − GTDVU** | Có (modes 1, 3, 4 — crest transfer) | Có (modes 1, 3, 4 — crest transfer) | Không document riêng |
| **Trough transfer** | Không giới hạn | Không giới hạn | — |

**Lưu ý:** RA8T2 manual không có section riêng, nhưng register cấu trúc giống hệt RA6T2 → constraint này **vẫn nên tuân thủ**.

---

## 9. Buffer Transfer Timing (Chi Tiết)

### 9.1 GTCCRA Buffer Chain — Single Buffer (Mode 1, 2, 3)

Buffer chain cho duty cycle trong complementary PWM: `GTCCRD → Temp_A → GTCCRC → GTCCRA`. Cơ chế **giống nhau** trên cả 3 devices, chỉ khác tên channel prefix (GPT16n vs GPT32n).

#### Transfer Step 1: GTCCRD → Temporary Register A

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Trigger** | Viết GTCCRD của **GPT16n+2** (slave channel 2) | Viết GTCCRD của **GPT32n+2** (slave channel 2) | Viết GTCCRD của **GPT32n+2** (slave channel 2) |
| **Timing** | Sau 1 GTCLK cycle từ write | Sau 1 GTCLK cycle từ write | Sau 1 GTCLK cycle từ write |
| **Scope** | 3 channels đồng thời | 3 channels đồng thời | 3 channels đồng thời |
| **GTCCRF → Temp_B** | Cũng transfer đồng thời | Cũng transfer đồng thời | Cũng transfer đồng thời |

#### Transfer Step 2: Temporary Register A → GTCCRC

| Condition | Mode 1 | Mode 2 | Mode 3 (Single Buffer) |
|---|---|---|---|
| **(1) Write trong middle section** | Up-counting middle → 1 GTCLK sau transfer | Down-counting middle → 1 GTCLK sau transfer | Middle section → 1 GTCLK sau transfer |
| **(2) Write ngoài middle section** | Cuối trough section | Cuối crest section | Cuối crest AND trough sections |

**Giống nhau trên cả 3 devices** — chỉ khác channel prefix.

#### Transfer Step 3: GTCCRC → GTCCRA

| Mode | Transfer Timing | Counter Clear Timing |
|---|---|---|
| **Mode 1** | Cuối crest section | Up-counting middle + crest section |
| **Mode 2** | Cuối trough section (trừ initial output) | Down-counting middle + trough section |
| **Mode 3** | Cuối crest + cuối trough (trừ initial output) | Counter clear |

**Giống nhau trên cả 3 devices.**

### 9.2 GTCCRA Buffer Chain — Double Buffer (Mode 3 only)

Khi `GTBER2.CP3DB = 1`, thêm đường transfer thứ 2:

```
GTCCRF → Temp_B → GTCCRE → GTCCRA (tại trough)
GTCCRD → Temp_A → GTCCRC → GTCCRA (tại crest)
```

| Buffer Transfer | Transfer Timing |
|---|---|
| GTCCRD → Temp_A | 1 GTCLK sau write GPT(16/32)n+2.GTCCRD |
| GTCCRF → Temp_B | 1 GTCLK sau write GPT(16/32)n+2.GTCCRD (cùng lúc) |
| Temp_A → GTCCRC | Middle section: 1 GTCLK; khác: cuối crest+trough |
| Temp_B → GTCCRE | Middle section: 1 GTCLK; khác: cuối crest+trough |
| GTCCRC → GTCCRA | Cuối **crest** section |
| GTCCRE → GTCCRA | Cuối **trough** section (trừ initial output) |

**Double buffer delay (Mode 3):** Giá trị mới viết vào GTCCRD/GTCCRF cần tối thiểu **2 cycle** (crest+trough hoặc trough+crest) trước khi apply vào GTCCRA compare match. Delay này **giống nhau trên cả 3 devices**.

### 9.3 Complementary PWM Mode 4 — Immediate Transfer

Mode 4 thêm path transfer **ngay lập tức** từ GTCCRD/GTCCRF đến GTCCRA, bỏ qua crest/trough timing:

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Immediate transfer path** | GTCCRD → GTCCRC + GTCCRA (đồng thời) | GTCCRD → GTCCRC + GTCCRA (đồng thời) | GTCCRD → GTCCRC + GTCCRA (đồng thời) |
| **Double buffer (CP3DB=1)** | GTCCRF → GTCCRE + GTCCRA | GTCCRF → GTCCRE + GTCCRA | GTCCRF → GTCCRE + GTCCRA |
| **Guaranteed sections** | Up-counting middle + down-counting middle | Up-counting middle + down-counting middle | Up-counting middle + down-counting middle |
| **Prohibited values** | ≤ dead time value, ≥ count cycle | ≤ dead time value, ≥ count cycle | ≤ dead time value, ≥ count cycle |

**Cơ chế giống nhau trên cả 3 devices.**

### 9.4 GTPR Buffer Transfer Timing

| Buffer | Saw-wave | Triangle-wave (Mode 1/3) | Triangle-wave (Mode 2) |
|---|---|---|---|
| GTPDBR → GTPBR | Overflow/underflow/clear | Trough | Crest + Trough |
| GTPBR → GTPR | Overflow/underflow/clear | Trough | Crest + Trough |

**Lưu ý trong complementary PWM mode:** Buffer operation cho GTDVU register **bị cấm** — phải set trực tiếp.

---

## 10. CPSCD Bit — Complementary PWM Mode Synchronous Clear Disable

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **CPSCD bit** | **Không có** | **Không có** | **CÓ** (chỉ RA8T2) |
| **Bit position** | N/A | N/A | Bit 12 trong GTCR |
| **Available channels** | N/A | N/A | GPT324–GPT329 |

**CPSCD bit** trên RA8T2:
- `CPSCD = 0`: Enable synchronous counter clear từ channel khác (trừ trough section) trong complementary PWM mode.
- `CPSCD = 1`: Disable synchronous counter clear từ channel khác (trừ trough section).
- Slave channels cũng được control bởi CPSCD bit của master channel.

Tính năng này giúp RA8T2 linh hoạt hơn khi có nhiều channel groups cần hoạt động **độc lập** — tránh synchronous clear không mong muốn giữa 2 motor control groups.

---

## 11. CPSCIR Bit — Initial Output at Synchronous Clear Disable

| Thuộc Tính | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **CPSCIR bit** | Có | Có | Có |
| **Related section** | Section 20.2.14 | Section 21.10.10 | Section 22.10.8 |
| **Compare match range restriction** | Có | Có (section 21.10.10) | Có (section 22.10.8) |

Khi `CPSCIR = 1` và synchronous clear xảy ra, initial output của pins **không** bị reset. Cả 3 devices đều hỗ trợ, nhưng RA6T2 và RA8T2 có thêm section riêng mô tả restriction cho compare match register setting range khi CPSCIR=1.

---

## 12. Số Lượng Complementary PWM Channel Groups

| Device | Max Groups | Channels Khả Dụng | Motor Control Capability |
|---|---|---|---|
| **RA2T1** | **1 group** | CH0–CH2 | 1 motor (3-phase) |
| **RA6T2** | **2 groups** | CH4–CH6, CH7–CH9 | 2 motors (3-phase mỗi motor) |
| **RA8T2** | **2 groups** | CH4–CH6, CH7–CH9 | 2 motors (3-phase mỗi motor) |

---

## 13. Register Address Map (Chi Tiết)

### 13.1 Base Address

| Device | Formula | Ví dụ Master Channel |
|---|---|---|
| **RA2T1** | GPT16n = `0x4008_9000 + 0x0100 × n` (n=0–2); GPT163 = `0x4008_9300` | GPT160 (master) = `0x4008_9000` |
| **RA6T2** | GPT32n = `0x4016_9000 + 0x0100 × n` (n=0–9) | GPT324 (master) = `0x4016_9400` |
| **RA8T2** | GPT32n = `0x4032_2000 + 0x0100 × n` (n=0–13) | GPT324 (master) = `0x4032_2400` |

### 13.2 Register Offset Map (Complementary PWM Related)

Register offset **giống nhau** trên cả 3 devices. Chỉ base address và register width khác.

| Offset | Register | Chức năng trong Complementary PWM | Data Width |
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

*\*CPSCD bit tại GTCR offset 0x30 chỉ available trên RA8T2*

### 13.3 Additional Registers (RA6T2/RA8T2 only)

| Register | Offset | Chức năng | RA2T1 | RA6T2 | RA8T2 |
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

**Slave2.GTCCRD** là register cần viết **CUỐI CÙNG** để trigger buffer transfer đồng thời 3 channels.

---

## 14. GPT Register Count Comparison

| Metric | RA2T1 | RA6T2 | RA8T2 |
|---|---|---|---|
| **Total GPT registers** | 82 | 86 | 86 |
| **GTBER2** | ✓ | ✓ | ✓ |
| **GTCLKCR** | ✗ | ✓ | ✓ |
| **GTICLF** | ✗ | ✓ | ✓ |
| **CPSCD bit in GTCR** | ✗ | ✗ | ✓ |
| **Register offsets** | Giống | Giống | Giống |

---

## 15. Tổng Hợp Constraints Khi Porting

### Porting từ RA2T1 → RA6T2/RA8T2

| Hạng Mục | Thay Đổi Cần Thiết |
|---|---|
| **Channel mapping** | CH0/1/2 → CH4/5/6 (hoặc CH7/8/9) |
| **Register prefix** | `GPT16n` → `GPT32n` |
| **Register width** | 16-bit → 32-bit (tất cả GPT data registers) |
| **Base address** | `0x4008_9000` → `0x4016_9000` (RA6T2) / `0x4032_2000` (RA8T2) |
| **GTPR value** | Scale lại theo clock mới (có thể lớn hơn rất nhiều) |
| **GTDVU value** | Scale lại theo clock mới |
| **GTCCRA values** | Scale lại (duty cycle range lớn hơn) |
| **Clock source** | PCLKD only → có thể chọn GPTCLK (async), cần config GTCLKCR |
| **Prescaler** | Thêm options GTCLK/128, GTCLK/512 |
| **POEG** | 2 groups → 4 groups |
| **Module stop bit** | MSTPCRx bit number khác nhau |
| **Interrupt vector** | IELSRn.IELS[8:0] (RA2T1) vs IELS[8:0] (RA6T2) vs IELS[9:0] (RA8T2) |

### Porting từ RA6T2 → RA8T2

| Hạng Mục | Thay Đổi Cần Thiết |
|---|---|
| **Channel mapping** | Giống nhau (CH4–9) |
| **Register structure** | Gần như giống hệt |
| **CPSCD bit** | Chỉ RA8T2 có — thêm option synchronous clear disable |
| **Extra channels** | RA8T2 có thêm CH10–CH13 (nhưng không hỗ trợ complementary PWM) |
| **Base address** | `0x4016_9000` → `0x4032_2000` |
| **Interrupt vector** | IELS[8:0] → IELS[9:0] (wider) |
| **GTPBR/GTPDBR range note** | RA8T2 không document riêng (vẫn nên tuân thủ) |
| **Counter clear in comp. PWM** | RA8T2 không document riêng |

### Porting từ RA6T2 ↔ RA8T2 (Ít thay đổi nhất)

Hai devices này **gần như identical** về GPT complementary PWM behavior. Thay đổi chính: base address, CPSCD bit (RA8T2 only), và IELS width.

---

*Document generated from analysis of:*
- *RA2T1 User's Manual: r01uh1089ej0100 Rev.1.00 (Jun 25, 2025)*
- *RA6T2 User's Manual: r01uh0951ej0150 Rev.1.50 (Jul 2, 2025)*
- *RA8T2 User's Manual: r01uh1067ej0130 Rev.1.30 (Feb 27, 2026)*
