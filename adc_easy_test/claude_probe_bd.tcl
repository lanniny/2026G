set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
open_project "$proj_dir/adc_easy_test.xpr"
open_bd_design "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"
puts "--- update_module_reference help ---"
catch {help update_module_reference} msg
puts $msg
puts "--- current ad9248test_0 pins ---"
foreach p [lsort [get_bd_pins -quiet ad9248test_0/*]] { puts $p }
puts "--- current ad9248test_0 intf pins ---"
foreach p [lsort [get_bd_intf_pins -quiet ad9248test_0/*]] { puts $p }
puts "--- files matching ad9248test ---"
foreach f [get_files -quiet *ad9248test.v] { puts $f }
close_project
