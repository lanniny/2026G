`timescale 1ns / 1ps

/*
 * Small behavioral replacements for the three FPGA-specific blocks used by
 * ltc2208_capture.  They are simulation-only and deliberately live outside the
 * Vivado synthesis source set.
 */

module ad9248clk(
    output wire clk_out1,
    output wire clk_out2,
    output wire clk_out3,
    output wire clk_out4,
    input  wire reset,
    output reg  locked,
    input  wire clk_in1
);
    reg clk64;

    initial begin
        clk64 = 1'b0;
        locked = 1'b0;
        #120;
        locked = 1'b1;
    end

    always #7.8125 clk64 = ~clk64;

    assign clk_out1 = clk64;
    assign clk_out2 = ~clk64;
    assign clk_out3 = clk64;
    assign clk_out4 = ~clk64;

    always @(posedge reset) begin
        locked <= 1'b0;
    end

    wire unused_clk_in1 = clk_in1;
endmodule

module ODDR #(
    parameter DDR_CLK_EDGE = "SAME_EDGE"
) (
    output wire Q,
    input  wire C,
    input  wire CE,
    input  wire D1,
    input  wire D2,
    input  wire R,
    input  wire S
);
    assign Q = R ? 1'b0 :
               S ? 1'b1 :
               CE ? (C ? D1 : D2) : 1'b0;
endmodule

module adc1_data_12x4096(
    input  wire        clka,
    input  wire        ena,
    input  wire [0:0]  wea,
    input  wire [13:0] addra,
    input  wire [17:0] dina,
    input  wire        clkb,
    input  wire        enb,
    input  wire [13:0] addrb,
    output reg  [17:0] doutb
);
    reg [17:0] memory [0:16383];

    always @(posedge clka) begin
        if (ena && wea[0]) begin
            memory[addra] <= dina;
        end
    end

    always @(posedge clkb) begin
        if (enb) begin
            doutb <= memory[addrb];
        end
    end
endmodule
