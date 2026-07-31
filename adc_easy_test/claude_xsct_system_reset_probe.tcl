connect
puts "=== TARGETS ==="
targets
catch {targets -set -filter {name =~ "APU"}} apu_msg
puts "SET_APU_MSG=$apu_msg"
catch {rst -system} sysrst_msg
puts "RST_SYSTEM_MSG=$sysrst_msg"
after 1000
catch {targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}} a9_msg
puts "SET_A9_MSG=$a9_msg"
catch {stop} stop_msg
puts "STOP_MSG=$stop_msg"
targets
