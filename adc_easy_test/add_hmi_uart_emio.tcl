# Add a dedicated serial-screen port while keeping the existing UART0/USB
# debug connection.  The serial screen uses PS UART1 routed through EMIO to
# ordinary PL package pins constrained in ad9226x.xdc.

set proj_dir [file normalize [file dirname [info script]]]
set proj_file [file join $proj_dir adc_easy_test.xpr]
set bd_file [file join $proj_dir adc_easy_test.srcs sources_1 bd design_1 design_1.bd]

if {[llength [get_projects -quiet]] == 0} {
    open_project $proj_file
}

open_bd_design $bd_file
set ps [get_bd_cells processing_system7_0]
if {[llength $ps] != 1} {
    error "processing_system7_0 was not found in design_1.bd"
}

set_property -dict [list \
    CONFIG.PCW_UART1_PERIPHERAL_ENABLE {1} \
    CONFIG.PCW_UART1_UART1_IO {EMIO}] $ps

set uart1_pin [get_bd_intf_pins -quiet $ps/UART_1]
if {[llength $uart1_pin] != 1} {
    error "PS UART1 interface did not appear after enabling UART1/EMIO"
}

if {[llength [get_bd_intf_ports -quiet HMI_UART_0]] == 0} {
    set hmi_uart [create_bd_intf_port \
        -mode Master \
        -vlnv xilinx.com:interface:uart_rtl:1.0 \
        HMI_UART_0]
    connect_bd_intf_net $hmi_uart $uart1_pin
} else {
    set hmi_uart [get_bd_intf_ports HMI_UART_0]
    set uart1_net [get_bd_intf_nets -quiet -of_objects $uart1_pin]
    if {[llength $uart1_net] == 0} {
        connect_bd_intf_net $hmi_uart $uart1_pin
    }
}

validate_bd_design
save_bd_design
set bd_object [get_files $bd_file]
generate_target all $bd_object

set wrapper_files [make_wrapper -files $bd_object -top]
if {[llength [get_files -quiet */design_1_wrapper.v]] == 0} {
    add_files -norecurse $wrapper_files
}

update_compile_order -fileset sources_1

set uart1_enable [get_property CONFIG.PCW_UART1_PERIPHERAL_ENABLE $ps]
set uart1_io [get_property CONFIG.PCW_UART1_UART1_IO $ps]
puts "HMI_UART1_ENABLE=$uart1_enable"
puts "HMI_UART1_IO=$uart1_io"
if {$uart1_enable ne "1" || $uart1_io ne "EMIO"} {
    error "UART1 configuration did not persist: enable=$uart1_enable io=$uart1_io"
}
if {[llength [get_bd_intf_ports -quiet HMI_UART_0]] != 1} {
    error "HMI_UART_0 external interface was not created"
}

puts "HMI UART1 EMIO port added as HMI_UART_0."
puts "Starting synthesis/implementation and exporting an XSA with bitstream..."

reset_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1

set impl_status [get_property STATUS [get_runs impl_1]]
puts "HMI_IMPL_STATUS=$impl_status"
if {[string first "Complete" $impl_status] < 0} {
    error "Implementation failed: $impl_status"
}

set xsa_file [file join $proj_dir design_1_wrapper.xsa]
write_hw_platform -fixed -include_bit -force -file $xsa_file

puts "HMI_XSA=$xsa_file"
puts "HMI UART hardware update complete."
