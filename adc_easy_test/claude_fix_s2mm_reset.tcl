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

if {[llength [get_bd_cells -quiet rst_s2mm_data]] == 0} {
    create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_s2mm_data
}
catch {set_property -dict [list CONFIG.C_EXT_RESET_HIGH {0}] [get_bd_cells rst_s2mm_data]}

disconnect_pin_nets rst_s2mm_data/slowest_sync_clk
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins rst_s2mm_data/slowest_sync_clk]

disconnect_pin_nets rst_s2mm_data/ext_reset_in
connect_bd_net [get_bd_pins processing_system7_0/FCLK_RESET0_N] [get_bd_pins rst_s2mm_data/ext_reset_in]

foreach rst_pin {axi_mem_intercon/ARESETN axi_mem_intercon/S00_ARESETN axi_mem_intercon/M00_ARESETN} {
    disconnect_pin_nets $rst_pin
    connect_bd_net [get_bd_pins rst_s2mm_data/peripheral_aresetn] [get_bd_pins $rst_pin]
}

validate_bd_design
save_bd_design
make_wrapper -files [get_files $bd_file] -top -import
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1
close_project
