# Migrate the verified design to the LTC2208, regenerate the Block Design,
# rebuild the bitstream, sign off timing, and export an XSA.  Run with:
#   vivado -mode batch -source build_4msps.tcl

set proj_dir [file normalize [file dirname [info script]]]
set proj_file [file join $proj_dir adc_easy_test.xpr]
set xsa_file [file join $proj_dir design_1_wrapper.xsa]
set report_dir [file join $proj_dir timing_ltc2208_4msps]
set vivado_wrapper [file normalize \
    [file join $proj_dir .. .. .. firmware_ps jtag vivado.ps1]]

# This machine still has a 2018 RemoteSolverDispatcher service.  A migrated
# project can inherit cluster-run preferences and leave all local runs parked
# in *.queue.rst forever.  Force deterministic local process execution.
set_param runs.enableClusterConf false
set_param runs.monitorLSFJobs false

proc run_generated_vivado_run {run_name} {
    global proj_dir vivado_wrapper

    set run_objects [get_runs -quiet $run_name]
    if {[llength $run_objects] != 1} {
        error "Expected one Vivado run named $run_name, found [llength $run_objects]"
    }
    set run_dir [get_property DIRECTORY [lindex $run_objects 0]]
    set run_scripts [glob -nocomplain -directory $run_dir *.tcl]
    if {[llength $run_scripts] != 1} {
        error "Expected one generated Tcl script for $run_name, found [llength $run_scripts]"
    }
    if {![file exists $vivado_wrapper]} {
        error "Vivado runtime wrapper is missing: $vivado_wrapper"
    }

    foreach marker {.vivado.begin.rst .vivado.end.rst .vivado.error.rst} {
        file delete -force [file join $run_dir $marker]
    }
    close [open [file join $run_dir .vivado.begin.rst] w]

    puts "RUN_LOCAL_BEGIN=$run_name"
    set saved_dir [pwd]
    cd $run_dir
    set child_runtime [file normalize \
        [file join $proj_dir .. tmp "vivado-child-$run_name"]]
    set child_log [file join $run_dir "$run_name.child.log"]
    set powershell_exe [file normalize \
        [file join $::env(SystemRoot) System32 WindowsPowerShell v1.0 powershell.exe]]
    set child_command [list $powershell_exe -NoProfile -ExecutionPolicy Bypass \
        -File $vivado_wrapper -Script [lindex $run_scripts 0] \
        -RuntimeRoot $child_runtime]
    set run_failed [catch {exec {*}$child_command > $child_log 2>@1} run_output]
    cd $saved_dir
    if {$run_failed} {
        close [open [file join $run_dir .vivado.error.rst] w]
        error "Generated Vivado run failed: $run_name; see $child_log ($run_output)"
    }

    set log_handle [open $child_log r]
    set log_text [read $log_handle]
    close $log_handle
    set fatal_log_lines [regexp -all -inline -line \
        {^(?:ERROR|CRITICAL WARNING):.*$} $log_text]
    if {[llength $fatal_log_lines] != 0} {
        close [open [file join $run_dir .vivado.error.rst] w]
        error "Vivado run $run_name emitted fatal diagnostics: [join $fatal_log_lines { | }]"
    }

    close [open [file join $run_dir .vivado.end.rst] w]
    puts "RUN_LOCAL_COMPLETE=$run_name"
}

set opened_project_here 0
if {[llength [get_projects -quiet]] == 0} {
    open_project $proj_file
    set opened_project_here 1
} else {
    set current_proj_file [file normalize [get_property DIRECTORY [current_project]]]
    if {$current_proj_file ne $proj_dir} {
        error "A different project is open: $current_proj_file"
    }
}
set_property top design_1_wrapper [current_fileset]

