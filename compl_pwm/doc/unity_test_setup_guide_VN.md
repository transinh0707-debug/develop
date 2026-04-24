# Hướng dẫn Unity Test — Thiết kế test case và thiết lập môi trường trên e2studio

**Đối tượng:** Developer FSP cần viết unit test cho driver Renesas RA và chạy được toàn bộ test suite trên máy cá nhân qua e2studio.
**Framework:** Unity / Unity Fixture (ThrowTheSwitch)
**Tham khảo chính thức:** [Unity Assertions Reference — ThrowTheSwitch/Unity (github.com)](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md)
**Tài liệu liên quan trong repo:** `compl_pwm/doc/r_gpt_tests_tg2_pwm_guide_VN.md` (phân tích chi tiết file test group 2)

---

## Mục lục

- [Phần A — Kiến thức nền về Unity Framework](#phần-a--kiến-thức-nền-về-unity-framework)
  - [1. Unity là gì](#1-unity-là-gì)
  - [2. Cấu trúc của một test case](#2-cấu-trúc-của-một-test-case)
  - [3. Các assertion macro quan trọng](#3-các-assertion-macro-quan-trọng)
  - [4. Entry point `UnityMain()`](#4-entry-point-unitymain)
- [Phần B — Thiết lập môi trường trên e2studio](#phần-b--thiết-lập-môi-trường-trên-e2studio)
  - [Step 1 — Import project từ GitLab](#step-1--import-project-từ-gitlab)
  - [Step 2 — Convert thành C/C++ project](#step-2--convert-thành-cc-project)
  - [Step 3 — Update submodule](#step-3--update-submodule)
  - [Step 4 — Cấu hình compiler paths](#step-4--cấu-hình-compiler-paths)
  - [Step 5 — Chỉnh settings của e2studio](#step-5--chỉnh-settings-của-e2studio)
- [Phần C — Checklist trước khi chạy test lần đầu](#phần-c--checklist-trước-khi-chạy-test-lần-đầu)
- [Phần D — Troubleshooting thường gặp](#phần-d--troubleshooting-thường-gặp)

---

# Phần A — Kiến thức nền về Unity Framework

## 1. Unity là gì

Unity (do ThrowTheSwitch phát triển) là unit-testing framework **viết bằng C thuần, dành cho C** — đặc biệt phù hợp với embedded, vì:

- Không đụng tới C++ hay chuẩn POSIX.
- Chỉ cần 2-3 file `.c`/`.h` để build vào firmware.
- Output là plain text, có thể route qua `printf`, UART, hay SEGGER RTT.
- Có phần mở rộng **Unity Fixture** cho mô hình test group + setup/teardown (FSP dùng phần này).

### Các macro cốt lõi — chỉ 6 hàm đủ để viết 90% test case

| Macro | Ý nghĩa |
|---|---|
| `TEST_ASSERT_TRUE(condition)` | Đánh giá biểu thức `condition`, **fail nếu nó là `false`** |
| `TEST_ASSERT_FALSE(condition)` | Đánh giá biểu thức `condition`, **fail nếu nó là `true`** |
| `TEST_ASSERT(condition)` | Cách gọi khác của `TEST_ASSERT_TRUE` |
| `TEST_ASSERT_UNLESS(condition)` | Cách gọi khác của `TEST_ASSERT_FALSE` |
| `TEST_FAIL()` / `TEST_FAIL_MESSAGE(message)` | Đánh dấu test **fail ngay lập tức**, kèm lý do |
| `TEST_ASSERT_EQUAL_INT(expected, actual)` | So sánh 2 số nguyên, nếu không bằng thì fail và in ra dưới dạng signed int |

**Nhớ kỹ điều này:** khi một `TEST_ASSERT_*` fail, Unity dùng `longjmp` để nhảy thoát khỏi hàm test luôn. **Các dòng code phía sau sẽ KHÔNG chạy nữa**. Đây là lý do phải đặt assertion quan trọng nhất lên trước — assertion yếu hơn ở sau chỉ chạy khi cái mạnh hơn đã pass.

Ngoài 6 macro cơ bản trên, Unity còn hàng chục macro tiện ích: `TEST_ASSERT_EQUAL_UINT32`, `TEST_ASSERT_UINT_WITHIN`, `TEST_ASSERT_GREATER_THAN`, `TEST_ASSERT_NULL`, `TEST_ASSERT_NOT_NULL`, `TEST_ASSERT_EQUAL_STRING`, `TEST_ASSERT_EQUAL_MEMORY`, v.v. Xem đầy đủ ở [UnityAssertionsReference.md](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md).

---

## 2. Cấu trúc của một test case

Một test case Unity điển hình (theo phong cách FSP của Renesas) có **3 phần rõ ràng**: test inputs → gọi API → expected outputs.

### Ví dụ minh họa — `TEST(R_SCI_Open_Test, TG001_001)`

```c
TEST(R_SCI_Open_Test, TG001_001)
{
    // ========== 1. TEST INPUTS (chuẩn bị dữ liệu đầu vào) ==========
    sci_err_t    error = SCI_SUCCESS;
    uint8_t      chan;
    sci_mode_t   mode;
    sci_cfg_t  * p_cfg = &g_config;
    void      (* p_callback)(void * p_args);
    sci_hdl_t  * p_hdl = &g_sci_handle;
    chan         = 100;                   // kênh không hợp lệ → kỳ vọng lỗi
    p_callback   = test_sci_callback;

    // ========== 2. GỌI HÀM API CẦN TEST ==========
    error = R_SCI_Open(chan, mode, p_cfg, p_callback, p_hdl);

    // ========== 3. EXPECTED OUTPUTS (so sánh kết quả) ==========
    TEST_ASSERT_EQUAL(SCI_MODE_OFF,     g_sci_handle->mode);
    TEST_ASSERT_EQUAL(SCI_ERR_BAD_CHAN, error);   // ← Unity assertion
}
```

### Giải thích từng phần

**(1) Test Inputs:**
- Khai báo biến local và gán giá trị đầu vào cho API.
- Biến pointer (`p_cfg`, `p_hdl`) trỏ vào global đã được khởi tạo sẵn ở file.
- Ở ví dụ trên, `chan = 100` là kênh không tồn tại — dùng để kích hoạt đường fail của API.

**(2) Gọi API:**
- Chỉ một lần gọi duy nhất, đây là **subject-under-test** — đối tượng cần verify.
- Không có logic phức tạp ở đây; nếu cần nhiều bước, nên tách helper function.

**(3) Expected Outputs:**
- Các macro `TEST_ASSERT_EQUAL(expected, actual)` kiểm tra tất cả điều mà API được kỳ vọng phải thay đổi:
  - Trả về error code đúng (`SCI_ERR_BAD_CHAN`).
  - Cập nhật struct state đúng (`g_sci_handle->mode == SCI_MODE_OFF`).
- Thứ tự quan trọng: thường kiểm tra **return value trước**, state **sau**. Nếu return value sai, state không có ý nghĩa.

### Nguyên tắc thiết kế test case tốt

1. **Mỗi test case verify một hành vi cụ thể** — đặt tên mô tả rõ (`TC_Open_InvalidChannel_ReturnsBadChan`), không phải `TC_Open_Test1`.
2. **Không phụ thuộc thứ tự** — test case thứ 5 không được dựa vào side effect của test case thứ 4. Setup/teardown đảm bảo điều này.
3. **Có cả đường happy path và đường fail** — cùng 1 API thường cần ≥2 test case: một cho input hợp lệ, một cho input biên/không hợp lệ.
4. **Assertion đầu tiên là assertion quan trọng nhất** — vì Unity dừng test ở assertion đầu tiên fail, nên đặt cái quan trọng lên đầu.
5. **Dùng macro phù hợp** — đo thời gian / đọc waveform thì dùng `TEST_ASSERT_UINT_WITHIN(tolerance, expected, actual)` thay vì `TEST_ASSERT_EQUAL`, vì sẽ luôn có jitter.

---

## 3. Các assertion macro quan trọng

Dưới đây là bảng tham khảo nhanh, sắp xếp theo mục đích sử dụng:

### 3.1. Assertion cơ bản (boolean)

| Macro | Dùng khi |
|---|---|
| `TEST_ASSERT_TRUE(cond)` | Cần verify một điều kiện đúng |
| `TEST_ASSERT_FALSE(cond)` | Cần verify một điều kiện sai |
| `TEST_ASSERT_TRUE_MESSAGE(cond, "msg")` | Như `TRUE` nhưng kèm lý do khi fail |
| `TEST_FAIL_MESSAGE("msg")` | Force fail — dùng trong nhánh `else` không bao giờ nên đến |

### 3.2. So sánh giá trị

| Macro | Dùng khi |
|---|---|
| `TEST_ASSERT_EQUAL(expected, actual)` | So sánh 2 giá trị (auto-detect kiểu) |
| `TEST_ASSERT_EQUAL_INT(expected, actual)` | So sánh signed int, in output dưới dạng số có dấu |
| `TEST_ASSERT_EQUAL_UINT32(expected, actual)` | So sánh unsigned 32-bit, in dạng unsigned |
| `TEST_ASSERT_NOT_EQUAL(expected, actual)` | Verify 2 giá trị khác nhau |

### 3.3. So sánh với sai số (quan trọng cho test đo đạc)

| Macro | Dùng khi |
|---|---|
| `TEST_ASSERT_UINT_WITHIN(delta, expected, actual)` | Fail nếu `|expected - actual| > delta` — dùng cho đo PWM, timing |
| `TEST_ASSERT_GREATER_THAN_UINT32(threshold, actual)` | `actual > threshold`, không bằng |
| `TEST_ASSERT_LESS_THAN_UINT32(threshold, actual)` | `actual < threshold` |
| `TEST_ASSERT_GREATER_OR_EQUAL_UINT32(threshold, actual)` | `actual >= threshold` |

### 3.4. Pointer và NULL

| Macro | Dùng khi |
|---|---|
| `TEST_ASSERT_NULL(pointer)` | Verify pointer == NULL |
| `TEST_ASSERT_NOT_NULL(pointer)` | Verify pointer != NULL |

### 3.5. Điều khiển luồng test

| Macro | Dùng khi |
|---|---|
| `TEST_IGNORE_MESSAGE("reason")` | Skip test và in lý do — dùng khi MCU không support feature |
| `TEST_FAIL()` | Fail ngay, không kèm lý do |
| `TEST_FAIL_MESSAGE("reason")` | Fail ngay, có lý do |

**Lưu ý:** `TEST_IGNORE_MESSAGE` thường dùng trong `TEST_SETUP` khi cần check capability của MCU. Ví dụ trong `r_gpt_tests_tg2_pwm.c`:

```c
TEST_SETUP(R_GPT_TG2)
{
#if 2U != GPT_CFG_OUTPUT_SUPPORT_ENABLE
    TEST_IGNORE_MESSAGE("This test is only run if triangle-wave PWM output is enabled.");
#endif
    // ...
}
```

Tất cả test trong group sẽ được skip nếu feature không enable — không fail, chỉ báo skipped.

---

## 4. Entry point `UnityMain()`

Để chạy được toàn bộ test suite, firmware phải gọi `UnityMain()` từ `main()` hoặc `hal_entry()`:

```c
int UnityMain (int argc, char argv[], void (*runAllTests)())
{
    int result = UnityGetCommandLineOptions(argc, argv);
    unsigned int r;
    if (result != 0)
        return result;
    for (r = 0; r < UnityFixture.RepeatCount; r++)
    {
        announceTestRun(r);
        UnityBegin();
        runAllTests();                  // ← hàm do user cung cấp, chạy mọi test
        UNITY_OUTPUT_CHAR('\n');
        UnityEnd();
    }
    return UnityFailureCount();
}
```

### Các argument của `UnityMain`

| Tham số | Ý nghĩa |
|---|---|
| `argc` | Số option được pass vào qua `argv[]` |
| `argv[]` | Mảng option — các flag điều khiển cách Unity chạy |
| `runAllTests` | **Function pointer** trỏ tới hàm gọi tất cả test group (trong FSP đó là `RunAllR_GPTTests`) |

### Các option của `argv[]`

| Option | Tác dụng |
|---|---|
| `-v` | **Verbose output** — in mọi test đã chạy, kể cả pass |
| `-s` | **Silent mode** — chỉ in test failure, ẩn pass |
| `-g NAME` | Chỉ chạy test group có tên chứa chuỗi `NAME` |
| `-n NAME` | Chỉ chạy test case có tên chứa chuỗi `NAME` |
| `-r NUMBER` | Chạy lặp lại tất cả test `NUMBER` lần (stress test) |
| `-h` / `--help` | In ra danh sách option |

### Ví dụ hoàn chỉnh cho `main()`

```c
void main()
{
    // Chạy tất cả test một lần, output tối giản
    UnityMain(0, 0, RunAllTests);

    while (1)
    {
        /* Infinite loop — firmware idle sau khi test xong */
    }
}
```

### Ví dụ verbose mode

```c
const char * argv[] = {"unity", "-v"};
UnityMain(sizeof(argv)/sizeof(argv[0]), (const char **)argv, RunAllR_GPTTests);
```

Đây chính là pattern mà `hal_entry.c` trong `compl_pwm/` đang dùng. Verbose mode hữu ích lúc đang phát triển để thấy rõ từng test chạy thế nào; production có thể để không option để output gọn hơn.

---

# Phần B — Thiết lập môi trường trên e2studio

**Mục tiêu:** Từ máy PC trắng, có thể clone source repo, build và chạy được Unity test suite.

**Prerequisites (cài trước):**
- e2studio (bản mới nhất của Renesas, có tích hợp FSP).
- GCC ARM Embedded Toolchain (phiên bản 10.3.1 hoặc phù hợp với FSP).
- LLVM (cho công cụ `clang-tidy` static analysis).
- Python 3.9+ với gói `scons` cài qua `pip install scons`.
- Git.
- SEGGER J-Link Software (để flash và đọc RTT log).

## Step 1 — Import project từ GitLab

Đây là bước kéo source code về máy qua e2studio (không dùng git command line).

### 1.1. Mở menu Import

Trong e2studio, vào **File → Import…**

Trong cửa sổ Import, chọn **Git → Projects from Git (with smart import)**, bấm **Next**.

### 1.2. Chọn nguồn Git

Màn hình **Select Repository Source** hiện ra → chọn **Clone URI**, bấm **Next**.

### 1.3. Nhập thông tin repository

Điền vào form **Source Git Repository**:

| Field | Giá trị |
|---|---|
| **URI** | `http://gitlab.rvc.renesas.com/RSS2/ra-fsp.git` (hoặc URL của repo team bạn) |
| **Host** | Tự fill từ URI |
| **Repository path** | Tự fill từ URI |
| **Protocol** | `http` |
| **User** | Username GitLab cá nhân |
| **Password** | Password hoặc Personal Access Token |

Check **KHÔNG** vào "Store in Secure Store" trừ khi bạn muốn e2studio lưu password — an toàn hơn là điền lại mỗi lần clone.

Bấm **Next**.

### 1.4. Chọn branch

Màn hình **Branch Selection** → chọn branch **master** (hoặc branch đang làm việc, ví dụ `develop`).

Ở phần "Tag fetching strategy", chọn **Don't fetch any tags** để tiết kiệm thời gian clone (tags có thể fetch sau nếu cần).

Bấm **Next**.

### 1.5. Đợi clone xong

Progress bar hiện tiến độ clone. Với repo FSP đầy đủ có thể mất vài phút.

Sau khi clone xong, sẽ hiện màn hình **Select a wizard to use for importing projects**.

### 1.6. Chọn wizard import

- Nếu thư mục đã clone **có sẵn file `.project`** của Eclipse → chọn **Import existing Eclipse projects** → Next.
- Nếu **chưa có** `.project` → chọn **Import as general project** (mặc định).

### 1.7. Đặt tên project

Màn hình **Import Projects**:
- **Project name:** đặt tên project, ví dụ `ra_fsp` hoặc `compl_pwm`.
- **Directory:** tự fill theo đường dẫn đã clone.

Bấm **Finish**. Project xuất hiện trong Project Explorer.

---

## Step 2 — Convert thành C/C++ project

Nếu ở Step 1 đã import dưới dạng "general project", nó chưa biết là C project — cần convert.

### 2.1. Mở wizard

**File → New → C/C++ Project → "Create a new C or C++ project"**

### 2.2. Cấu hình convert

Trong cửa sổ **Convert to a C/C++ Project**:

- **Candidates for conversion:** tick vào project `ra-fsp` (hoặc tên project bạn đã đặt).
- **Convert to C or C++:** chọn **C Project**.
- **Project options:**
  - **Project type:** chọn **Executable → Shared Library** (hoặc theo template phù hợp với repo bạn).
  - **Toolchains:** chọn **GCC ARM Embedded** — đây là bước quan trọng, nếu chọn sai toolchain thì build sẽ không ra firmware cho Renesas RA.
- Tick **"Show project types and toolchains only if they are supported on the platform"** để lọc bớt noise.

Bấm **Finish**. Project giờ có C nature, các file `.c`/`.h` được hiện đúng icon.

---

## Step 3 — Update submodule

Repo FSP dùng Git submodule để quản lý các thư viện phụ thuộc như CMSIS và Unity (ThrowTheSwitch). Mặc định, `git clone` **không** pull submodule — cần gọi lệnh riêng.

### 3.1. Mở command prompt

Mở Windows Command Prompt hoặc Git Bash, chuyển thư mục đến **root của repo** (folder chứa file `fsp_test_parallel.py`, `ac6.version`, folder `ra/`, `test_files/`, v.v.).

```cmd
cd C:\Users\<username>\git\ra-fsp
```

### 3.2. Chạy 2 lệnh submodule update

```bash
git submodule update --init --recursive ./ra/arm/CMSIS_5/
git submodule update --init --recursive ./ra/ThrowTheSwitch/
```

**Giải thích các flag:**
- `--init` — khởi tạo các submodule chưa được init.
- `--recursive` — đệ quy vào các submodule lồng nhau (một số submodule có submodule con).
- `./ra/arm/CMSIS_5/` — submodule chứa CMSIS headers (các register definition).
- `./ra/ThrowTheSwitch/` — submodule chứa **Unity và Unity Fixture** framework.

### 3.3. Verify

Sau khi 2 lệnh trên chạy xong, trong Project Explorer của e2studio bạn sẽ thấy folder `ra/ThrowTheSwitch/` có file bên trong (trước đó nó trống hoặc chỉ có `.gitmodules`).

**Nếu bỏ qua bước này:** build sẽ fail với lỗi `unity.h: No such file or directory`.

---

## Step 4 — Cấu hình compiler paths

Unity test harness của FSP dùng một file XML để biết compiler nằm ở đâu. Cần sửa file này cho phù hợp với máy mình.

### 4.1. Mở file config

Mở file sau bằng text editor (Notepad++, VS Code, hoặc chính e2studio):

```
ra-fsp/test_files/shared/tools_cfg_ref.xml
```

### 4.2. Nội dung file

Bên trong sẽ có các tag dạng XML:

```xml
<tools_cfg>
    <gcc_install>C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10</gcc_install>
    <iar_install>/opt/iarsystems/bxarm-9.30.1/arm</iar_install>
    <ac6_install>/opt/arm_compiler_6.18/</ac6_install>
    <ac6_license>8224@RTPFLEXBUILD0.REA.RENESAS.COM</ac6_license>
    <!-- jlink_server is used by run_tests.py -->
    <jlink_server>C:\Program Files (x86)\SEGGER\JLink</jlink_server>
    <clang_path>C:\Program Files\LLVM\bin</clang_path>
</tools_cfg>
```

### 4.3. Chỉnh đường dẫn

Cần sửa 2 tag cho khớp với máy mình:

| Tag | Nội dung | Cách lấy đường dẫn đúng |
|---|---|---|
| `<gcc_install>` | Thư mục cài GCC ARM Toolchain | Mở Windows Explorer, tìm nơi bạn cài toolchain (thường là `C:\Program Files (x86)\GNU Arm Embedded Toolchain\<version>`) |
| `<clang_path>` | Thư mục `bin` của LLVM | Tương tự, tìm nơi cài LLVM (thường `C:\Program Files\LLVM\bin`) |

**Lưu ý path trên Windows:**
- Dùng dấu `\` backslash (chuẩn XML không bắt buộc escape `\` trong nội dung tag).
- KHÔNG để dấu `\` cuối đường dẫn (ví dụ `...\10 2021.10` chứ không phải `...\10 2021.10\`).

Các tag khác (`<iar_install>`, `<ac6_install>`, `<ac6_license>`, `<jlink_server>`) có thể để nguyên nếu bạn không dùng IAR/ARMCC compiler hoặc J-Link server riêng.

### 4.4. Save file

Lưu file. Không đổi tên, không đổi structure — chỉ sửa giá trị bên trong tag.

---

## Step 5 — Chỉnh settings của e2studio

Đây là bước cuối — cấu hình cho e2studio biết dùng `scons` làm build system thay cho `make`, và biết tìm `scons.exe` / `clang-tidy.exe` ở đâu.

### 5.1. Cấu hình Builder — dùng scons

**Chuột phải project `ra-fsp` → Properties → C/C++ Build**

Trong tab **Builder Settings**:

| Field | Giá trị |
|---|---|
| **Builder type** | `External builder` |
| **Use default build command** | **Bỏ tick** (uncheck) |
| **Build command** | `scons` |
| **Generate Makefiles automatically** | Bỏ tick |
| **Build directory** | `${workspace_loc:/ra-fsp}` (đường dẫn root project) |

**Giải thích vì sao dùng scons:** `scons` là build tool viết bằng Python, FSP test framework dùng nó để tự động phát hiện file thay đổi và build selective. Nhanh hơn nhiều so với rebuild toàn bộ.

Chưa bấm Apply — còn bước nữa.

### 5.2. Cấu hình Build arguments

Vẫn trong **C/C++ Build**, chuyển sang tab **Behavior**:

- **Use custom build arguments:** chọn.
- **Build arguments:** `-k`
  - Flag `-k` (keep going) bảo scons tiếp tục khi gặp lỗi, không dừng ngay. Hữu ích vì lỗi trong một file không làm cả build report biến mất.
- **Workbench build type:**
  - **Build (Incremental build):** điền:
    ```
    --build=wdt --compiler=gcc --board=ra2a2_tool
    ```
  - **Clean:** điền:
    ```
    --clean --build=wdt --compiler=gcc --board=ra2a2_tool
    ```

**Giải thích các flag:**
- `--build=wdt` — tên build target (watchdog timer test, thay bằng target của project bạn).
- `--compiler=gcc` — chọn compiler GCC (có thể thay bằng `iar`, `ac6` nếu dùng compiler khác).
- `--board=ra2a2_tool` — tên board target (thay tương ứng với board bạn dùng, ví dụ `ra2t1_fpb`).

**Tuỳ chọn static analysis:**
Nếu muốn chạy `clang-tidy` để check static analysis, thêm:
```
--static_analyze=export
```
vào sau chuỗi build argument. Scons sẽ chạy clang-tidy trên mỗi file `.c` và xuất báo cáo.

### 5.3. Verify PATH environment

Vẫn trong **Properties → C/C++ Build**, click **Environment** ở sidebar trái.

Màn hình **Environment variables to set** hiện ra với các biến:

| Variable | Value (ví dụ) | Origin |
|---|---|---|
| `CWD` | `C:\Users\tamdinh\git\ra-fsp` | BUILD SYSTEM |
| `GCC_VERSION` | `10.3.1` | BUILD SYSTEM |
| **`PATH`** | `C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bi...` | **USER: CONFIG** |
| `PWD` | `C:\Users\tamdinh\git\ra-fsp` | BUILD SYSTEM |
| `TCINSTALL` | `C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\` | BUILD SYSTEM |
| `TC_VERSION` | `10.3.1.20210824` | BUILD SYSTEM |

Double-click vào dòng **PATH** để edit, và **verify** rằng 2 đường dẫn sau nằm trong biến `PATH`:

1. **Đường dẫn `scons.exe`** — thường tại:
   ```
   C:\Users\<username>\AppData\Roaming\Python\Python39\Scripts
   ```
   (Đây là chỗ `pip install scons` cài `scons.exe` trên Windows với Python cá nhân.)

2. **Đường dẫn `clang-tidy.exe`** — thường tại:
   ```
   C:\Program Files\LLVM\bin
   ```

Nếu chưa có, thêm vào bằng cách nối ngăn cách bằng dấu `;`:
```
C:\Users\<username>\AppData\Roaming\Python\Python39\Scripts;C:\Program Files\LLVM\bin
```

Chọn **"Append variables to native environment"** (mặc định) thay vì Replace — để giữ các biến PATH hệ thống gốc.

### 5.4. Apply và Close

Bấm **Apply** → **Apply and Close**.

E2studio sẽ rebuild index cho project. Sau khi xong, bạn có thể bấm **Project → Build All** (Ctrl+B) để build lần đầu. Nếu thành công, output trong Console sẽ hiện các dòng `scons: Reading SConscript files...` và kết thúc bằng `scons: done building targets.`

---

# Phần C — Checklist trước khi chạy test lần đầu

Tick lần lượt từng mục trước khi bấm "Build":

- [ ] Đã cài đầy đủ: e2studio, GCC ARM Toolchain, LLVM, Python 3.9+, SEGGER J-Link.
- [ ] Đã `pip install scons` và biết đường dẫn `scons.exe` nằm ở đâu.
- [ ] Đã clone repo qua **Step 1** và chọn đúng branch.
- [ ] Đã convert sang C project qua **Step 2** với toolchain **GCC ARM Embedded**.
- [ ] Đã chạy `git submodule update --init --recursive` cho cả `CMSIS_5/` và `ThrowTheSwitch/` — verify bằng cách xem folder `ra/ThrowTheSwitch/unity/` có file bên trong.
- [ ] Đã sửa `test_files/shared/tools_cfg_ref.xml`, tag `<gcc_install>` và `<clang_path>` khớp với máy mình.
- [ ] Đã set **Build command = `scons`** và **Build directory = `${workspace_loc:/ra-fsp}`** trong Properties.
- [ ] Đã set **Build arguments = `-k`** và các flag `--build`, `--compiler`, `--board` đúng.
- [ ] Đã verify PATH chứa đường dẫn `scons.exe` và `clang-tidy.exe`.
- [ ] Đã flash J-Link driver, board kết nối nhận được trong Device Manager.

---

# Phần D — Troubleshooting thường gặp

### Lỗi: `scons: command not found` hoặc `'scons' is not recognized as internal or external command`

**Nguyên nhân:** `scons.exe` không nằm trong biến PATH của e2studio (có thể nó nằm trong PATH hệ thống, nhưng e2studio dùng PATH riêng trong Properties).

**Fix:**
1. Mở Command Prompt riêng, chạy `where scons` để xem đường dẫn thật của `scons.exe`.
2. Vào Properties → C/C++ Build → Environment → edit PATH, thêm đường dẫn đó vào.
3. Apply, clean build, rebuild.

### Lỗi: `unity.h: No such file or directory`

**Nguyên nhân:** Chưa chạy `git submodule update` cho `ThrowTheSwitch/`.

**Fix:** Quay lại **Step 3**, chạy lệnh submodule update.

### Lỗi: `undefined reference to 'UnityMain'`

**Nguyên nhân:** File `unity.c` và `unity_fixture.c` không được thêm vào build.

**Fix:**
1. Mở `SConstruct` hoặc `SConscript` của project.
2. Kiểm tra phần glob source file có bao gồm `ra/ThrowTheSwitch/unity/src/*.c` và `unity_fixture/src/*.c` không.
3. Nếu chưa có, thêm vào pattern source.

### Build thành công nhưng test không in gì ra RTT

**Nguyên nhân:** `UNITY_OUTPUT_CHAR` chưa được hook vào hàm `SEGGER_RTT_PutChar`.

**Fix:**
1. Mở `test_cfg.h` hoặc `unity_config.h`.
2. Tìm dòng `#define UNITY_OUTPUT_CHAR(a) ...`.
3. Đảm bảo nó gọi `SEGGER_RTT_PutChar(0, a)` hoặc macro tương đương.

### Lỗi: `clang-tidy: error: unable to handle compilation...`

**Nguyên nhân:** clang-tidy không tìm thấy include path của GCC. Chạy `--static_analyze=export` với compile_commands.json sai.

**Fix:**
1. Build trước với flag `--static_analyze=export` (không chạy tidy, chỉ export).
2. Verify file `compile_commands.json` được sinh ra ở build directory.
3. Rerun với `--static_analyze=run`.

### Test chạy nhưng tất cả bị IGNORED, không có test nào pass/fail

**Nguyên nhân:** `TEST_SETUP` đang gọi `TEST_IGNORE_MESSAGE` do feature không enable.

**Fix:** Kiểm tra `#if GPT_CFG_OUTPUT_SUPPORT_ENABLE` hoặc tương đương trong `TEST_SETUP` — đảm bảo macro được define đúng giá trị (thường là 2 cho "Enabled with Extra Features") trong `fsp_cfg/r_gpt_cfg.h`.

---

# Tham khảo

| Resource | URL |
|---|---|
| Unity GitHub | https://github.com/ThrowTheSwitch/Unity |
| Unity Assertions Reference | https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md |
| Unity Fixture Usage | https://github.com/ThrowTheSwitch/Unity/blob/master/extras/fixture/readme.txt |
| Renesas FSP docs | https://renesas.github.io/fsp/ |
| Hướng dẫn phân tích file test TG2 | `compl_pwm/doc/r_gpt_tests_tg2_pwm_guide_VN.md` trong repo |
