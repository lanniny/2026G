set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_file "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"

open_project $proj_file
open_bd_design $bd_file

update_compile_order -fileset sources_1
set umr_msg ""
catch {update_module_reference [get_bd_cells ad9226test_0]} umr_msg
puts "UPDATE_MODULE_REFERENCE_MSG=$umr_msg"
validate_bd_design
save_bd_design

reset_target all [get_files $bd_file]
generate_target all [get_files $bd_file]
export_ip_user_files -of_objects [get_files $bd_file] -no_script -sync -force -quiet
update_compile_order -fileset sources_1

set smc_bd_synth "$proj_dir/adc_easy_test.gen/sources_1/bd/design_1/ip/design_1_axi_smc_0/bd_0/synth/bd_afc3.v"
if {[file exists $smc_bd_synth]} {
    puts "SMARTCONNECT_CHILD_SYNTH_EXISTS=$smc_bd_synth"
} else {
    puts "SMARTCONNECT_CHILD_SYNTH_MISSING=$smc_bd_synth"
}

puts "BD_OUTPUTS_REGENERATED=OK"
close_project
