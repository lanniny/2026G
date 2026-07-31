# 2026G LTC2208 16-bit v1.0.1

发布日期：2026-07-31

## 本版改进

- 为 SD 冷启动时 Zynq 与串口屏同步上电的 UART1 波特率切换竞态加入针对性处理。
- 串口屏初始化前增加 2 秒有界上电等待，再执行 9600 到 115200 的双波特率同步。
- 删除应用对 `XUartPs_WaitTransmitDone` 的调用；TX FIFO 等待限制为 20 ms，整帧发送完成等待限制为 250 ms。
- UART1 超时后只禁用 HMI 输出，主采集、谐波分析和 UART0 诊断继续运行。
- Vitis 平台刷新时强制同步当前 XSA 的 `ps7_init.c/.h` 到 quiet FSBL，并修复生成工程的 ARM size 工具路径。
- 修复主机回归和 PL 仿真脚本对启动目录的隐式依赖。

## 验证摘要

- Vitis 平台与 quiet FSBL 从当前 XSA 完整重建成功。
- PS 应用 clean-build 成功，`-Wall -Wextra` 无应用源码警告。
- 应用反汇编确认 `XUartPs_WaitTransmitDone` 调用数为 0。
- 主机回归通过：`PS_ANALYSIS_PASS`、`REQUIREMENT_SWEEP_PASS`、`INTERFERENCE_TEST_PASS`。
- PL 仿真通过：`PL_SIM_PASS samples=8192 status=0x8a`。
- Bootgen 生成并反读三段式镜像成功，应用入口为 `0x00100000`。

## SD 固化文件

将 `BOOT.BIN` 写入 FAT/FAT32 SD 卡根目录。SHA-256：

```text
478E8A1A5B8FC5ED498E3DD5B184CA36B03ED9C9C00A59EC98D47758A26A37AB
```

完整校验值见 `SHA256SUMS.txt`，详细验证证据见 `VERIFICATION.md`。

当前镜像已完成 SD 写入与哈希回读。用户使用 17:37 生成的同哈希镜像完成 SD 冷启动实测，串口屏显示正常。
