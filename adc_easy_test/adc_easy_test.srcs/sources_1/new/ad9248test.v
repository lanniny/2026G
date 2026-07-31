`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/04/28 16:11:19
// Design Name: 
// Module Name: ad9248test
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


module ad9248test(
    input               SYS_CLK_IN,         // FPGA 开发板上的主时钟输入
    input               SYS_RST_N_IN,


    output wire       m_axis_aclk,
    output reg [31:0] m_axis_tdata,
    output reg [3:0]  m_axis_tkeep,
    output reg        m_axis_tvalid,
    output reg        m_axis_tlast,
    input             m_axis_tready,


    output              ADC_CLK_COMMON_OUT1_0,
    output              ADC_CLK_COMMON_OUT2_0,
 
    input      [13:0]   ADC_DATA_IN_PIN_0
    );



//reg             new_data_ready;
ad9248clk ad9248c
(
 // Clock out ports
 .clk_out1(clk_out1),     // output clk_out1
 .clk_out2(adc1_clk),     // output clk_out2
 .clk_out3(adc2_clk),     // output clk_out3
 .clk_out4(sys_main_clk),
 // Status and control signals
 .reset(~SYS_RST_N_IN), // input reset
 .locked(locked),       // output locked
// Clock in ports
 .clk_in1(SYS_CLK_IN)      // input clk_in1
);

assign ADC_CLK_COMMON_OUT2_0=clk_out1;
assign ADC_CLK_COMMON_OUT1_0=clk_out1;
assign m_axis_aclk=sys_main_clk;






//adc_ram
reg  weana1=1'b0;
reg  enbb1=1'b0;
reg [12:0]  adra1=13'b0;
reg [12:0]  adrb1=13'b0;
reg  [17:0]  data_in1=18'b0;

wire   [17:0]  data_out1;

reg [12:0] pr_adra1=13'b0;
reg [12:0] pr_adra2=13'b0;

adc1_data_12x4096 adc1_data_ram (
  .clka(adc1_clk),    // input wire clka
  .ena(weana1),      // input wire ena
  .wea(weana1),      // input wire [0 : 0] wea
  .addra(adra1),  // input wire [11 : 0] addra
  .dina(data_in1),    // input wire [17 : 0] dina  data_in1
  .clkb(sys_main_clk),    // input wire clkb
  .enb(enbb1),      // input wire enb
  .addrb(adrb1),  // input wire [11 : 0] addrb
  .doutb(data_out1)  // output wire [17 : 0] doutb
);

reg  weana2=1'b0;
reg  enbb2=1'b0;
reg [12:0]  adra2=13'b0;
reg [12:0]  adrb2=13'b0;
reg  [17:0]  data_in2=18'b0;

wire  [17:0]  data_out2;

adc2_data_12x4096 adc2_data_ram (
  .clka(adc2_clk),    // input wire clka
  .ena(weana2),      // input wire ena
  .wea(weana2),      // input wire [0 : 0] wea
  .addra(adra2),  // input wire [11 : 0] addra
  .dina(data_in2),    // input wire [17 : 0] dina  data_in1
  .clkb(sys_main_clk),    // input wire clkb
  .enb(enbb2),      // input wire enb
  .addrb(adrb2),  // input wire [11 : 0] addrb
  .doutb(data_out2)  // output wire [17 : 0] doutb
);

localparam TEST_1=2'b0;
localparam TEST_2=2'b1;
localparam TEST_3=2'b10;
localparam TEST_4=2'b11;

(* mark_debug="true" *)reg [1:0] test_status=2'b0;



// 写入-adc1
always @(posedge adc1_clk) begin
            case(test_status)
                TEST_1: begin
                    // 写入端口运转
                    if (adra1 < 13'd8191) begin
                        weana1 <= 1'b1;
                        adra1 <= pr_adra1;
                        data_in1 <= {4'b0,ADC_DATA_IN_PIN_0};
                        pr_adra1 <= pr_adra1+13'b1;
                    end 
                    else begin
                        weana1 <= 1'b0;
                        adra1 <= 13'b0;
                        data_in1 <= 18'b0;
                        pr_adra1 <= 13'b0;
                    end
                end
                TEST_2: begin
                    // 写入端口无效
                    weana1 <= 1'b0;
                    adra1 <= 13'b0;
                    data_in1 <= 18'b0;
                    pr_adra1 <= 13'b0;

                end
                default: begin
                    // 写入
                    weana1 <= 1'b0;
                    adra1 <= 13'b0;
                    data_in1 <= 18'b0;
                    pr_adra1 <= 13'b0;

                end
            endcase
end

// 写入-adc2
always @(posedge adc2_clk ) begin

            case(test_status)
                TEST_1: begin
                    // 写入端口运转
                    if (adra2 < 13'd8191) begin
                        weana2 <= 1'b1;
                        adra2 <= pr_adra2;
                        data_in2 <= {4'b0, ADC_DATA_IN_PIN_0};
                        pr_adra2 <= pr_adra2+13'b1;
                    end 
                    else begin
                        weana2 <= 1'b0;
                        adra2 <= 13'b0;
                        data_in2 <= 18'b0;
                        pr_adra2 <= 13'b0;
                    end
                end
                TEST_2: begin
                    // 写入端口无效
                    weana2 <= 1'b0;
                    adra2 <= 13'b0;
                    data_in2 <= 18'b0;
                    pr_adra2 <= 13'b0;

                end
                default: begin
                    // 写入
                    weana2 <= 1'b0;
                    adra2 <= 13'b0;
                    data_in2 <= 18'b0;
                    pr_adra2 <= 13'b0;

                end
            endcase
end

(* mark_debug="true" *)reg [1:0]  now_times_adc=2'b0;
(* mark_debug="true" *)reg [17:0] fft_in_data=18'b0;

localparam [12:0] FRAME_LAST_ADDR = 13'd8191;
localparam integer data_delay = 2;
localparam integer dma_extra_wait = 2;
localparam integer ram_read_wait_cycles = data_delay + dma_extra_wait;

localparam STREAM_IDLE = 3'd0;
localparam STREAM_WAIT = 3'd1;
localparam STREAM_SEND = 3'd2;
localparam STREAM_DONE = 3'd3;

reg [2:0]  stream_state=STREAM_IDLE;
reg [12:0] stream_rd_addr=13'b0;
reg [3:0]  ram_wait_cnt=4'b0;
reg        stream_done=1'b0;

wire axis_fire;
wire [17:0] stream_ram_data;

assign axis_fire = m_axis_tvalid && m_axis_tready;
assign stream_ram_data = now_times_adc[0] ? data_out2 : data_out1;

always @(posedge sys_main_clk) begin
    if (!SYS_RST_N_IN) begin
        enbb1 <= 1'b0;
        enbb2 <= 1'b0;
        adrb1 <= 13'b0;
        adrb2 <= 13'b0;
        m_axis_tdata <= 32'b0;
        m_axis_tkeep <= 4'hF;
        m_axis_tvalid <= 1'b0;
        m_axis_tlast <= 1'b0;
        fft_in_data <= 18'b0;
        stream_state <= STREAM_IDLE;
        stream_rd_addr <= 13'b0;
        ram_wait_cnt <= 4'b0;
        stream_done <= 1'b0;
    end
    else begin
        stream_done <= 1'b0;
        m_axis_tkeep <= 4'hF;

        case(stream_state)
            STREAM_IDLE: begin
                enbb1 <= 1'b0;
                enbb2 <= 1'b0;
                adrb1 <= 13'b0;
                adrb2 <= 13'b0;
                m_axis_tvalid <= 1'b0;
                m_axis_tlast <= 1'b0;
                stream_rd_addr <= 13'b0;
                ram_wait_cnt <= 4'b0;

                if (test_status == TEST_3) begin
                    stream_rd_addr <= 13'b0;
                    ram_wait_cnt <= 4'b0;
                    if (now_times_adc[0]) begin
                        enbb2 <= 1'b1;
                        adrb2 <= 13'b0;
                    end
                    else begin
                        enbb1 <= 1'b1;
                        adrb1 <= 13'b0;
                    end
                    stream_state <= STREAM_WAIT;
                end
            end

            STREAM_WAIT: begin
                enbb1 <= 1'b0;
                enbb2 <= 1'b0;

                if (test_status != TEST_3) begin
                    stream_state <= STREAM_IDLE;
                end
                else if (ram_wait_cnt < ram_read_wait_cycles - 1) begin
                    ram_wait_cnt <= ram_wait_cnt + 1'b1;
                end
                else begin
                    fft_in_data <= stream_ram_data;
                    m_axis_tdata <= {14'b0, stream_ram_data};
                    m_axis_tvalid <= 1'b1;
                    m_axis_tlast <= (stream_rd_addr == FRAME_LAST_ADDR);
                    stream_state <= STREAM_SEND;
                end
            end

            STREAM_SEND: begin
                enbb1 <= 1'b0;
                enbb2 <= 1'b0;

                if (test_status != TEST_3) begin
                    m_axis_tvalid <= 1'b0;
                    m_axis_tlast <= 1'b0;
                    stream_state <= STREAM_IDLE;
                end
                else if (axis_fire) begin
                    m_axis_tvalid <= 1'b0;
                    m_axis_tlast <= 1'b0;

                    if (stream_rd_addr == FRAME_LAST_ADDR) begin
                        stream_done <= 1'b1;
                        stream_state <= STREAM_DONE;
                    end
                    else begin
                        stream_rd_addr <= stream_rd_addr + 1'b1;
                        ram_wait_cnt <= 4'b0;
                        if (now_times_adc[0]) begin
                            enbb2 <= 1'b1;
                            adrb2 <= stream_rd_addr + 1'b1;
                        end
                        else begin
                            enbb1 <= 1'b1;
                            adrb1 <= stream_rd_addr + 1'b1;
                        end
                        stream_state <= STREAM_WAIT;
                    end
                end
            end

            STREAM_DONE: begin
                enbb1 <= 1'b0;
                enbb2 <= 1'b0;
                m_axis_tvalid <= 1'b0;
                m_axis_tlast <= 1'b0;

                if (test_status != TEST_3) begin
                    stream_state <= STREAM_IDLE;
                end
            end

            default: begin
                stream_state <= STREAM_IDLE;
            end
        endcase
    end
end

//状态机
always @(posedge sys_main_clk) begin

            case(test_status)
                TEST_1: begin
                    if (adra2 < 13'd8191) begin
                        // 状态坤
                        test_status <= TEST_1;
                    end 
                    else begin
                        // 状态坤
                        test_status <= TEST_2;
                    end
                end
                TEST_2: begin
                    test_status <= TEST_3;
                end
                TEST_3: begin
                    if (stream_done) begin
                        test_status <= TEST_4;
                    end
                    else begin
                        test_status <= TEST_3;

                    end
                end
                TEST_4: begin
                    if (now_times_adc[0:0] == 1'b1) begin //(new_data_ready && realy_enable == 1'b1)
                        test_status <= TEST_1;
                        now_times_adc <= now_times_adc+1;
                    end 
                    else if (now_times_adc[0:0] == 1'b0) begin //(gototwo == 1'b1)
                        test_status <= TEST_2;
                        now_times_adc <= now_times_adc+1;
                    end
                    else  begin
                        test_status <= TEST_4;
                        now_times_adc <= now_times_adc;
                    end
                end
                default: begin
                    // 状态坤恢复
                    test_status <= TEST_1;
                end
                
            endcase
end

endmodule
