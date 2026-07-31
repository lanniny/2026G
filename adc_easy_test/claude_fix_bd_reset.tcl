set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_file "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"

proc disconnect_pin_nets {pin_name} {
    set pins [get_bd_pins -quiet $pin_name]
    foreach pin $pins {
        set nets [get_bd_nets -quiet -of_objects $pin]
        foreach net $nets { catch {disconnect_bd_net $net $pin} }
    }
}

open_project $proj_file
open_bd_design $bd_file

if {[llength [get_bd_cells -quiet rst_axis_stream]] == 0} {
    create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_axis_stream
}

disconnect_pin_nets rst_axis_stream/slowest_sync_clk
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins rst_axis_stream/slowest_sync_clk]

disconnect_pin_nets rst_axis_stream/ext_reset_in
connect_bd_net [get_bd_pins processing_system7_0/FCLK_RESET0_N] [get_bd_pins rst_axis_stream/ext_reset_in]

if {[llength [get_bd_pins -quiet rst_axis_stream/dcm_locked]] > 0} {
    disconnect_pin_nets rst_axis_stream/dcm_locked
    connect_bd_net [get_bd_pins ad9248test_0/SYS_RST_N_IN] [get_bd_pins rst_axis_stream/dcm_locked]
}

disconnect_pin_nets axis_clock_converter_0/s_axis_aresetn
connect_bd_net [get_bd_pins rst_axis_stream/peripheral_aresetn] [get_bd_pins axis_clock_converter_0/s_axis_aresetn]

validate_bd_design
save_bd_design
make_wrapper -files [get_files $bd_file] -top -import
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1
close_project
