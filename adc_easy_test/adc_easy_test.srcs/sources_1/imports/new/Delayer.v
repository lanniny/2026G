`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2025/07/15 15:57:26
// Design Name: 
// Module Name: Delayer
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module Delayer (
    input      aclk,       // Clock signal
    input      valid_in,   // The original, undelayed valid signal
    output reg valid_out   // The delayed valid signal, synchronized with IP core output
);
parameter LATENCY = 5;


reg [31:0] in_temp=32'b0;
always @(posedge aclk) begin
    in_temp <= (in_temp<<1'b1)+valid_in;
    valid_out <= in_temp[LATENCY-2:LATENCY-2];
end


endmodule