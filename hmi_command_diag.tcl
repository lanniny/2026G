proc uart1_status {} {
    return [mrd -value 0xE000102C]
}

proc uart1_drain_rx {} {
    set bytes {}
    while {([uart1_status] & 0x2) == 0} {
        lappend bytes [expr {[mrd -value 0xE0001030] & 0xFF}]
    }
    return $bytes
}

proc uart1_send_command {command} {
    binary scan $command c* bytes
    foreach byte $bytes {
        while {([uart1_status] & 0x10) != 0} {
            after 1
        }
        mwr 0xE0001030 [expr {$byte & 0xFF}]
    }
    foreach byte {255 255 255} {
        while {([uart1_status] & 0x10) != 0} {
            after 1
        }
        mwr 0xE0001030 $byte
    }
    while {([uart1_status] & 0x8) == 0} {
        after 1
    }
}

connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~ "*A9*#0"}
catch {stop}
configparams force-mem-access 1

puts "RX_BEFORE=[uart1_drain_rx]"
uart1_send_command "bkcmd=3"
after 100
puts "ACK_BKCMD=[uart1_drain_rx]"

uart1_send_command "page 0"
after 100
puts "ACK_PAGE=[uart1_drain_rx]"

for {set font 0} {$font < 16} {incr font} {
    uart1_send_command [format {xstr 607,102,135,35,%d,65535,0,1,1,1,"123.45"} $font]
    after 50
    puts "ACK_FONT_${font}=[uart1_drain_rx]"
}

uart1_send_command "bkcmd=0"
after 50
puts "ACK_FINAL=[uart1_drain_rx]"

con
configparams force-mem-access 0
disconnect
puts "HMI_COMMAND_DIAGNOSTIC_COMPLETE"
