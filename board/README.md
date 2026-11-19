# 硬件资料

- `cvbs_system.eprj2`：EasyEDA Pro 工程（原理图 + PCB）
- `SCH_Schematic2_2026-03-20.pdf`：原理图 PDF
- `pictures/渲染图.png`：PCB 渲染图
- `pictures/焊接图.jpg`：焊接/装配参考
- `pictures/有信号测试.jpg`：有信号输出测试
- `pictures/无信号雪花测试.jpg`：无信号雪花测试

信号链：CVBS → AMT630A → RGB888 + HS/VS/DE → SII9022A → HDMI；PY32F003 通过 I2C 配置 SII9022A。
