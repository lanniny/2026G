connect
puts "=== TARGETS ==="
targets
puts "=== CORTEX A9 TARGETS ==="
set a9_targets [targets -target-properties -filter {name =~ "*Cortex-A9*"}]
puts $a9_targets
puts "=== DONE ==="
