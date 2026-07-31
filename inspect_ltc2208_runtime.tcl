proc read_word {address} {
    return [format "0x%08X" [mrd -value $address]]
}

connect
targets -set -nocase -filter {name =~ "xc7z020"}

for {set index 0} {$index < 5} {incr index} {
    puts [format "SNAPSHOT_%d PL=%s DMA_CR=%s DMA_SR=%s FLAGS=%s MINMAX=%s CLIPPED=%s DC=%s FREQ=%s VPP=%s RMS=%s COUNT=%s" \
        $index \
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
        [read_word 0x0011C1C0]]
    after 250
}

puts "ADC_SAMPLES_BEGIN"
puts [mrd 0x0011D500 32]
puts "ADC_SAMPLES_END"
puts "DMA_WORDS_BEGIN"
puts [mrd 0x00121C80 16]
puts "DMA_WORDS_END"
puts "LTC2208_RUNTIME_SNAPSHOT_PASS"
disconnect

