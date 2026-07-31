connect
puts "=== TARGETS BEFORE ==="
targets
catch {targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}} target_msg
puts "TARGET_SET_MSG=$target_msg"
catch {stop} stop_msg
puts "STOP_MSG=$stop_msg"
catch {rst -processor} rst_msg
puts "RST_PROCESSOR_MSG=$rst_msg"
source "D:/FPGAPROJECT/ZYNQ/adctestvitis/adctestp/export/adctestp/hw/ps7_init.tcl"
catch {ps7_init} init_msg
puts "PS7_INIT_MSG=$init_msg"
catch {ps7_post_config} post_msg
puts "PS7_POST_CONFIG_MSG=$post_msg"
catch {mwr 0x00000000 0x12345678} mwr_msg
puts "MWR_DDR0_MSG=$mwr_msg"
catch {mrd 0x00000000 1} mrd_msg
puts "MRD_DDR0_MSG=$mrd_msg"
puts "=== TARGETS AFTER ==="
targets -target-properties -filter {name =~ "*Cortex-A9*"}
