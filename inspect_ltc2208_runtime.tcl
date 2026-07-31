proc read_word {address} {
    return [format "0x%08X" [mrd -value $address]]
}

connect
targets -set -nocase -filter {name =~ "*A9*#0"}
configparams force-mem-access 1

for {set index 0} {$index < 5} {incr index} {
    puts [format "SNAPSHOT_%d PC=%s PL=%s DMA_CR=%s DMA_SR=%s FLAGS=%s MINMAX=%s CLIPPED=%s DC=%s FREQ=%s VPP=%s RMS=%s DISPLAY_INIT=%s HMI_TX=%s HMI_RX=%s" \
        $index \
        [rrd pc] \
        [read_word 0x41200008] \
        [read_word 0x40400030] \
        [read_word 0x40400034] \
        [read_word 0x0011C194] \
        [read_word 0x0011C198] \
        [read_word 0x0011C19C] \
        [read_word 0x0011C1A0] \
        [read_word 0x0011C1A4] \
        [read_word 0x0011C1A8] \
        [read_word 0x0011C1AC] \
        [read_word 0x00151D30] \
        [read_word 0x00151D54] \
        [read_word 0x00151D58]]
    after 250
}

puts "ADC_SAMPLES_BEGIN"
puts [mrd 0x0011D500 32]
puts "ADC_SAMPLES_END"
puts "DMA_WORDS_BEGIN"
puts [mrd 0x00121C80 16]
puts "DMA_WORDS_END"
puts [format "UART_CLK_CTRL=%s" [read_word 0xF8000154]]
puts [format "APER_CLK_CTRL=%s" [read_word 0xF800012C]]
puts "LTC2208_RUNTIME_SNAPSHOT_PASS"
configparams force-mem-access 0
disconnect
