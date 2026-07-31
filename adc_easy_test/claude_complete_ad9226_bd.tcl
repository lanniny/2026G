set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_name "design_1"

open_project $proj_file
open_bd_design "$proj_dir/adc_easy_test.srcs/sources_1/bd/$bd_name/$bd_name.bd"

update_compile_order -fileset sources_1
catch {update_module_reference [get_bd_cells ad9226test_0]}

set ps [get_bd_cells processing_system7_0]
set dma [get_bd_cells axi_dma_0]
set adc [get_bd_cells ad9226test_0]
set ctrl_smc [get_bd_cells axi_smc]

if {[llength $adc] == 0} {
    error "ad9226test_0 not found. Add the RTL module to the BD first."
}

set_property -dict [list CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100} CONFIG.PCW_CLK0_FREQ {100000000}] $ps
set_property CONFIG.NUM_MI 2 $ctrl_smc

set gpio [get_bd_cells -quiet axi_gpio_ad9226_ctrl]
if {[llength $gpio] == 0} {
    set gpio [create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_ad9226_ctrl]
}
set_property -dict [list \
    CONFIG.C_GPIO_WIDTH {1} \
    CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_IS_DUAL {1} \
    CONFIG.C_GPIO2_WIDTH {8} \
    CONFIG.C_ALL_INPUTS_2 {1} \
] $gpio

set axis_cc [get_bd_cells -quiet axis_clock_converter_ad9226]
if {[llength $axis_cc] == 0} {
    set axis_cc [create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_ad9226]
}

proc disconnect_intf_if_connected {pin_name} {
    set pin [get_bd_intf_pins -quiet $pin_name]
    if {[llength $pin] == 0} { return }
    set net [get_bd_intf_nets -quiet -of_objects $pin]
    if {[llength $net] != 0} {
        disconnect_bd_intf_net $net $pin
    }
}

proc disconnect_pin_if_connected {pin_name} {
    set pin [get_bd_pins -quiet $pin_name]
    if {[llength $pin] == 0} { return }
    set net [get_bd_nets -quiet -of_objects $pin]
    if {[llength $net] != 0} {
        disconnect_bd_net $net $pin
    }
}

foreach connection {
    {axi_smc/M01_AXI axi_gpio_ad9226_ctrl/S_AXI}
    {ad9226test_0/m_axis axis_clock_converter_ad9226/S_AXIS}
    {axis_clock_converter_ad9226/M_AXIS axi_dma_0/S_AXIS_S2MM}
} {
    set a [lindex $connection 0]
    set b [lindex $connection 1]
    disconnect_intf_if_connected $a
    disconnect_intf_if_connected $b
    connect_bd_intf_net [get_bd_intf_pins $a] [get_bd_intf_pins $b]
}

foreach pin {
    axi_gpio_ad9226_ctrl/s_axi_aclk
    axi_gpio_ad9226_ctrl/s_axi_aresetn
    axis_clock_converter_ad9226/s_axis_aclk
    axis_clock_converter_ad9226/s_axis_aresetn
    axis_clock_converter_ad9226/m_axis_aclk
    axis_clock_converter_ad9226/m_axis_aresetn
    axi_dma_0/m_axi_s2mm_aclk
    axi_s2mm_smc/aclk
    processing_system7_0/S_AXI_HP0_ACLK
    rst_s2mm_data/slowest_sync_clk
    rst_s2mm_data/ext_reset_in
    ad9226test_0/SYS_CLK_IN
    ad9226test_0/SYS_RST_N_IN
    ad9226test_0/ps_capture_start
    ad9226test_0/ps_status
} {
    disconnect_pin_if_connected $pin
}

connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] \
    [get_bd_pins axi_gpio_ad9226_ctrl/s_axi_aclk] \
    [get_bd_pins axis_clock_converter_ad9226/m_axis_aclk] \
    [get_bd_pins axi_dma_0/m_axi_s2mm_aclk] \
    [get_bd_pins axi_s2mm_smc/aclk] \
    [get_bd_pins processing_system7_0/S_AXI_HP0_ACLK] \
    [get_bd_pins ad9226test_0/SYS_CLK_IN]

connect_bd_net [get_bd_pins ad9226test_0/m_axis_aclk] \
    [get_bd_pins axis_clock_converter_ad9226/s_axis_aclk] \
    [get_bd_pins rst_s2mm_data/slowest_sync_clk]

connect_bd_net [get_bd_pins processing_system7_0/FCLK_RESET0_N] \
    [get_bd_pins rst_s2mm_data/ext_reset_in]

connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] \
    [get_bd_pins axi_gpio_ad9226_ctrl/s_axi_aresetn] \
    [get_bd_pins axis_clock_converter_ad9226/m_axis_aresetn] \
    [get_bd_pins ad9226test_0/SYS_RST_N_IN]

connect_bd_net [get_bd_pins rst_s2mm_data/peripheral_aresetn] \
    [get_bd_pins axis_clock_converter_ad9226/s_axis_aresetn]

connect_bd_net [get_bd_pins axi_gpio_ad9226_ctrl/gpio_io_o] \
    [get_bd_pins ad9226test_0/ps_capture_start]
connect_bd_net [get_bd_pins ad9226test_0/ps_status] \
    [get_bd_pins axi_gpio_ad9226_ctrl/gpio2_io_i]

catch {set_property FREQ_HZ 100000000 [get_bd_pins ad9226test_0/SYS_CLK_IN]}
catch {set_property FREQ_HZ 64000000 [get_bd_pins ad9226test_0/m_axis_aclk]}

assign_bd_address
set gpio_seg [get_bd_addr_segs -quiet /processing_system7_0/Data/SEG_axi_gpio_ad9226_ctrl_Reg]
if {[llength $gpio_seg] != 0} {
    set_property offset 0x41200000 $gpio_seg
    set_property range 64K $gpio_seg
}

validate_bd_design
save_bd_design
close_project
