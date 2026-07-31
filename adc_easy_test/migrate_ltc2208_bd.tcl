# Replace the legacy dual-AD9226 module reference with the single-channel
# LTC2208 capture block while preserving the verified AXI DMA/PS topology.

set old_cell [get_bd_cells -quiet ad9226test_0]
set new_cell [get_bd_cells -quiet ltc2208_capture_0]

if {[llength $old_cell] != 0 || [llength $new_cell] == 0} {
    foreach port_name {
        ADC_CLK_COMMON_OUT1_0 ADC_CLK_COMMON_OUT2_0
        ADC_DATA_IN_PIN1_0 ADC_DATA_IN_PIN2_0
        LTC2208_CKI LTC2208_SHDN LTC2208_CKO LTC2208_OFA LTC2208_DATA
    } {
        set port_obj [get_bd_ports -quiet $port_name]
        if {[llength $port_obj] != 0} {
            delete_bd_objs $port_obj
        }
    }

    foreach cell_obj [concat $old_cell $new_cell] {
        if {[llength $cell_obj] != 0} {
            delete_bd_objs $cell_obj
        }
    }

    update_compile_order -fileset sources_1
    set capture_cell [create_bd_cell -type module \
        -reference ltc2208_capture ltc2208_capture_0]

    connect_bd_net [get_bd_pins processing_system7_0/FCLK_CLK0] \
        [get_bd_pins $capture_cell/SYS_CLK_IN]
    connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] \
        [get_bd_pins $capture_cell/SYS_RST_N_IN]
    connect_bd_net [get_bd_pins axi_gpio_ad9226_ctrl/gpio_io_o] \
        [get_bd_pins $capture_cell/ps_capture_start]
    connect_bd_net [get_bd_pins axi_gpio_ad9226_ctrl/gpio2_io_i] \
        [get_bd_pins $capture_cell/ps_status]

    connect_bd_intf_net [get_bd_intf_pins $capture_cell/m_axis] \
        [get_bd_intf_pins axis_clock_converter_ad9226/S_AXIS]
    connect_bd_net [get_bd_pins $capture_cell/m_axis_aclk] \
        [get_bd_pins axis_clock_converter_ad9226/s_axis_aclk] \
        [get_bd_pins rst_s2mm_data/slowest_sync_clk]

    create_bd_port -dir O LTC2208_CKI
    create_bd_port -dir O LTC2208_SHDN
    create_bd_port -dir I LTC2208_CKO
    create_bd_port -dir I LTC2208_OFA
    create_bd_port -dir I -from 15 -to 0 LTC2208_DATA

    connect_bd_net [get_bd_ports LTC2208_CKI] \
        [get_bd_pins $capture_cell/LTC2208_CKI]
    connect_bd_net [get_bd_ports LTC2208_SHDN] \
        [get_bd_pins $capture_cell/LTC2208_SHDN]
    connect_bd_net [get_bd_ports LTC2208_CKO] \
        [get_bd_pins $capture_cell/LTC2208_CKO]
    connect_bd_net [get_bd_ports LTC2208_OFA] \
        [get_bd_pins $capture_cell/LTC2208_OFA]
    connect_bd_net [get_bd_ports LTC2208_DATA] \
        [get_bd_pins $capture_cell/LTC2208_DATA]

    puts "LTC2208_BD_MIGRATION=APPLIED"
} else {
    puts "LTC2208_BD_MIGRATION=ALREADY_APPLIED"
}

