set project_root [file dirname [file normalize [info script]]]
set bit_file [file join $project_root adc_easy_test adc_easy_test.runs impl_1 design_1_wrapper.bit]
set xsa_file [file join $project_root adctestvitis adctestp export adctestp hw design_1_wrapper.xsa]
set ps7_init_file [file join $project_root adctestvitis adctestp hw sdt ps7_init.tcl]
set elf_file [file join $project_root adctestvitis adctestps build adctestps.elf]

foreach required_file [list $bit_file $xsa_file $ps7_init_file $elf_file] {
    if {![file isfile $required_file]} {
        error "Required JTAG artifact was not found: $required_file"
    }
}

connect
targets -set -nocase -filter {name =~ "APU*"}
rst -system
after 3000

targets -set -nocase -filter {name =~ "xc7z020"}
fpga -file $bit_file
puts "LTC2208_JTAG_FPGA_DONE"

targets -set -nocase -filter {name =~ "APU*"}
configparams force-mem-access 1
source $ps7_init_file
ps7_init
ps7_post_config
puts "LTC2208_JTAG_PS_INIT_DONE"

targets -set -nocase -filter {name =~ "*A9*#0"}
rst -processor
dow $elf_file
puts "LTC2208_JTAG_ELF_DOWNLOAD_DONE"
con
configparams force-mem-access 0
disconnect
puts "LTC2208_APPLICATION_RUNNING"
