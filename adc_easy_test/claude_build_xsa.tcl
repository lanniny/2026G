set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_file "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"
set xsa_dir "$proj_dir/export"
set xsa_file "$xsa_dir/adc_easy_test_dma_hp0.xsa"

open_project $proj_file
open_bd_design $bd_file
validate_bd_design
save_bd_design
make_wrapper -files [get_files $bd_file] -top -import
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

generate_target all [get_files $bd_file]
export_ip_user_files -of_objects [get_files $bd_file] -no_script -sync -force -quiet

reset_run synth_1
launch_runs synth_1 -jobs 32
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%" || [get_property STATUS [get_runs synth_1]] !~ "*Complete*"} {
    error "synth_1 failed: [get_property STATUS [get_runs synth_1]]"
}

reset_run impl_1
launch_runs impl_1 -to_step write_bitstream -jobs 32
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%" || [get_property STATUS [get_runs impl_1]] !~ "*Complete*"} {
    error "impl_1 failed: [get_property STATUS [get_runs impl_1]]"
}

file mkdir $xsa_dir
write_hw_platform -fixed -include_bit -force -file $xsa_file
puts "XSA_OUTPUT=$xsa_file"
close_project
