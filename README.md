# CVBS → HDMI 转换器（AMT630A + SII9022A + PY32F003）

把模拟 CVBS 图传 / 摄像头信号转换成 HDMI。AMT630A 完成 CVBS 解码、去隔行和缩放，输出 24-bit RGB888 + HS/VS/DE；PY32F003 通过 I2C 配置 SII9022A，把 RGB888 编码成 HDMI TMDS。低信号或无信号时尽量保持“雪花”，而不是蓝屏/黑屏。

## 特性

- CVBS 输入，支持 PAL / NTSC / PAL60 自动识别
- AMT630A 输出 800×480 RGB888，像素时钟 30.75MHz
- SII9022A 输出 HDMI，带 AVI InfoFrame（VIC 0，非 CEA 800×480 模式）
- PY32F003 自动读取 SII9022A 的 H_RES/V_RES，跟随 AMT630A 的 60Hz/50Hz 输出时序
- 无信号时输出雪花：关闭 AMT630A 硬件自动蓝屏，打开雪花发生器
- 按键切换 60Hz / 50Hz 和 DVI / HDMI 输出模式，设置保存在 Flash
- 上电初始化约 300ms（原厂示例的 10 秒等待已去掉）
- HDMI 热插拔后自动重发 AVI、重新打开 TMDS

## 硬件链路

```text
CVBS ──► AMT630A ──► RGB888 + HS/VS/DE ──► SII9022A ──► HDMI
          │                                  ▲
          └──────── 8051 内核/屏参            │ I2C (PA13/PA14)
                                               │
                                          PY32F003
```

- AMT630A：CVBS 解码 + Scaler + TCON，7 寸 800×480 屏参
- SII9022A：RGB888 → HDMI，I2C 地址 0x39（8 位写地址 0x72）
- PY32F003：SII9022A 初始化、时序跟随、按键、配置存储
- 按键与 I2C SDA 复用 PA13；PA14 为 SCL

## 目录结构

```text
CVBS2HDMI/
├─ amt630a/        AMT630A（8051）工程、屏参、ARK 视频库
├─ py32/           PY32F003 + SII9022A 工程（Keil MDK / EIDE）
├─ board/          原理图、PCB、渲染图、测试照片
└─ README.md
```

### amt630a/

- `Keil Prj/64K/Prj/ARK_SDK_Ver2.0.uvproj`：Keil C51 工程
- `LCD Pannels/LCD parameter/P_MEX070TM_50D_DHV_AMT630A_20141113.c`：7 寸 800×480 屏参
- `LCD Pannels/ConfigLcdPara.c`：屏参写入，以及蓝屏/雪花修正
- `Device driver/AMT/AMT_Drv.c`：`SetNoSignalSnow()` 雪花函数
- `UserConfig/Prj config/Prj_AMT_7D_SDK_DEMO.h`：项目宏（`NoSignalSnowEn`、`BacklightEn` 等）
- `ARKLib/`：ARK 视频库、IO 库、SPI 库
- `PC Tool/`：Keil 编译后处理需要的 `HexMerge.exe`、`18X9_Function_Location.exe`

### py32/

- `Src/main.c`：初始化、按键、HPD/时序轮询、Flash 配置
- `Src/sil9022.c`、`Inc/sil9022.h`：SII9022A TPI 驱动（已加 800×480 模式和 AVI）
- `Src/my_i2c.c`、`Inc/my_i2c.h`：软件 I2C（约 100kHz，带总线恢复）
- `MDK-ARM/Project.uvprojx`：Keil MDK 工程
- `EIDE/`：VS Code + EIDE + arm-none-eabi-gcc 的备用构建
- `Drivers/`：PY32F0xx HAL、CMSIS、Start Kit 支持文件

## 关键实现

### 1. AMT630A 实际输出时序

7 寸 800×480 屏参的显示 PLL 为：

```text
27MHz ÷ 9 × 164 ÷ 4 = 123MHz
显示时钟再 ÷4      = 30.75MHz (DCLK)
```

AMT630A 会根据输入制式切换 Scaler 的 H/V 总点数：

| 制式 | H total | V total | 刷新率 |
|---|---:|---:|---:|
| NTSC / PAL60 | 929 | 552 | 59.95Hz |
| PAL | 1129 | 544 | 50.07Hz |

SII9022A 的 TPI `0x00~0x07` 必须按这组值配置，不能用 480i/27MHz 的标准 CEA 时序。代码中对应两个专用模式：

- `HDMI_800_480_60`：30.75MHz，929×552，VFreq 5996
- `HDMI_800_480_50`：30.75MHz，1129×544，VFreq 5007

`siHdmiTx_UpdateTimingFromInput()` 会读 SII9022A 的 `H_RES/V_RES`（`0x6A~0x6D`）自动跟随制式切换。

### 2. 无信号雪花

原屏参 `0xFFB0 = 0x27` 中：

- bit5 `BLUE_SCREEN_EN = 1`（蓝屏开）
- bit7 `snowflower_en = 0`（雪花关）

ARK 视频库 `InitAMT()` 还会写 `0xFFD8 = 0x80`（输入不稳定时硬件自动蓝屏）。只打开 `NoSignalSnowEn`、只置雪花位是不够的，蓝屏会盖住雪花。

`ConfigLcdPara.c` 在屏参写完后强制：

```c
XBYTE[0xFFD8] = 0x00;      /* 关硬件自动蓝屏 */
XBYTE[0xFFB0] &= ~_BIT5;   /* 关蓝屏 */
XBYTE[0xFFB0] |= _BIT7;    /* 开雪花 */
XBYTE[0xFFDA] = 0x6C;      /* 雪花大小/源 */
```