# The LTC2208 XDC constrains package pins, the forwarded output clock, input
# delays, and post-link CDC endpoints inside an OOC module reference.  Reading
# it during top-level synthesis sees unresolved black boxes; defer it until
# implementation has linked the complete hierarchy.
set ltc2208_xdc_files [get_files -quiet */ad9226x.xdc]
if {[llength $ltc2208_xdc_files] != 1} {
    error "Expected one LTC2208 XDC, found [llength $ltc2208_xdc_files]"
}
set ltc2208_xdc [lindex $ltc2208_xdc_files 0]
set_property USED_IN_SYNTHESIS false $ltc2208_xdc
set_property USED_IN_IMPLEMENTATION true $ltc2208_xdc
set_property PROCESSING_ORDER LATE $ltc2208_xdc
puts "LTC2208_XDC_MODE synthesis=[get_property USED_IN_SYNTHESIS $ltc2208_xdc] implementation=[get_property USED_IN_IMPLEMENTATION $ltc2208_xdc] order=[get_property PROCESSING_ORDER $ltc2208_xdc]"

set bd_files [get_files -quiet */design_1.bd]
if {[llength $bd_files] != 1} {
    error "Expected one design_1.bd, found [llength $bd_files]"
}
set bd_file [lindex $bd_files 0]

open_bd_design $bd_file
update_compile_order -fileset sources_1
source [file join $proj_dir migrate_ltc2208_bd.tcl]
set syntax_result [check_syntax -fileset sources_1 -return_string]
puts "RTL_SYNTAX_CHECK=$syntax_result"

# The analyzer sends a 32 KiB AXI-stream frame.  Run S2MM in cut-through
# mode so it can continuously drain the clock converter into DDR instead of
# depending on the optional internal Store-and-Forward FIFO.
set_property CONFIG.c_include_s2mm_sf 0 [get_bd_cells axi_dma_0]
if {[get_property CONFIG.c_include_s2mm_sf [get_bd_cells axi_dma_0]] != 0} {
    error "Failed to disable AXI DMA S2MM Store-and-Forward"
}

# axi_s2mm_smc runs on FCLK_CLK0 (100 MHz), so its reset must also be
# synchronized to that domain.  rst_s2mm_data is synchronized to the
# LTC2208/AXIS source clock (64 MHz) and is only for the S side of the AXIS
# clock converter.
set s2mm_smc_reset_pin [get_bd_pins axi_s2mm_smc/aresetn]
foreach old_reset_net [get_bd_nets -quiet -of_objects $s2mm_smc_reset_pin] {
    disconnect_bd_net $old_reset_net $s2mm_smc_reset_pin
}
connect_bd_net [get_bd_pins rst_ps7_0_50M/peripheral_aresetn] \
    $s2mm_smc_reset_pin

validate_bd_design
save_bd_design

reset_target all $bd_file
generate_target all $bd_file
export_ip_user_files -of_objects $bd_file -no_script -sync -force -quiet
update_compile_order -fileset sources_1

# Target regeneration can remove a stale OOC run without recreating the run
# for its current XCI.  Make both safety-critical OOC runs explicit and
# idempotent before enforcing the no-stale-DCP gate below.
proc ensure_ooc_run {run_pattern xci_pattern label} {
    if {[llength [get_runs -quiet $run_pattern]] != 0} {
        return
    }

    set xci_files [get_files -quiet $xci_pattern]
    if {[llength $xci_files] != 1} {
        error "Expected one generated $label XCI, found [llength $xci_files]"
    }
    puts "CREATE_OOC_RUN_FOR_$label=[lindex $xci_files 0]"
    create_ip_run [lindex $xci_files 0]
}

ensure_ooc_run *ltc2208_capture*_synth_1 \
    */design_1_ltc2208_capture_0_0.xci LTC2208
ensure_ooc_run *processing_system7*_synth_1 \
    */design_1_processing_system7_0_0.xci PS7

