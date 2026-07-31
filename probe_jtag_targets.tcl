connect

set target_list [targets]
puts "JTAG_TARGETS_BEGIN"
puts $target_list
puts "JTAG_TARGETS_END"

if {[string first "xc7z020" $target_list] < 0} {
    error "xc7z020 target was not detected"
}
if {[string first "Cortex-A9" $target_list] < 0} {
    error "Cortex-A9 target was not detected"
}

puts "JTAG_TARGET_PROBE_PASS"
disconnect
