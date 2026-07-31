set_property PACKAGE_PIN M17 [get_ports UART_1_0_rxd]
set_property PACKAGE_PIN L17 [get_ports UART_1_0_txd]
set_property IOSTANDARD LVCMOS33 [get_ports UART_1_0_rxd]
set_property IOSTANDARD LVCMOS33 [get_ports UART_1_0_txd]

# Dedicated USART-HMI screen link on ordinary BANK35 PL I/O.
#   Zynq H19 (TX) -> screen RX
#   Zynq H20 (RX) <- screen TX
# UART0 on L17/M17 remains available for the board USB debug serial port.
set_property PACKAGE_PIN H19 [get_ports HMI_UART_0_txd]
set_property PACKAGE_PIN H20 [get_ports HMI_UART_0_rxd]
set_property IOSTANDARD LVCMOS33 [get_ports HMI_UART_0_txd]
set_property IOSTANDARD LVCMOS33 [get_ports HMI_UART_0_rxd]
set_property PULLUP true [get_ports HMI_UART_0_rxd]

# LTC2208 module 2x12 connector, starting after the two power pairs:
#   SHDN/CKI -> W22/V22
#   D0/CKO   -> Y21/Y20
#   D2/D1    -> AB22/AA22
#   D4/D3    -> AB21/AA21
#   D6/D5    -> AB19/AB20
#   D8/D7    -> AA19/Y19
#   D10/D9   -> AB16/AA16
#   D12/D11  -> Y18/AA18
#   D14/D13  -> AB14/AB15
#   OFA/D15  -> Y13/AA13

set_property PACKAGE_PIN W22 [get_ports LTC2208_SHDN]
set_property PACKAGE_PIN V22 [get_ports LTC2208_CKI]
set_property PACKAGE_PIN Y20 [get_ports LTC2208_CKO]
set_property PACKAGE_PIN Y13 [get_ports LTC2208_OFA]

set_property PACKAGE_PIN Y21  [get_ports {LTC2208_DATA[0]}]
set_property PACKAGE_PIN AA22 [get_ports {LTC2208_DATA[1]}]
set_property PACKAGE_PIN AB22 [get_ports {LTC2208_DATA[2]}]
set_property PACKAGE_PIN AA21 [get_ports {LTC2208_DATA[3]}]
set_property PACKAGE_PIN AB21 [get_ports {LTC2208_DATA[4]}]
set_property PACKAGE_PIN AB20 [get_ports {LTC2208_DATA[5]}]
set_property PACKAGE_PIN AB19 [get_ports {LTC2208_DATA[6]}]
set_property PACKAGE_PIN Y19  [get_ports {LTC2208_DATA[7]}]
set_property PACKAGE_PIN AA19 [get_ports {LTC2208_DATA[8]}]
set_property PACKAGE_PIN AA16 [get_ports {LTC2208_DATA[9]}]
set_property PACKAGE_PIN AB16 [get_ports {LTC2208_DATA[10]}]
set_property PACKAGE_PIN AA18 [get_ports {LTC2208_DATA[11]}]
set_property PACKAGE_PIN Y18  [get_ports {LTC2208_DATA[12]}]
set_property PACKAGE_PIN AB15 [get_ports {LTC2208_DATA[13]}]
set_property PACKAGE_PIN AB14 [get_ports {LTC2208_DATA[14]}]
set_property PACKAGE_PIN AA13 [get_ports {LTC2208_DATA[15]}]

set_property IOSTANDARD LVCMOS33 [get_ports {LTC2208_SHDN LTC2208_CKI LTC2208_CKO LTC2208_OFA}]
set_property IOSTANDARD LVCMOS33 [get_ports {LTC2208_DATA[*]}]
set_property DRIVE 8 [get_ports LTC2208_CKI]
set_property SLEW FAST [get_ports LTC2208_CKI]

# LTC2208 full-rate CMOS timing at up to 130 MS/s:
#   ENC-to-data tD = 1.3 ns minimum, 4.0 ns maximum.
# Add 0.8 ns in each direction for module/header trace mismatch and skew.
# CKI is forwarded at 0 degrees; the IOB data registers capture at 300
# degrees (13.0208 ns at 64 MHz), near the center of the valid data eye.
set LTC2208_INPUT_DELAY_MIN_NS 0.500
set LTC2208_INPUT_DELAY_MAX_NS 4.800

set ltc2208_clk_fwd_oddr [get_cells -hierarchical -filter \
    {NAME =~ */ltc2208_clk_fwd_oddr}]
create_generated_clock -name ltc2208_fwd_clk \
    -source [get_pins -of_objects $ltc2208_clk_fwd_oddr -filter \
        {REF_PIN_NAME == C}] \
    -divide_by 1 [get_ports LTC2208_CKI]

set ltc2208_data_ports [get_ports {LTC2208_DATA[*]}]
set_input_delay -clock [get_clocks ltc2208_fwd_clk] \
    -min $LTC2208_INPUT_DELAY_MIN_NS $ltc2208_data_ports
set_input_delay -clock [get_clocks ltc2208_fwd_clk] \
    -max $LTC2208_INPUT_DELAY_MAX_NS $ltc2208_data_ports

# CKO and OFA are health/status inputs only.  Both enter explicit two-flop
# synchronizers and do not participate in sample capture.
set_false_path -from [get_ports {LTC2208_CKO LTC2208_OFA}]

# The AXI GPIO command and returned status cross between the 100 MHz PS clock
# and the 64 MHz capture clock through explicit two-register synchronizers.
# Cut only the asynchronous input of each first stage; the second stage stays
# timed so placement still protects synchronizer MTBF.
set ltc2208_start_meta_cells [get_cells -hierarchical -filter \
    {NAME =~ */start_meta_reg}]
set_false_path -to [get_pins -of_objects $ltc2208_start_meta_cells -filter \
    {REF_PIN_NAME == D}]
