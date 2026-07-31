set sim_dir [file normalize [file dirname [info script]]]
set rtl_file [file normalize [file join $sim_dir .. \
    adc_easy_test.srcs sources_1 new ad9226test.v]]
set glbl_file [file normalize [file join $::env(XILINX_VIVADO) \
    data verilog src glbl.v]]
cd $sim_dir

proc run_tool {args} {
    puts "RUN_TOOL=$args"
    if {[catch {exec {*}$args 2>@1} output options]} {
        puts $output
        return -options $options $output
    }
    puts $output
}

run_tool xvlog -sv ad9226_sim_models.sv tb_ad9226_4msps.sv $rtl_file \
    $glbl_file
run_tool xelab -L xpm tb_ad9226_4msps glbl -s tb_ad9226_4msps_sim
run_tool xsim tb_ad9226_4msps_sim -runall

if {![file exists [file join $sim_dir pl_samples.txt]]} {
    error "Simulation did not create pl_samples.txt"
}
puts "LTC2208_RTL_SIM=PASS"
exit
