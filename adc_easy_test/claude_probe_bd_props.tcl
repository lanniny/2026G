set proj_dir "D:/FPGAPROJECT/ZYNQ/adc_easy_test"
set proj_file "$proj_dir/adc_easy_test.xpr"
set bd_file "$proj_dir/adc_easy_test.srcs/sources_1/bd/design_1/design_1.bd"

open_project $proj_file
open_bd_design $bd_file

foreach obj_name {
    axi_mem_intercon
    axi_mem_intercon/S00_AXI
    axi_mem_intercon/M00_AXI
    axi_mem_intercon/M01_AXI
    axi_mem_intercon/xbar
    axi_mem_intercon/xbar/S00_AXI
    axi_mem_intercon/xbar/M00_AXI
    axi_mem_intercon/m00_couplers/auto_pc/S_AXI
    axi_dma_0/M_AXI_S2MM
    processing_system7_0/S_AXI_HP0
} {
    set objs [get_bd_cells -quiet $obj_name]
    if {[llength $objs] == 0} { set objs [get_bd_intf_pins -quiet $obj_name] }
    puts "\n=== $obj_name ==="
    if {[llength $objs] == 0} {
        puts "NOT_FOUND"
    } else {
        foreach obj $objs {
            foreach prop {CONFIG.NUM_MI CONFIG.NUM_SI CONFIG.READ_WRITE_MODE CONFIG.PROTOCOL CONFIG.DATA_WIDTH CONFIG.FREQ_HZ CONFIG.CLK_DOMAIN CONFIG.M00_S00_READ_CONNECTIVITY CONFIG.M00_S00_WRITE_CONNECTIVITY} {
                if {![catch {set val [get_property $prop $obj]}]} {
                    puts "$prop=$val"
                }
            }
        }
    }
}

close_project
