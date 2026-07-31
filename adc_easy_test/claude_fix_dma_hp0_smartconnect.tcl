set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_file "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"
set axis_clk_hz 64000000

proc disconnect_pin_nets {pin_name} {
    foreach pin [get_bd_pins -quiet $pin_name] {
        foreach net [get_bd_nets -quiet -of_objects $pin] {
            catch {disconnect_bd_net $net $pin}
        }
    }
}

proc disconnect_intf_pin_nets {pin_name} {
    foreach pin [get_bd_intf_pins -quiet $pin_name] {
        foreach net [get_bd_intf_nets -quiet -of_objects $pin] {
            catch {disconnect_bd_intf_net $net $pin}
        }
    }
}

proc connect_net_once {src_pin dst_pin} {
    disconnect_pin_nets $dst_pin
    connect_bd_net [get_bd_pins $src_pin] [get_bd_pins $dst_pin]
}

proc connect_intf_once {src_pin dst_pin} {
    disconnect_intf_pin_nets $src_pin
    disconnect_intf_pin_nets $dst_pin
    connect_bd_intf_net [get_bd_intf_pins $src_pin] [get_bd_intf_pins $dst_pin]
}

open_project $proj_file
open_bd_design $bd_file

update_module_reference -quiet design_1_ad9248test_0_0

# PS: GP0 master controls AXI DMA registers; HP0 slave receives DMA writes into PS DDR.
set_property -dict [list \
    CONFIG.PCW_USE_M_AXI_GP0 {1} \
    CONFIG.PCW_USE_S_AXI_GP0 {0} \
    CONFIG.PCW_USE_S_AXI_HP0 {1} \
    CONFIG.PCW_USE_FABRIC_INTERRUPT {1} \
    CONFIG.PCW_IRQ_F2P_INTR {1} \
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {50} \
] [get_bd_cells processing_system7_0]

# Drive the module clock wizard from the external clock-capable SYS_CLK_IN pin.
if {[llength [get_bd_ports -quiet SYS_CLK_IN]] == 0} {
    create_bd_port -dir I -type clk SYS_CLK_IN
}
set_property CONFIG.FREQ_HZ 50000000 [get_bd_ports SYS_CLK_IN]
disconnect_pin_nets ad9248test_0/SYS_CLK_IN
foreach net [get_bd_nets -quiet -of_objects [get_bd_ports SYS_CLK_IN]] {
    catch {disconnect_bd_net $net [get_bd_ports SYS_CLK_IN]}
}
connect_bd_net [get_bd_ports SYS_CLK_IN] [get_bd_pins ad9248test_0/SYS_CLK_IN]

# Remove old AXIS clock converter/reset remnants if present.
disconnect_intf_pin_nets ad9248test_0/m_axis
disconnect_intf_pin_nets axi_dma_0/S_AXIS_S2MM
foreach cell {axis_clock_converter_0 rst_axis_stream} {
    if {[llength [get_bd_cells -quiet $cell]] > 0} {
        delete_bd_objs [get_bd_cells $cell]
    }
}
connect_bd_intf_net [get_bd_intf_pins ad9248test_0/m_axis] [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]

# Tell IP Integrator the module-generated AXIS clock is 64 MHz.
foreach obj [list \
    [get_bd_pins -quiet ad9248test_0/m_axis_aclk] \
    [get_bd_intf_pins -quiet ad9248test_0/m_axis] \
] {
    if {[llength $obj] > 0} {
        catch {set_property CONFIG.FREQ_HZ $axis_clk_hz $obj}
    }
}

# Control path stays on FCLK0.
foreach dst_pin {
    processing_system7_0/M_AXI_GP0_ACLK
    axi_dma_0/s_axi_lite_aclk
    axi_smc/aclk
    rst_ps7_0_50M/slowest_sync_clk
} {
    if {[llength [get_bd_pins -quiet $dst_pin]] > 0} {
        connect_net_once processing_system7_0/FCLK_CLK0 $dst_pin
    }
}
connect_net_once processing_system7_0/FCLK_RESET0_N rst_ps7_0_50M/ext_reset_in
connect_net_once rst_ps7_0_50M/peripheral_aresetn axi_smc/aresetn
connect_net_once rst_ps7_0_50M/peripheral_aresetn axi_dma_0/axi_resetn
connect_net_once rst_ps7_0_50M/peripheral_aresetn ad9248test_0/SYS_RST_N_IN

# Data path reset is synchronous to the 64 MHz AXIS/S2MM/HP0 clock.
if {[llength [get_bd_cells -quiet rst_s2mm_data]] == 0} {
    create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_s2mm_data
}
connect_net_once ad9248test_0/m_axis_aclk rst_s2mm_data/slowest_sync_clk
connect_net_once processing_system7_0/FCLK_RESET0_N rst_s2mm_data/ext_reset_in
connect_net_once rst_s2mm_data/peripheral_aresetn ad9248test_0/SYS_RST_N_IN

# Replace the stale dual-output AXI Interconnect with a single-output SmartConnect for DMA S2MM -> PS HP0.
disconnect_intf_pin_nets axi_dma_0/M_AXI_S2MM
disconnect_intf_pin_nets processing_system7_0/S_AXI_HP0
if {[llength [get_bd_cells -quiet axi_mem_intercon]] > 0} {
    delete_bd_objs [get_bd_cells axi_mem_intercon]
}
if {[llength [get_bd_cells -quiet axi_s2mm_smc]] == 0} {
    create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 axi_s2mm_smc
}
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] [get_bd_cells axi_s2mm_smc]
connect_bd_intf_net [get_bd_intf_pins axi_dma_0/M_AXI_S2MM] [get_bd_intf_pins axi_s2mm_smc/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_s2mm_smc/M00_AXI] [get_bd_intf_pins processing_system7_0/S_AXI_HP0]
connect_net_once ad9248test_0/m_axis_aclk axi_dma_0/m_axi_s2mm_aclk
connect_net_once ad9248test_0/m_axis_aclk processing_system7_0/S_AXI_HP0_ACLK
connect_net_once ad9248test_0/m_axis_aclk axi_s2mm_smc/aclk
connect_net_once rst_s2mm_data/peripheral_aresetn axi_s2mm_smc/aresetn

# Keep UART external interface name aligned with the active XDC.
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
