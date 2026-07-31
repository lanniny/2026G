# 2026G

基于 Zynq-7020 和 LTC2208 16 位高速 ADC 的周期信号测量与谐波分析工程。当前 `v1.0.1` 已完成 PL 采集链、PS7 平台、Vitis 应用、JTAG 下载和 SD 卡固化，并加入针对 UART1 冷启动竞态的上电等待及有界超时；用户使用同哈希 SD 镜像完成冷启动实测，屏显恢复正常。

## 硬件与采样参数

- FPGA/SoC：XC7Z020-CLG484-1
- ADC：LTC2208，16 位 offset binary，并行 CMOS
- ADC 时钟：64 MHz
- 应用采样率：4 MS/s（PL 端 16 倍抽取）
- 模块输入：50 ohm，9 Vpp 满量程（-4.5 V 至 +4.5 V）
- 工具链：AMD Vivado/Vitis 2025.2

## LTC2208 引脚

| 信号 | Zynq 引脚 | 信号 | Zynq 引脚 |
| --- | --- | --- | --- |
| SHDN | W22 | CKI | V22 |
| D0 | Y21 | CKO | Y20 |
| D2 | AB22 | D1 | AA22 |
| D4 | AB21 | D3 | AA21 |
| D6 | AB19 | D5 | AB20 |
| D8 | AA19 | D7 | Y19 |
| D10 | AB16 | D9 | AA16 |
| D12 | Y18 | D11 | AA18 |
| D14 | AB14 | D13 | AB15 |
| OFA | Y13 | D15 | AA13 |

## 目录

- `adc_easy_test/adc_easy_test.xpr`：Vivado 工程入口
- `adc_easy_test/adc_easy_test.srcs/`：RTL、Block Design、IP 配置和 XDC
- `adc_easy_test/build_4msps.tcl`：LTC2208 迁移、综合、实现、时序检查和 XSA 导出
- `adctestvitis/adctestps/src/`：PS 应用源码
- `adctestvitis/adctestps/tests/`：主机端信号分析回归测试
- `adctestvitis/refresh_dual_uart_platform.py`：Vitis 平台刷新脚本
- `run_dual_uart_target.tcl`：JTAG 下载并运行应用
- `run_xsdb.ps1`：带完整 Vivado 运行环境的 XSDB 启动器
- `release/v1.0.1/`：可直接使用的 SD/JTAG 发布产物

## SD 卡启动

最直接的用法是将 `release/v1.0.1/BOOT.BIN` 放到 FAT32 SD 卡根目录，板卡切换到 SD 启动模式后重新上电。

Windows 下也可以使用带防误写检查的脚本。第一次命令只预检目标，第二次才写入：

```powershell
.\release\v1.0.1\write_sd.ps1 -DriveLetter K -ExpectedDiskIndex 2
.\release\v1.0.1\write_sd.ps1 -DriveLetter K -ExpectedDiskIndex 2 -ConfirmWrite
```

请按本机实际盘符和物理磁盘号替换示例值。脚本只接受 `Removable` 类型的 FAT/FAT32 分区，并在写入后回读 SHA-256。

重新生成镜像：

```powershell
.\release\v1.0.1\build_boot.ps1 -Force
```

`BOOT.BIN` 的分区顺序固定为 quiet FSBL、PL bitstream、PS 应用 ELF。

## 构建与测试

```powershell
F:\AMDDesignTools\2025.2\Vivado\bin\vivado.bat -mode batch -source .\adc_easy_test\build_4msps.tcl
python .\adctestvitis\refresh_dual_uart_platform.py
F:\AMDDesignTools\2025.2\Vivado\bin\empyro.bat build_app -s .\adctestvitis\adctestps\src -b .\adctestvitis\adctestps\build
cmd /c .\adctestvitis\adctestps\tests\run_host_tests.bat
cmd /c .\adc_easy_test\sim_4msps\run_sim.bat
.\run_xsdb.ps1 -Script .\run_dual_uart_target.tcl
```

JTAG 命令要求板卡、LTC2208 和调试器均已上电连接。当前发布的详细验证证据见 `release/v1.0.1/VERIFICATION.md`。

## v1.0.1 校验值

`release/v1.0.1/BOOT.BIN`：

```text
478E8A1A5B8FC5ED498E3DD5B184CA36B03ED9C9C00A59EC98D47758A26A37AB
```

完整校验表见 `release/v1.0.1/SHA256SUMS.txt`。

## 最新实测候选

`release/v1.0.2-rc2/BOOT.BIN` 修复低频大幅度纯正弦在有限记录中泄漏为伪
谐波的问题，并按官方问答加入 500 Hz 基频网格、5 mV 最小分量、H50 上限和
“H1 + 最多两个谐波”约束。它已通过主机回归、160 组全网格压力扫、
Cortex-A9 构建、PL 仿真、Bootgen 反读和板上实测。

实测同时确认：供电能力不足会导致串口屏刷新变慢、低频频谱幅值与频率随机
错误。使用装置唯一 5 V 输入供电时，必须保证电源和连接线在整机负载下稳定。

`v1.0.2-rc2` BOOT.BIN SHA-256：

```text
9D7FFE73213BBDF76F62ADD4A1D2FB5A30E100AD70EB631B4A4F85FEBE9CAE3C
```