`SetNoSignalSnow()` 里也补了 `VP_EN_REG &= ~_BIT5`，保证无信号时雪花优先级高于蓝屏。

### 3. HDMI 兼容性

原代码只配了 480i/27MHz，而且没有发 AVI InfoFrame，所以只有部分显示器能自己按实际输入同步锁上。现在：

- 使用实际 800×480 时序
- `siHdmiTx_Init()` 中补发 `SetAVI_InfoFrames()`
- 打开原厂 Bug#18128 的 8bit 初始化 workaround
- 修复 PC 模式 VIC 查表越界
- HPD 轮询，插拔后重发 AVI/TMDS

## 构建

### AMT630A（Keil C51）

1. 用 Keil uVision（C51）打开：

   ```text
   amt630a/Keil Prj/64K/Prj/ARK_SDK_Ver2.0.uvproj
   ```

2. 选择目标 `ARK-SDK`，Build（F7）。
3. 生成：

   ```text
   amt630a/Keil Prj/64K/Hex/AMT_CAR_HEX.hex
   amt630a/Keil Prj/Bin/AMT_CAR_BIN.bin
   ```

`AfterMake` 会调用 `PC Tool/HexMerge.exe` 把主程序 hex 和 `OSD/Logo/Hex/Logo.hex` 合并成烧录 bin。如果后处理报错，可在 `amt630a/Keil Prj/64K/Prj` 目录手动执行：

```bat
..\..\..\PC Tool\18X9_Function_Location "\\List"
..\..\..\PC Tool\HexMerge ..\..\Bin\AMT_CAR_BIN.bin ..\Hex\AMT_CAR_HEX.hex ..\..\..\OSD\Logo\Hex\Logo.hex
```

> AMT630A 源码是 **GBK/ANSI + CRLF**，请不要批量转成 UTF-8，否则 Keil C51 里的中文注释和字符可能乱码。

### PY32F003（Keil MDK）

1. 用 Keil MDK-ARM 5 打开：

   ```text
   py32/MDK-ARM/Project.uvprojx
   ```

2. 确认器件为 `PY32F003x8`，Build（F7），下载。
3. 工程已包含 `py32/Drivers`，不需要再装 PY32 SDK 到原来的六层目录。

### PY32F003（EIDE / GCC，可选）

1. VS Code 安装 EIDE 扩展、ARM GCC、PY32F0xx DFP。
2. 打开：

   ```text
   py32/EIDE/Project.code-workspace
   ```

3. 选择 `Project` 目标，Build。

## 按键

按键接在 PA13（与 I2C SDA 复用），上电初始化完成后生效：

| 操作 | 功能 |
|---|---|
| 短按（≥50ms，<2s） | 时序模式循环：自动 → 强制 60Hz → 强制 50Hz → 自动 |
| 长按（≥2s） | 输出模式切换：HDMI ↔ DVI |

设置写入 Flash `0x0800FC00`（64KB 型号最后 1KB）。首次上电/空白 Flash 会自动写入默认值：HDMI + 自动时序。

## 测试照片

| 有信号 | 无信号雪花 |
|---|---|
| ![有信号](<board/pictures/有信号测试.jpg>) | ![无信号雪花](<board/pictures/无信号雪花测试.jpg>) |

板卡渲染图/焊接图/原理图见 [`board/`](board/)。

## 已知限制

1. **800×480 不是 CEA 标准 HDMI 模式。** 支持 800×480（WVGA/PC 模式）的显示器/电视一般能显示；只接受 480p/720p/1080p 的电视仍可能拒绝。要彻底兼容所有 HDMI 屏，建议把 AMT630A 屏参重新生成为 **720×480p（858×525，27MHz）**，SII9022A 改用 VIC 2；当前代码已为 800×480 两种刷新率做好适配，改屏参时再扩展一个 720×480 模式即可。
2. **自动 50/60Hz 跟随**依赖 SII9022A 的 `H_RES/V_RES` 测量。如果某些板子测量不到，短按按键强制 60Hz 或 50Hz。
3. **低信号雪花**取决于 AMT630A 解码器是否判定 `tv_active`。如果极低信号下仍然黑而不是雪花，可以继续调 `0xFE10 hjit_max`、`0xFE11 hlock_num_thr`，或用 `0xFE28 noisy_flag` 提前强制雪花。
4. **PY32 型号**：Keil 工程按 `PY32F003x8`（64KB Flash / 8KB RAM）配置，编译出的 RW 约 7.5KB。若实际焊接的是 16KB/2KB 的 `PY32F003L24D6`，需要改器件、减小 RAM 并调整配置页地址；请先确认芯片丝印和实际容量。

## 第三方代码与许可

本仓库包含多个厂商的代码/二进制，**上传公开仓库前请确认你有权分发**：

- `py32/Drivers/`：Puya PY32F0xx HAL/CMSIS，BSD-3-Clause。
- `py32/Src/sil9022.c`、`py32/Inc/sil9022.h`：Silicon Image SiI9022A 驱动；文件头为 GPL-2.0，请保留原版权声明。
- `amt630a/ARKLib/*.LIB`、`amt630a/PC Tool/*.exe`、ARK SDK 源码：ARK Micro（艾科创新）的 AMT630A SDK，通常为厂商授权代码，不一定允许公开再分发。若没有明确授权，建议把仓库设为 private，或删除这些厂商二进制/源码后再公开。
- `board/` 下的原理图、PCB、渲染图和测试照片为你自己的硬件资料。

如果准备公开到 GitHub，建议先阅读各文件头部的版权说明，并在仓库中保留本节的第三方声明。
