set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
open_project "$proj_dir/adc_easy_test.xpr"
open_bd_design "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"
puts "--- get_ips ---"
foreach ip [lsort [get_ips -quiet *ad9248*]] { puts $ip }
puts "--- get_bd_cells properties ---"
puts [report_property -return_string [get_bd_cells ad9248test_0]]
puts "--- update module reference by xci/ip instance name ---"
foreach name {design_1_ad9248test_0_0 /design_1_ad9248test_0_0 ad9248test ad9248test_0} {
    puts "TRY $name"
    if {[catch {update_module_reference -verbose $name} msg]} { puts "ERROR<$msg>" } else { puts "OK<$msg>" }
}
puts "--- after pins ---"
foreach p [lsort [get_bd_pins -quiet ad9248test_0/*]] { puts $p }
close_project
