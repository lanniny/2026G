proc read_word {address} {
    return [mrd -value $address]
}

connect -url tcp:127.0.0.1:3121
targets -set -nocase -filter {name =~ "*A9*#0"}
configparams force-mem-access 1

set uart_clk [read_word 0xF8000154]
set aper_clk [read_word 0xF800012C]
set uart_cr [read_word 0xE0001000]
set uart_mr [read_word 0xE0001004]
set uart_brgr [read_word 0xE0001018]
set uart_sr [read_word 0xE000102C]
set uart_bdiv [read_word 0xE0001034]

puts [format "CPU_STATE=%s" [state]]
puts [format "UART_CLK_CTRL=0x%08X" $uart_clk]
puts [format "APER_CLK_CTRL=0x%08X" $aper_clk]
puts [format "UART1_CR=0x%08X" $uart_cr]
puts [format "UART1_MR=0x%08X" $uart_mr]
puts [format "UART1_BRGR=0x%08X" $uart_brgr]
puts [format "UART1_SR=0x%08X" $uart_sr]
puts [format "UART1_BDIV=0x%08X" $uart_bdiv]

if {($uart_clk & 0x2) == 0} {
    error "UART1 reference clock is disabled"
}
if {($aper_clk & 0x00200000) == 0} {
    error "UART1 APB clock is disabled"
}
if {($uart_cr & 0x14) != 0x14} {
    error "UART1 transmitter/receiver is not enabled"
}

configparams force-mem-access 0
disconnect
puts "UART1_RUNTIME_REGISTER_PASS"