# Do not allow Vivado to reuse the May 1 module-reference or PS7 checkpoints.
# DMA/SmartConnect artifacts remain protected by Vivado's content-addressed
# IP cache after their current BD properties are read back above.
set required_ooc_patterns {
    *ltc2208_capture*_synth_1
    *processing_system7*_synth_1
}
set required_ooc_run_names {}
foreach run_pattern $required_ooc_patterns {
    set matching_runs [get_runs -quiet $run_pattern]
    if {[llength $matching_runs] == 0} {
        error "Required OOC run pattern not found: $run_pattern"
    }
    foreach run_obj $matching_runs {
        lappend required_ooc_run_names [get_property NAME $run_obj]
    }
}
set required_ooc_run_names [lsort -unique $required_ooc_run_names]
foreach run_name $required_ooc_run_names {
    puts "RESET_OOC_RUN=$run_name"
    reset_run [get_runs $run_name]
}

reset_run synth_1

# Generate the standard Vivado run scripts, then execute them locally in a
# deterministic order.  launch_runs itself is unusable on this machine: its
# dispatcher leaves runs parked in *.queue.rst and the generated batch file
# selects a broken vendor wrapper.
launch_runs synth_1 -jobs 1 -scripts_only
foreach run_name $required_ooc_run_names {
    run_generated_vivado_run $run_name
}
run_generated_vivado_run [get_runs synth_1]

# Reopen the project so Vivado reloads the begin/end markers and DCP status
# created by the generated run scripts.
close_project
open_project $proj_file
set synth_status [get_property STATUS [get_runs synth_1]]
puts "SYNTH_STATUS=$synth_status"
if {[string first "Complete" $synth_status] < 0} {
    error "synth_1 failed: $synth_status"
}
foreach run_name $required_ooc_run_names {
    set ooc_status [get_property STATUS [get_runs $run_name]]
    puts "OOC_STATUS $run_name=$ooc_status"
    if {[string first "Complete" $ooc_status] < 0} {
        error "OOC synthesis failed or was not rebuilt: $run_name ($ooc_status)"
    }
}

# Prove that the LTC2208 OOC checkpoint is newer than the edited RTL.  A new
# top-level bitstream timestamp alone is not sufficient because Vivado can
# otherwise link an old OOC DCP into it.
set adc_rtl_file [file join $proj_dir \
    adc_easy_test.srcs sources_1 new ad9226test.v]
set adc_ooc_run [lindex [get_runs -quiet *ltc2208_capture*_synth_1] 0]
set adc_ooc_dir [get_property DIRECTORY $adc_ooc_run]
set adc_ooc_dcps [glob -nocomplain -directory $adc_ooc_dir *.dcp]
if {[llength $adc_ooc_dcps] != 1} {
    error "Expected one LTC2208 OOC DCP in $adc_ooc_dir"
}
set adc_ooc_dcp [lindex $adc_ooc_dcps 0]
puts "ADC_RTL_MTIME=[clock format [file mtime $adc_rtl_file]]"
puts "ADC_OOC_DCP_MTIME=[clock format [file mtime $adc_ooc_dcp]]"
if {[file mtime $adc_ooc_dcp] < [file mtime $adc_rtl_file]} {
    error "LTC2208 OOC DCP is older than ad9226test.v"
}

reset_run impl_1
launch_runs impl_1 -to_step write_bitstream -jobs 1 -scripts_only
run_generated_vivado_run [get_runs impl_1]
close_project
open_project $proj_file
set impl_status [get_property STATUS [get_runs impl_1]]
puts "IMPL_STATUS=$impl_status"
if {[string first "Complete" $impl_status] < 0} {
    error "impl_1 failed: $impl_status"
}

file mkdir $report_dir
open_run impl_1

# Timing sign-off reports for the LTC2208 source-synchronous interface.
report_timing_summary -delay_type min_max -report_unconstrained \
    -check_timing_verbose -max_paths 20 \
    -file [file join $report_dir timing_summary.rpt]
report_timing -delay_type max -max_paths 20 -nworst 1 \
    -from [get_ports {LTC2208_DATA[*]}] \
    -file [file join $report_dir ltc2208_input_setup.rpt]
