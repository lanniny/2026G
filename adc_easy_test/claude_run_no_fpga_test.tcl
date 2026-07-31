connect
puts "=== TARGETS INITIAL ==="
targets
puts "=== SET A9 ==="
targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}
catch {stop} stop_msg
puts "STOP_MSG=$stop_msg"
catch {rst -processor} rst_msg
puts "RST_PROCESSOR_MSG=$rst_msg"
puts "=== PS7 INIT ==="
source "D:/FPGAPROJECT/ZYNQ/adctestvitis/adctestp/export/adctestp/hw/ps7_init.tcl"
catch {ps7_init} init_msg
puts "PS7_INIT_MSG=$init_msg"
catch {ps7_post_config} post_msg
puts "PS7_POST_CONFIG_MSG=$post_msg"
puts "=== DDR PROBE ==="
catch {mwr 0x00100000 0x12345678} mwr_msg
puts "MWR_MSG=$mwr_msg"
catch {mrd 0x00100000 1} mrd_msg
puts "MRD_MSG=$mrd_msg"
puts "=== DOWNLOAD ELF ==="
catch {dow "D:/FPGAPROJECT/ZYNQ/adctestvitis/adctestps/build/adctestps.elf"} dow_msg
puts "DOW_MSG=$dow_msg"
catch {con} con_msg
puts "CON_MSG=$con_msg"
