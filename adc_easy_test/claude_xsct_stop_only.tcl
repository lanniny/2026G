connect
puts "=== TARGETS ==="
targets
catch {targets -set -filter {name =~ "ARM Cortex-A9 MPCore #0"}} target_msg
puts "TARGET_SET_MSG=$target_msg"
catch {stop} stop_msg
puts "STOP_MSG=$stop_msg"
puts "=== TARGETS AFTER STOP ==="
targets -target-properties -filter {name =~ "*Cortex-A9*"}
