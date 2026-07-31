`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2025/07/16 20:06:25
// Design Name: 
// Module Name: Datadelayer
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


module Datadelayer(
    input      aclk,       // Clock signal
    input  [30:0]    data_in,   // The original, undelayed valid signal
    output reg [30:0] data_out   // The delayed valid signal, synchronized with IP core output
);
parameter LATENCY = 5;

reg [247:0] data_temp1 , data_temp2 , data_temp3 , data_temp4 , data_temp5 , data_temp6,data_temp7,data_temp8;


always @(posedge aclk) begin
    data_temp1 <= data_in;
    data_temp2 <= data_temp1;
    data_temp3 <= data_temp2;
    data_temp4 <= data_temp3;
    data_temp5 <= data_temp4;
    data_temp6 <= data_temp5;
    data_temp7 <= data_temp6;
    data_temp8 <= data_temp7;
    case (LATENCY) 
    1: begin
        data_out <= data_in;
    end
    2: begin
        data_out <= data_temp1;
    end
    3: begin
        data_out <= data_temp2;
    end
    4: begin
        data_out <= data_temp3;
    end
    5: begin
        data_out <= data_temp4;
    end
    6: begin
        data_out <= data_temp5;
    end
    7: begin
        data_out <= data_temp6;
    end
    8: begin
        data_out <= data_temp7;
    end
    9: begin
        data_out <= data_temp8;
    end
    endcase
end
endmodule
