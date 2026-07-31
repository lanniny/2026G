set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
open_project "$proj_dir/adc_easy_test.xpr"
set rtl "$proj_dir/adc_easy_test.srcs/sources_1/new/ad9248test.v"
update_compile_order -fileset sources_1
open_bd_design "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"
puts "--- before pins ---"
foreach p [lsort [get_bd_pins -quiet ad9248test_0/*]] { puts $p }
puts "--- update module reference by name ---"
if {[catch {update_module_reference -verbose ad9248test_0} msg]} { puts "ERROR: $msg" } else { puts $msg }
puts "--- after pins ---"
foreach p [lsort [get_bd_pins -quiet ad9248test_0/*]] { puts $p }
close_project
