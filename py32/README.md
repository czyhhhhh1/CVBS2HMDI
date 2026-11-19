# PY32F003 + SII9022A 部分

PY32F003 通过软件 I2C 配置 SII9022A，把 AMT630A 输出的 24-bit RGB888（800×480）编码成 HDMI。工程同时提供 Keil MDK 和 EIDE/GCC 两套构建方式。

## 硬件连接

| 信号 | PY32 引脚 | 说明 |
|---|---|---|
| I2C SCL | PA14 | 到 SII9022A SCL，外部上拉 |
| I2C SDA | PA13 | 到 SII9022A SDA，外部上拉；按键也接在此脚 |
| 按键 | PA13 | 按下拉低，初始化完成后生效 |
| SII9022A 地址 | 0x39（7 位） | 8 位写地址 0x72 |

SII9022A 的 RESET#/HPD 按板子实际连接处理；本工程通过 TPI 软件复位和 HPD 轮询工作。

## 目录

```text
py32/
├─ Src/
│  ├─ main.c                 初始化、按键、HPD/时序轮询、Flash 配置
│  ├─ sil9022.c              SII9022A TPI 驱动
│  ├─ my_i2c.c               软件 I2C
│  ├─ py32f0xx_hal_msp.c     HAL 底层
│  ├─ py32f0xx_it.c          中断
│  └─ system_py32f0xx.c      系统时钟
├─ Inc/
│  ├─ main.h
│  ├─ sil9022.h
│  ├─ my_i2c.h
│  ├─ py32f0xx_hal_conf.h
│  └─ py32f0xx_it.h
├─ Drivers/                  PY32F0xx HAL、CMSIS、Start Kit（已裁剪）
├─ MDK-ARM/                  Keil MDK 工程
├─ EIDE/                     VS Code + EIDE + arm-none-eabi-gcc 工程
└─ readme.txt                工程内短说明
```

## 视频时序

AMT630A 7 寸 800×480 屏参的 DCLK = 30.75MHz，SII9022A 的 TPI 必须匹配：

| 模式 | 宏 | 像素时钟 | H total | V total | VFreq |
|---|---|---|---:|---:|---:|
| 800×480@60 | `HDMI_800_480_60` | 30.75MHz | 929 | 552 | 5996 |
| 800×480@50 | `HDMI_800_480_50` | 30.75MHz | 1129 | 544 | 5007 |

两个模式都使用：

- `VIC = 0`（800×480 不是 CEA 模式）
- AVI 16:9，BT.709
- `TclkSel = X1`（24-bit RGB，单像素/时钟）
- `EXTERNAL_HSVSDE`（外部 HS/VS/DE）

`siHdmiTx_UpdateTimingFromInput()` 每 500ms 读一次 SII9022A 的 `H_RES/V_RES`（TPI `0x6A~0x6D`），识别 929×552 或 1129×544 后改写 `TPI 0x00~0x07`，所以 AMT630A 在 PAL/NTSC 之间切换时 HDMI 侧能自动跟随。

## 初始化和修复点

`main.c` 的 `sil9022_init()`：

1. `I2C_GPIO_Init()`，约 100ms 等 SII 上电
2. `siHdmiTx_VideoSel()` + `siHdmiTx_TPI_Init()`，失败重试 3 次
3. `siHdmiTx_Init(out_mode)`：初始化视频、发 AVI InfoFrame、开 TMDS
4. `siHdmiTx_PowerStateD0fromD2()`
5. 自动读一次输入时序（auto 模式）

关键修复：

- `siHdmiTx_Init()` 补发 `SetAVI_InfoFrames()`。原移植版漏了这一步，很多 HDMI 屏没有 AVI 就一直黑。
- 打开 SiI 参考代码的 Bug#18128 8bit workaround（原代码被 `#if 0` 关掉）。
- `siHdmiTx_VideoSel()` 新增 800×480@60/50 两个非 CEA 模式，VIC 0。
- `ConvertVIC_To_VM_Index()` 对 PC 模式 VIC 越界做了保护。
- `StartTPI()` 的 10 秒等待改为 50ms。
- 软件 I2C 从 `HAL_Delay(1)`（每 bit 2ms）改为约 5µs 半位（约 100kHz），并加：
  - SDA 先写 ODR 再切输出，避免毛刺
  - Start/Stop 建立/保持时间
  - 9 时钟总线恢复
  - `I2C_DELAY_LOOPS` 可调（默认 20）
- HPD 事件轮询：插拔 HDMI 后重发 AVI、重新打开 TMDS。
- 按键消抖后短按/长按切换模式，配置写入 Flash `0x0800FC00`。

## 构建

### Keil MDK

1. 打开：

   ```text
   MDK-ARM/Project.uvprojx
   ```

2. 器件：`PY32F003x8`（64KB Flash / 8KB RAM）。
3. Build（F7），下载。

工程已把原来 `..\..\..\..\..\..\Drivers` 的六层路径改成 `..\Drivers`，`Drivers/` 已随仓库提供，不需要额外安装 PY32 SDK。

### EIDE / GCC

1. VS Code 安装 EIDE、ARM GCC、PY32F0xx DFP。
2. 打开：

   ```text
   EIDE/Project.code-workspace
   ```

3. 目标 `Project`，Build。

`EIDE/Makefile` 和 `EIDE/.eide/eide.json` 已包含 `sil9022.c`、`my_i2c.c`，路径也已改成 `../Drivers`。

## 按键和 Flash 配置

| 操作 | 功能 |
|---|---|
| 短按（≥50ms，<2s） | 自动 → 强制 60Hz → 强制 50Hz → 自动 |
| 长按（≥2s） | HDMI ↔ DVI |

Flash 地址 `0x0800FC00`（64KB 最后 1KB），保存两个 32 位值：

- `cfg[0]`：0 = DVI，1 = HDMI
- `cfg[1]`：0 = 自动，1 = 强制 60Hz，2 = 强制 50Hz

空白/非法值会自动写默认值（HDMI + 自动）。

## 器件容量注意

Keil 工程按 `PY32F003x8` 配置，旧构建的 RW 约 7.5KB（8KB SRAM）。如果你实际焊的是 `PY32F003L24D6`（数据手册标 16KB Flash / 2KB SRAM），这个工程放不下，需要：

- 改 Keil Device 为 `PY32F003x6`/对应型号
- 减少全局变量/缓冲区
- 把配置页地址改到实际 Flash 最后一页

请先确认芯片丝印和实际 Flash/RAM 容量。

## 许可

- `Drivers/`：Puya PY32F0xx HAL/CMSIS，BSD-3-Clause。
- `Src/sil9022.c`、`Inc/sil9022.h`：Silicon Image SiI9022A 驱动，文件头为 GPL-2.0，请保留版权声明。
