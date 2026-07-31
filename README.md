# 2026G

基于 Zynq-7020 和 LTC2208 16 位高速 ADC 的周期信号测量与谐波分析工程。当前 `v1.0` 已完成 PL 采集链、PS7 平台、Vitis 应用、JTAG 下载和 SD 卡固化，并由实机信号测试确认原 12 位采集下的小幅谐波误判问题已基本解决。

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
- `release/v1.0/`：可直接使用的 SD/JTAG 发布产物

## SD 卡启动

最直接的用法是将 `release/v1.0/BOOT.BIN` 放到 FAT32 SD 卡根目录，板卡切换到 SD 启动模式后重新上电。

Windows 下也可以使用带防误写检查的脚本。第一次命令只预检目标，第二次才写入：

```powershell
.\release\v1.0\write_sd.ps1 -DriveLetter K -ExpectedDiskIndex 2
.\release\v1.0\write_sd.ps1 -DriveLetter K -ExpectedDiskIndex 2 -ConfirmWrite
```

请按本机实际盘符和物理磁盘号替换示例值。脚本只接受 `Removable` 类型的 FAT/FAT32 分区，并在写入后回读 SHA-256。

重新生成镜像：

```powershell
.\release\v1.0\build_boot.ps1 -Force
```

`BOOT.BIN` 的分区顺序固定为 quiet FSBL、PL bitstream、PS 应用 ELF。

## 构建与测试

```powershell
F:\AMDDesignTools\2025.2\Vivado\bin\vivado.bat -mode batch -source .\adc_easy_test\build_4msps.tcl
python .\adctestvitis\refresh_dual_uart_platform.py
cmd /c .\adctestvitis\adctestps\tests\run_host_tests.bat
cmd /c .\adc_easy_test\sim_4msps\run_sim.bat
F:\AMDDesignTools\2025.2\Vivado\bin\xsdb.bat .\run_dual_uart_target.tcl
```

JTAG 命令要求板卡、LTC2208 和调试器均已上电连接。当前发布的详细验证证据见 `release/v1.0/VERIFICATION.md`。

## v1.0 校验值

`release/v1.0/BOOT.BIN`：

```text
62B1433683941D22092E8EEF03FF1BCE179CC4B582CE81371936C0C16181FC45
```

完整校验表见 `release/v1.0/SHA256SUMS.txt`。

