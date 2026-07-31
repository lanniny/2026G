connect -url tcp:127.0.0.1:3121
puts "=== TARGETS ==="
puts [targets]
targets -set -nocase -filter {name =~ "*A9*#0"}
puts "=== A9 STATUS ==="
puts [state]
puts "PC=[rrd pc]"
puts "UART0_CR=[mrd 0xE0000000]"
puts "UART0_MR=[mrd 0xE0000004]"
puts "UART0_BAUDGEN=[mrd 0xE0000018]"
puts "UART0_CSR=[mrd 0xE000002C]"
puts "UART1_CR=[mrd 0xE0001000]"
puts "UART1_MR=[mrd 0xE0001004]"
puts "UART1_BAUDGEN=[mrd 0xE0001018]"
puts "UART1_CSR=[mrd 0xE000102C]"
puts "HMI_TX_BYTES=[mrd 0x0014DD94]"
puts "HMI_RX_PACKETS=[mrd 0x0014DD98]"
puts "MEAS_FLAGS=[mrd 0x001181D4]"
puts "MEAS_MINMAX=[mrd 0x001181D8]"
puts "MEAS_CLIPPED=[mrd 0x001181DC]"
puts "MEAS_DC_FLOAT=[mrd 0x001181E0]"
puts "MEAS_FREQ_FLOAT=[mrd 0x001181E4]"
puts "MEAS_VPP_FLOAT=[mrd 0x001181E8]"
puts "MEAS_RMS_FLOAT=[mrd 0x001181EC]"
puts "DISPLAY_STATE=[mrd 0x0014DD70]"
puts "=== FLAG SAMPLES ==="
for {set i 0} {$i < 10} {incr i} {
    puts [mrd 0x001181D4]
    after 200
}
disconnect
