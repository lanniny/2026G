set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set xsa_dir "$proj_dir/export"
set xsa_file "$xsa_dir/adc_easy_test_dma_hp0.xsa"

open_project $proj_file
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

set synth_status [get_property STATUS [get_runs synth_1]]
puts "SYNTH_STATUS=$synth_status"
if {[string first "Complete" $synth_status] < 0} {
    launch_runs synth_1 -jobs 32
    wait_on_run synth_1
    set synth_status [get_property STATUS [get_runs synth_1]]
    puts "SYNTH_STATUS_AFTER=$synth_status"
    if {[string first "Complete" $synth_status] < 0} {
        error "synth_1 failed: $synth_status"
    }
}

reset_run impl_1
launch_runs impl_1 -to_step write_bitstream -jobs 32
wait_on_run impl_1
set impl_status [get_property STATUS [get_runs impl_1]]
puts "IMPL_STATUS=$impl_status"
if {[string first "Complete" $impl_status] < 0} {
    error "impl_1 failed: $impl_status"
}

file mkdir $xsa_dir
write_hw_platform -fixed -include_bit -force -file $xsa_file
puts "XSA_OUTPUT=$xsa_file"
close_project
