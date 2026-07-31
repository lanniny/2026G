open_hw_manager
connect_hw_server -url localhost:3121
set targets [get_hw_targets *]
puts "HW_TARGETS=$targets"
if {[llength $targets] == 0} {
    puts "ERROR_NO_HW_TARGETS"
    exit 1
}
foreach target $targets {
    puts "TARGET=$target"
    catch {puts "OLD_FREQ_[get_property NAME $target]=[get_property PARAM.FREQUENCY $target]"}
    catch {set_property PARAM.FREQUENCY 1000000 $target} set_msg
    puts "SET_FREQ_MSG=$set_msg"
    catch {puts "NEW_FREQ_[get_property NAME $target]=[get_property PARAM.FREQUENCY $target]"}
}
open_hw_target [lindex $targets 0]
set devices [get_hw_devices *]
puts "HW_DEVICES=$devices"
foreach dev $devices {
    puts "DEVICE=$dev PART=[get_property PART $dev] PROGRAMMED=[get_property PROGRAM.HW_CFGMEM $dev]"
}
puts "JTAG_FREQUENCY_SET_TO_1MHZ=OK"
close_hw_manager
