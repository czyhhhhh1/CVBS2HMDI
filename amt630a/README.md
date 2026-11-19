# AMT630A 部分

AMT630A 是 CVBS 解码 + Scaler + TCON 的 8051 内核芯片。本工程由 ARK（艾科创新）AMT630A 车载显示器 SDK 修改而来，用于把 CVBS 图传解码成 24-bit RGB888，送给 SII9022A 转 HDMI。

当前板子使用 7 寸 800×480 屏参（`P_MEX070TM_50D_DHV_AMT630A_20141113`），不是 LCD 屏，而是把 RGB/HS/VS/DE 接到 SII9022A。

## 目录

```text
amt630a/
├─ ARKLib/                 ARK 视频库、IO 库、SPI 库（.LIB）
├─ Debug/                  调试串口
├─ Device driver/          AMT630A 驱动、I2C、Flash、按键、WT5700 等
├─ GetUserInput/           按键/红外输入
├─ Inc/                    全局头文件
├─ Kernel/                 主循环、消息、中断
├─ LCD Pannels/            屏参和屏参写入
├─ Modules/                电源、视频处理、电池
├─ OSD/                    OSD 菜单、字库、Logo
├─ UserConfig/             工程宏配置
├─ PC Tool/                Keil 后处理工具（HexMerge 等）
└─ Keil Prj/64K/Prj/       Keil C51 工程
```

## 编译

1. 用 Keil uVision（C51）打开：

   ```text
   Keil Prj/64K/Prj/ARK_SDK_Ver2.0.uvproj
   ```

2. 选择目标 `ARK-SDK`，Build（F7）。
3. 输出：

   ```text
   Keil Prj/64K/Hex/AMT_CAR_HEX.hex
   Keil Prj/Bin/AMT_CAR_BIN.bin
   ```

构建后处理（`AfterMake`）会执行：

```bat
..\..\..\PC Tool\18X9_Function_Location "\\List"
..\..\..\PC Tool\HexMerge ..\..\Bin\AMT_CAR_BIN.bin ..\Hex\AMT_CAR_HEX.hex ..\..\..\OSD\Logo\Hex\Logo.hex
```

所以仓库里保留了：

- `PC Tool/18X9_Function_Location.exe`
- `PC Tool/HexMerge.exe`
- `OSD/Logo/Hex/Logo.hex`（Logo 字模，合并用）

如果后处理报路径错误，确认是在 `Keil Prj/64K/Prj` 目录下执行，或者直接在 Keil 的 `Options for Target → User` 里检查两条命令。

> 源码是 **GBK/ANSI 编码 + CRLF**。用 VS Code 打开时请选择 GBK/GB2312，不要保存成 UTF-8，否则 Keil C51 里的中文注释可能乱码。

## 7 寸 800×480 屏参时序

`LCD Pannels/LCD parameter/P_MEX070TM_50D_DHV_AMT630A_20141113.c` 里的关键值：

- 显示 PLL：`27MHz ÷ 9 × 164 ÷ 4 = 123MHz`
- `0xFD15 = 0x04`，DCLK = 123MHz / 4 = **30.75MHz**
- NTSC/PAL60：`Thlen = 929`，`Tvlen = 552` → 59.95Hz
- PAL：`Thlen = 1129`，`Tvlen = 544` → 50.07Hz

SII9022A 侧必须按这组 H/V total 和 30.75MHz 配置，不能用标准 480i/27MHz。PY32 工程里的 `HDMI_800_480_60` / `HDMI_800_480_50` 两个模式就是对应这组值。

## 本工程相对原厂 SDK 的修改

### 1. 无信号雪花

原屏参 `0xFFB0 = 0x27`：

- bit5 `BLUE_SCREEN_EN = 1`：无信号蓝屏打开
- bit7 `snowflower_en = 0`：雪花关闭

ARK 视频库 `InitAMT()` 还会写 `0xFFD8 = 0x80`（输入不稳定时硬件自动蓝屏）。所以只打开 `NoSignalSnowEn` 是不够的，蓝屏会盖住雪花。

`LCD Pannels/ConfigLcdPara.c` 的 `ConfigStaticPara()` 在写完屏参后强制：

```c
XBYTE[0xFFD8] = 0x00;      /* 关硬件自动蓝屏 */
XBYTE[0xFFB0] &= ~_BIT5;   /* 关蓝屏 */
XBYTE[0xFFB0] |= _BIT7;    /* 开雪花 */
XBYTE[0xFFDA] = 0x6C;      /* 雪花点大小/源 */
```

`Device driver/AMT/AMT_Drv.c` 的 `SetNoSignalSnow()` 也补了：

```c
VP_EN_REG &= ~_BIT5;   /* 蓝屏必须关，否则会盖住雪花 */
VP_EN_REG |= _BIT7;
SNOW_BIG_SEL = 0x6C;
```

### 2. 项目宏

`UserConfig/SystemConfig.h` 选择 `PRJ_AMT_7D_SDK_DEMO`。

`UserConfig/Prj config/Prj_AMT_7D_SDK_DEMO.h`：

- `#define NoSignalSnowEn`：无信号雪花
- `#define BacklightEn`：背光控制（本板可不用）
- 屏参：`P_MEX070TM_50D_DHV_AMT630A_20141113_EN`

### 3. 其他

- `Kernel/`、`Modules/VideoProc.c` 等保持原厂逻辑
- 工程里保留了 `17` 位/`18` 位并口、I2C 按键、WT5700、BT 等原厂驱动，便于以后改板

## 注意

- ARK SDK、`ARKLib/*.LIB`、`PC Tool/*.exe` 是厂商授权代码/工具，公开到 GitHub 前请确认你有再分发权；没有授权就设 private 或删除。
- 不要删除 `OSD/Logo/Hex/Logo.hex`，否则 `HexMerge` 后处理会找不到 Logo 字模。
- `Keil Prj/Bin/.gitkeep` 只是占位，保证 `Bin` 目录存在；编译后会生成 `AMT_CAR_BIN.bin`。
