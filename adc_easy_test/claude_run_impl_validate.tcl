set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
open_project $proj_file
update_compile_order -fileset sources_1
reset_run synth_1
reset_run impl_1
launch_runs impl_1 -to_step place_design -jobs 16
wait_on_run impl_1
set status [get_property STATUS [get_runs impl_1]]
puts "IMPL_STATUS=$status"
close_project
