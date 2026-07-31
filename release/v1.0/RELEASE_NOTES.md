# 2026G LTC2208 16-bit v1.0

发布日期：2026-07-31

## 本版内容

- 将原采集链迁移为单通道 LTC2208 16 位并行 CMOS 输入。
- 使用 64 MHz ADC 时钟和 16 倍抽取，保持 PS 端 4 MS/s 分析接口。
- 完成 LTC2208 引脚、输入时序、Block Design、PS7、DMA 和 Vitis 应用联调。
- 保留现有周期信号测量、频率/幅值计算、谐波识别、串口输出和串口屏流程。
- 提供 JTAG 调试产物、完整 XSA、三段式 SD 启动镜像及安全写卡脚本。

## 验证摘要

- Vivado routed timing：WNS `+2.366 ns`，TNS `0.000 ns`，零失败端点。
- LTC2208 输入 setup 最差裕量：`+2.366 ns`。
- 主机回归：`PS_ANALYSIS_PASS`、`REQUIREMENT_SWEEP_PASS`、`INTERFERENCE_TEST_PASS`。
- JTAG 实机：FPGA 配置、PS 初始化、ELF 下载和应用运行均成功。
- 用户实测：原 12 位方案下的小幅二次/四次谐波易误判问题已基本解决。
- SD 镜像经 Bootgen 反解确认包含 FSBL、bitstream 和应用 ELF，应用入口为 `0x00100000`。
- SD 卡写后回读哈希通过。

## SD 固化文件

直接使用 `BOOT.BIN`。其 SHA-256 为：

```text
62B1433683941D22092E8EEF03FF1BCE179CC4B582CE81371936C0C16181FC45
```

所有文件校验值见 `SHA256SUMS.txt`。

