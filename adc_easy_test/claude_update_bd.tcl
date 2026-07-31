set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_file "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"

proc disconnect_pin_nets {pin_name} {
    set pins [get_bd_pins -quiet $pin_name]
    foreach pin $pins {
        set nets [get_bd_nets -quiet -of_objects $pin]
        foreach net $nets {
            catch {disconnect_bd_net $net $pin}
        }
    }
}

proc disconnect_intf_pin_nets {pin_name} {
    set pins [get_bd_intf_pins -quiet $pin_name]
    foreach pin $pins {
        set nets [get_bd_intf_nets -quiet -of_objects $pin]
        foreach net $nets {
            catch {disconnect_bd_intf_net $net $pin}
        }
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

if {[llength [get_files -quiet $bd_file]] == 0} {
    add_files -fileset sources_1 $bd_file
}
catch {set_property IS_ENABLED true [get_files $bd_file]}
catch {set_property is_enabled true [get_files $bd_file]}

open_bd_design $bd_file
update_compile_order -fileset sources_1

if {[llength [get_bd_cells -quiet ad9248test_0]] == 0} {
    error "ad9248test_0 not found in design_1.bd"
}

catch {update_module_reference -verbose design_1_ad9248test_0_0}

if {[llength [get_bd_pins -quiet ad9248test_0/m_axis_aclk]] == 0} {
    error "ad9248test_0/m_axis_aclk not found after refreshing module reference"
}

if {[llength [get_bd_intf_ports -quiet UART_0_0]] > 0 && [llength [get_bd_intf_ports -quiet UART_1_0]] == 0} {
    set_property name UART_1_0 [get_bd_intf_ports UART_0_0]
}

# Preserve board SYS_CLK_IN as the ADC clock source. Do not drive it from PS FCLK.
disconnect_pin_nets ad9248test_0/SYS_CLK_IN
if {[llength [get_bd_ports -quiet SYS_CLK_IN]] == 0} {
    create_bd_port -dir I -type clk SYS_CLK_IN
}
set_property CONFIG.FREQ_HZ 50000000 [get_bd_ports SYS_CLK_IN]
connect_bd_net [get_bd_ports SYS_CLK_IN] [get_bd_pins ad9248test_0/SYS_CLK_IN]

# Enable HP0 for PL-to-PS DDR writes. Keep GP0 master for DMA AXI-Lite control.
disconnect_intf_pin_nets processing_system7_0/S_AXI_GP0
disconnect_pin_nets processing_system7_0/S_AXI_GP0_ACLK
set_property -dict [list CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_USE_S_AXI_GP0 {0}] [get_bd_cells processing_system7_0]

if {[llength [get_bd_intf_pins -quiet processing_system7_0/S_AXI_HP0]] == 0} {
    error "processing_system7_0/S_AXI_HP0 not available after enabling HP0"
}

disconnect_intf_pin_nets axi_mem_intercon/M00_AXI
connect_bd_intf_net [get_bd_intf_pins axi_mem_intercon/M00_AXI] [get_bd_intf_pins processing_system7_0/S_AXI_HP0]
connect_to_fclk processing_system7_0/S_AXI_HP0_ACLK

# Insert an AXI-Stream clock converter so ADC sampling/sys_main_clk domain is preserved.
# Source side is ad9248test_0/m_axis_aclk; DMA/HP side remains PS FCLK0.
disconnect_intf_pin_nets ad9248test_0/m_axis
disconnect_intf_pin_nets axi_dma_0/S_AXIS_S2MM

if {[llength [get_bd_cells -quiet axis_clock_converter_0]] == 0} {
    create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_0
}

connect_bd_intf_net [get_bd_intf_pins ad9248test_0/m_axis] [get_bd_intf_pins axis_clock_converter_0/S_AXIS]
connect_bd_intf_net [get_bd_intf_pins axis_clock_converter_0/M_AXIS] [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]

disconnect_pin_nets axis_clock_converter_0/s_axis_aclk
connect_bd_net [get_bd_pins ad9248test_0/m_axis_aclk] [get_bd_pins axis_clock_converter_0/s_axis_aclk]
connect_to_fclk axis_clock_converter_0/m_axis_aclk

if {[llength [get_bd_pins -quiet axis_clock_converter_0/s_axis_aresetn]] > 0} {
    disconnect_pin_nets axis_clock_converter_0/s_axis_aresetn
    connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins axis_clock_converter_0/s_axis_aresetn]
}
if {[llength [get_bd_pins -quiet axis_clock_converter_0/m_axis_aresetn]] > 0} {
    disconnect_pin_nets axis_clock_converter_0/m_axis_aresetn
    connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] [get_bd_pins axis_clock_converter_0/m_axis_aresetn]
}

# Keep DMA and HP memory-map side in FCLK0 domain.
connect_to_fclk axi_dma_0/s_axis_s2mm_aclk
connect_to_fclk axi_dma_0/m_axi_s2mm_aclk
connect_to_fclk axi_mem_intercon/ACLK
connect_to_fclk axi_mem_intercon/S00_ACLK
connect_to_fclk axi_mem_intercon/M00_ACLK

# Keep control path in FCLK0 domain.
connect_to_fclk processing_system7_0/M_AXI_GP0_ACLK
connect_to_fclk axi_dma_0/s_axi_lite_aclk
connect_to_fclk axi_smc/aclk
connect_to_fclk rst_ps7_0_50M/slowest_sync_clk

assign_bd_address
validate_bd_design
save_bd_design

set wrapper_file "$proj_dir/adc_easy_test.gen/sources_1/bd/design_1/hdl/design_1_wrapper.v"
catch {make_wrapper -files [get_files $bd_file] -top -import}
if {[file exists $wrapper_file] && [llength [get_files -quiet $wrapper_file]] == 0} {
    add_files -norecurse $wrapper_file
}
catch {set_property IS_ENABLED true [get_files $wrapper_file]}
catch {set_property is_enabled true [get_files $wrapper_file]}
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

save_project
close_project
