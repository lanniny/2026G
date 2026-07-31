connect
puts "=== TARGETS INITIAL ==="
targets
puts "=== PROGRAM FPGA ==="
targets -set -filter {name =~ "xc7z020"}
fpga "D:/FPGAPROJECT/ZYNQ/adc_easy_test/adc_easy_test.runs/impl_1/design_1_wrapper.bit"
puts "FPGA_PROGRAM_DONE"
after 1000
puts "=== SET A9 ==="
targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}
catch {stop} stop_msg
puts "STOP_MSG=$stop_msg"
catch {rst -processor} rst_msg
puts "RST_PROCESSOR_MSG=$rst_msg"
puts "=== PS7 INIT ==="
source "D:/FPGAPROJECT/ZYNQ/adctestvitis/adctestp/export/adctestp/hw/ps7_init.tcl"
ps7_init
ps7_post_config
puts "PS7_INIT_DONE"
puts "=== DDR PROBE ==="
mwr 0x00100000 0x12345678
mrd 0x00100000 1
puts "=== DOWNLOAD ELF ==="
dow "D:/FPGAPROJECT/ZYNQ/adctestvitis/adctestps/build/adctestps.elf"
puts "ELF_DOWNLOAD_DONE"
con
puts "APP_STARTED"