report_timing -delay_type min -max_paths 20 -nworst 1 \
    -from [get_ports {LTC2208_DATA[*]}] \
    -file [file join $report_dir ltc2208_input_hold.rpt]
set methodology_report [file join $report_dir methodology.rpt]
set cdc_report [file join $report_dir cdc.rpt]
report_methodology -file $methodology_report
report_cdc -details -file $cdc_report

set methodology_handle [open $methodology_report r]
set methodology_text [read $methodology_handle]
close $methodology_handle
if {![regexp {Checks found:\s+0} $methodology_text]} {
    error "Vivado methodology checks remain; see $methodology_report"
}

set cdc_handle [open $cdc_report r]
set cdc_text [read $cdc_handle]
close $cdc_handle
if {[regexp {CDC-[0-9]+\s+Critical} $cdc_text]} {
    error "Critical CDC findings remain; see $cdc_report"
}

if {[llength [get_clocks -quiet ltc2208_fwd_clk]] != 1} {
    error "Forwarded clock ltc2208_fwd_clk was not created by ad9226x.xdc"
}

set adc1_paths [get_timing_paths -delay_type max -max_paths 20 -nworst 1 \
    -from [get_ports {LTC2208_DATA[*]}]]
if {[llength $adc1_paths] < 16} {
    error "Expected 16 timed LTC2208 input paths, found [llength $adc1_paths]"
}

# These exact counts keep the CDC exceptions narrow.  A hierarchy/name drift
# must fail the build instead of silently leaving an unsafe crossing or
# accidentally cutting unrelated logic.
set start_meta_d [get_pins -quiet -hierarchical -filter \
    {NAME =~ */start_meta_reg/D}]
puts "CDC_ENDPOINT_COUNTS start=[llength $start_meta_d]"
if {[llength $start_meta_d] != 1} {
    error "Unexpected LTC2208 CDC endpoint count"
}

set pin_checks {
    {LTC2208_SHDN W22} {LTC2208_CKI V22} {LTC2208_CKO Y20}
    {LTC2208_OFA Y13}
    {{LTC2208_DATA[0]} Y21} {{LTC2208_DATA[1]} AA22}
    {{LTC2208_DATA[2]} AB22} {{LTC2208_DATA[3]} AA21}
    {{LTC2208_DATA[4]} AB21} {{LTC2208_DATA[5]} AB20}
    {{LTC2208_DATA[6]} AB19} {{LTC2208_DATA[7]} Y19}
    {{LTC2208_DATA[8]} AA19} {{LTC2208_DATA[9]} AA16}
    {{LTC2208_DATA[10]} AB16} {{LTC2208_DATA[11]} AA18}
    {{LTC2208_DATA[12]} Y18} {{LTC2208_DATA[13]} AB15}
    {{LTC2208_DATA[14]} AB14} {{LTC2208_DATA[15]} AA13}
}
foreach pin_check $pin_checks {
    lassign $pin_check port_name expected_pin
    set actual_pin [get_property PACKAGE_PIN [get_ports $port_name]]
    puts "PIN_CHECK $port_name=$actual_pin"
    if {$actual_pin ne $expected_pin} {
        error "Pin mismatch for $port_name: expected $expected_pin, got $actual_pin"
    }
}

set worst_setup_path [lindex [get_timing_paths -delay_type max \
    -max_paths 1 -nworst 1] 0]
set worst_hold_path [lindex [get_timing_paths -delay_type min \
    -max_paths 1 -nworst 1] 0]
set wns [get_property SLACK $worst_setup_path]
set whs [get_property SLACK $worst_hold_path]
puts "FINAL_WNS_NS=$wns"
puts "FINAL_WHS_NS=$whs"

if {$wns < 0.0 || $whs < 0.0} {
    error "Timing sign-off failed: WNS=$wns ns, WHS=$whs ns"
}

write_hw_platform -fixed -include_bit -force -file $xsa_file
puts "XSA_OUTPUT=$xsa_file"
puts "TIMING_REPORT_DIR=$report_dir"
if {$opened_project_here} {
    close_project
}
