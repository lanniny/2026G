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

proc disconnect_intf_pin_nets {pin_name} {
    set pins [get_bd_intf_pins -quiet $pin_name]
    foreach pin $pins {
        set nets [get_bd_intf_nets -quiet -of_objects $pin]
        foreach net $nets { catch {disconnect_bd_intf_net $net $pin} }
    }
}

proc connect_to_fclk {pin_name} {
    set pins [get_bd_pins -quiet $pin_name]
    if {[llength $pins] > 0} {
        disconnect_pin_nets $pin_name
        connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] $pins
    }
}

open_project $proj_file
open_bd_design $bd_file

catch {update_module_reference -verbose design_1_ad9248test_0_0}

# Use PS FCLK0 as the ad9248test input clock. This keeps the clk_wiz input at 50 MHz when FCLK0 is configured to 50 MHz.
if {[llength [get_bd_ports -quiet SYS_CLK_IN]] > 0} {
    disconnect_pin_nets SYS_CLK_IN
    delete_bd_objs [get_bd_ports SYS_CLK_IN]
}
disconnect_pin_nets ad9248test_0/SYS_CLK_IN
connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] [get_bd_pins ad9248test_0/SYS_CLK_IN]

# Remove the unused AXI-Stream clock converter/FIFO and its reset block.
disconnect_intf_pin_nets ad9248test_0/m_axis
disconnect_intf_pin_nets axi_dma_0/S_AXIS_S2MM
foreach cell {axis_clock_converter_0 rst_axis_stream} {
    if {[llength [get_bd_cells -quiet $cell]] > 0} {
        delete_bd_objs [get_bd_cells $cell]
    }
}
connect_bd_intf_net [get_bd_intf_pins ad9248test_0/m_axis] [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]

# Keep PS GP0 master for DMA control and HP0 slave for DMA writes into DDR.
set_property -dict [list CONFIG.PCW_USE_M_AXI_GP0 {1} CONFIG.PCW_USE_S_AXI_GP0 {0} CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {50}] [get_bd_cells processing_system7_0]

# Reconnect DMA memory-mapped write path to HP0.
disconnect_intf_pin_nets axi_mem_intercon/M00_AXI
disconnect_intf_pin_nets processing_system7_0/S_AXI_GP0
disconnect_intf_pin_nets processing_system7_0/S_AXI_HP0
connect_bd_intf_net [get_bd_intf_pins axi_mem_intercon/M00_AXI] [get_bd_intf_pins processing_system7_0/S_AXI_HP0]

# Clocks: control path uses FCLK0; DMA S2MM/HP data path uses the ad9248test AXIS clock sys_main_clk.
connect_to_fclk processing_system7_0/M_AXI_GP0_ACLK
connect_to_fclk axi_dma_0/s_axi_lite_aclk
connect_to_fclk axi_smc/aclk
connect_to_fclk rst_ps7_0_50M/slowest_sync_clk

disconnect_pin_nets axi_dma_0/m_axi_s2mm_aclk
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins axi_dma_0/m_axi_s2mm_aclk]
disconnect_pin_nets processing_system7_0/S_AXI_HP0_ACLK
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins processing_system7_0/S_AXI_HP0_ACLK]
disconnect_pin_nets axi_mem_intercon/ACLK
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins axi_mem_intercon/ACLK]
disconnect_pin_nets axi_mem_intercon/S00_ACLK
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins axi_mem_intercon/S00_ACLK]
disconnect_pin_nets axi_mem_intercon/M00_ACLK
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins axi_mem_intercon/M00_ACLK]

# Resets: keep aresetn from PS reset block. ad9248test reset also remains from PS reset block.
disconnect_pin_nets axi_dma_0/axi_resetn
connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins axi_dma_0/axi_resetn]
disconnect_pin_nets axi_mem_intercon/ARESETN
connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins axi_mem_intercon/ARESETN]
disconnect_pin_nets axi_mem_intercon/S00_ARESETN
connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins axi_mem_intercon/S00_ARESETN]
disconnect_pin_nets axi_mem_intercon/M00_ARESETN
connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins axi_mem_intercon/M00_ARESETN]
disconnect_pin_nets ad9248test_0/SYS_RST_N_IN
connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins ad9248test_0/SYS_RST_N_IN]

# Keep UART external interface name aligned with XDC.
if {[llength [get_bd_intf_ports -quiet UART_0_0]] > 0 && [llength [get_bd_intf_ports -quiet UART_1_0]] == 0} {
    set_property name UART_1_0 [get_bd_intf_ports UART_0_0]
}

assign_bd_address
validate_bd_design
save_bd_design
make_wrapper -files [get_files $bd_file] -top -import
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1
close_project
