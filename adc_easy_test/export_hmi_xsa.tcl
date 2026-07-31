set proj_dir [file normalize [file dirname [info script]]]
set proj_file [file join $proj_dir adc_easy_test.xpr]
set xsa_file [file join $proj_dir design_1_wrapper.xsa]

open_project $proj_file
write_hw_platform -fixed -include_bit -force -file $xsa_file

puts "HMI_XSA=$xsa_file"
puts "HMI_XSA_EXPORT_COMPLETE"
