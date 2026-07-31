`timescale 1ns / 1ps

// LTC2208 capture path for the periodic-signal analyzer.
//
// The ADC is clocked at 64 MS/s and uniformly decimated by 16 to 4 MS/s.
// One triggered frame contains 8192 samples (2.048 ms), which gives
// 4 MHz / 8192 = 488.28125 Hz FFT-bin spacing.
//
// This step intentionally performs sample selection only.  A low-pass
// anti-alias filter must be inserted before the decimator in the next step.
// The legacy filename is retained so the verified Vivado project keeps the
// same source-file reference.

module ltc2208_capture(
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 SYS_CLK_IN CLK" *)
    (* X_INTERFACE_PARAMETER = "FREQ_HZ 100000000" *)
    input               SYS_CLK_IN,
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 SYS_RST_N_IN RST" *)
    (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
    input               SYS_RST_N_IN,
    output              LTC2208_CKI,
    output              LTC2208_SHDN,

    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 m_axis_aclk CLK" *)
    (* X_INTERFACE_PARAMETER = "FREQ_HZ 64000000, PHASE 300.000, CLK_DOMAIN ltc2208_capture_m_axis_aclk, ASSOCIATED_BUSIF m_axis" *)
    output wire         m_axis_aclk,
    output reg [31:0]   m_axis_tdata,
    output reg [3:0]    m_axis_tkeep,
    output reg          m_axis_tvalid,
    output reg          m_axis_tlast,
    input               m_axis_tready,

    input               ps_capture_start,
    output      [7:0]   ps_status,

    input               LTC2208_CKO,
    input               LTC2208_OFA,
    input      [15:0]   LTC2208_DATA
    );

localparam integer RAW_SAMPLE_RATE_HZ = 64000000;
localparam integer OUTPUT_SAMPLE_RATE_HZ = 4000000;
localparam integer DECIMATION_FACTOR = 16;
localparam integer FRAME_SAMPLE_COUNT = 8192;
localparam [3:0] DECIMATION_LAST = 4'd15;
localparam [12:0] FRAME_LAST_ADDR = 13'd8191;

wire adc_capture_clk;
wire adc_forward_clk;
wire unused_clk_120;
wire unused_clk_180;
wire sys_main_clk;
wire locked;

ad9248clk ad9248c
(
    // Forward 0-degree CKI and capture the returned data at 300 degrees.
    .clk_out1(adc_forward_clk),
    .clk_out2(unused_clk_120),
    .clk_out3(adc_capture_clk),
    .clk_out4(unused_clk_180),
    .reset(~SYS_RST_N_IN),
    .locked(locked),
    .clk_in1(SYS_CLK_IN)
);

// Forward CKI through OLOGIC.  Besides reducing output skew, this gives
// Vivado an explicit launch-clock endpoint for the input-delay constraints.
ODDR #(
    .DDR_CLK_EDGE("SAME_EDGE")
) ltc2208_clk_fwd_oddr (
    .Q(LTC2208_CKI),
    .C(adc_forward_clk),
    .CE(1'b1),
    .D1(1'b1),
    .D2(1'b0),
    .R(1'b0),
    .S(1'b0)
);

// SHDN is active high.  Hold it low for normal conversion.
assign LTC2208_SHDN = 1'b0;

assign sys_main_clk = adc_capture_clk;
assign m_axis_aclk = sys_main_clk;

wire sys_reset;
wire ps_reset_capture;
wire locked_capture;

// Use the vendor-recognized CDC macros so reset assertion/deassertion and the
// MMCM lock indication are both identified correctly by report_cdc.
xpm_cdc_async_rst #(
    .DEST_SYNC_FF(2),
    .INIT_SYNC_FF(0),
    .RST_ACTIVE_HIGH(1)
) capture_reset_sync (
    .src_arst(~SYS_RST_N_IN),
    .dest_clk(sys_main_clk),
    .dest_arst(ps_reset_capture)
);

xpm_cdc_single #(
    .DEST_SYNC_FF(2),
    .INIT_SYNC_FF(0),
    .SIM_ASSERT_CHK(0),
    .SRC_INPUT_REG(0)
) capture_locked_sync (
    .src_clk(1'b0),
    .src_in(locked),
    .dest_clk(sys_main_clk),
    .dest_out(locked_capture)
);

assign sys_reset = ps_reset_capture || !locked_capture;

// First-stage ADC register.  The LTC2208 CMOS outputs change 1.3 ns to
// 4.0 ns after CKI.  At 64 MHz, the 300-degree capture phase is 13.0208 ns
// after the launch edge and therefore near the center of the data eye.
(* IOB = "TRUE" *) reg [15:0] adc_input_reg = 16'b0;

always @(posedge adc_capture_clk) begin
    if (sys_reset) begin
        adc_input_reg <= 16'b0;
    end
    else begin
        adc_input_reg <= LTC2208_DATA;
    end
end

// CKO is on a non-clock-capable FPGA pin, so it is monitored as data in the
// 100 MHz PS clock domain instead of being routed onto a global clock net.
// OFA is synchronized into the capture domain for software diagnostics.
(* ASYNC_REG = "TRUE" *) reg [1:0] cko_sync = 2'b00;
(* ASYNC_REG = "TRUE" *) reg [1:0] ofa_sync = 2'b00;
reg cko_sync_d = 1'b0;
reg cko_seen = 1'b0;

always @(posedge SYS_CLK_IN or negedge SYS_RST_N_IN) begin
    if (!SYS_RST_N_IN) begin
        cko_sync <= 2'b00;
        cko_sync_d <= 1'b0;
        cko_seen <= 1'b0;
    end
    else begin
        cko_sync <= {cko_sync[0], LTC2208_CKO};
        cko_sync_d <= cko_sync[1];
        if (cko_sync[1] != cko_sync_d) begin
            cko_seen <= 1'b1;
        end
    end
end

always @(posedge sys_main_clk) begin
    if (sys_reset) begin
        ofa_sync <= 2'b00;
    end
    else begin
        ofa_sync <= {ofa_sync[0], LTC2208_OFA};
    end
end

// ADC frame buffer.  The existing RAM has a depth of 16384 words; the new
// data chain uses addresses 0..8191.
reg         adc1_wr_en = 1'b0;
reg  [13:0] adc1_wr_addr = 14'b0;
reg  [17:0] adc1_wr_data = 18'b0;
reg         adc1_rd_en = 1'b0;
reg  [13:0] adc1_rd_addr = 14'b0;
wire [17:0] adc1_rd_data;

adc1_data_12x4096 adc1_data_ram (
    .clka(adc_capture_clk),
    .ena(adc1_wr_en),
    .wea(adc1_wr_en),
    .addra(adc1_wr_addr),
    .dina(adc1_wr_data),
    .clkb(sys_main_clk),
    .enb(adc1_rd_en),
    .addrb(adc1_rd_addr),
    .doutb(adc1_rd_data)
);

localparam TEST_1 = 2'b00; // capture
localparam TEST_2 = 2'b01; // flush final BRAM write
localparam TEST_3 = 2'b10; // stream frame to DMA
localparam TEST_4 = 2'b11; // idle

(* mark_debug = "true" *) reg [1:0] test_status = TEST_4;

reg [3:0]  decimation_count = 4'b0;
reg [12:0] capture_wr_addr = 13'b0;
reg        capture_queue_done = 1'b0;

// Capture one ADC sample every 16 raw ADC clocks.  adc1_wr_en/data/address
// are registered because the Block Memory Generator samples them at the same
// rising edge.  TEST_2 leaves one clock for the final queued write to commit.
always @(posedge sys_main_clk) begin
    if (sys_reset) begin
        adc1_wr_en <= 1'b0;
        adc1_wr_addr <= 14'b0;
        adc1_wr_data <= 18'b0;
        decimation_count <= 4'b0;
        capture_wr_addr <= 13'b0;
        capture_queue_done <= 1'b0;
    end
    else begin
        adc1_wr_en <= 1'b0;

        case (test_status)
            TEST_1: begin
                if (!capture_queue_done) begin
                    if (decimation_count == DECIMATION_LAST) begin
                        decimation_count <= 4'b0;
                        adc1_wr_en <= 1'b1;
                        adc1_wr_addr <= {1'b0, capture_wr_addr};
                        adc1_wr_data <= {2'b0, adc_input_reg};

                        if (capture_wr_addr == FRAME_LAST_ADDR) begin
                            capture_queue_done <= 1'b1;
                        end
                        else begin
                            capture_wr_addr <= capture_wr_addr + 1'b1;
                        end
                    end
                    else begin
                        decimation_count <= decimation_count + 1'b1;
                    end
                end
            end

            TEST_4: begin
                adc1_wr_addr <= 14'b0;
                adc1_wr_data <= 18'b0;
                decimation_count <= 4'b0;
                capture_wr_addr <= 13'b0;
                capture_queue_done <= 1'b0;
            end

            default: begin
                adc1_wr_en <= 1'b0;
            end
        endcase
    end
end

localparam STREAM_IDLE    = 3'd0;
localparam STREAM_READ    = 3'd1;
localparam STREAM_WAIT    = 3'd2;
localparam STREAM_PRESENT = 3'd3;
localparam STREAM_SEND    = 3'd4;
localparam STREAM_DONE    = 3'd5;

reg [2:0]  stream_state = STREAM_IDLE;
reg [12:0] stream_rd_addr = 13'b0;
reg        stream_done = 1'b0;

wire axis_fire;
wire stream_last_addr;

assign axis_fire = m_axis_tvalid && m_axis_tready;
assign stream_last_addr = (stream_rd_addr == FRAME_LAST_ADDR);

always @(posedge sys_main_clk) begin
    if (sys_reset) begin
        adc1_rd_en <= 1'b0;
        adc1_rd_addr <= 14'b0;
        m_axis_tdata <= 32'b0;
        m_axis_tkeep <= 4'hF;
        m_axis_tvalid <= 1'b0;
        m_axis_tlast <= 1'b0;
        stream_state <= STREAM_IDLE;
        stream_rd_addr <= 13'b0;
        stream_done <= 1'b0;
    end
    else begin
        stream_done <= 1'b0;
        m_axis_tkeep <= 4'hF;

        case (stream_state)
            STREAM_IDLE: begin
                adc1_rd_en <= 1'b0;
                adc1_rd_addr <= 14'b0;
                m_axis_tvalid <= 1'b0;
                m_axis_tlast <= 1'b0;
                stream_rd_addr <= 13'b0;

                if (test_status == TEST_3) begin
                    stream_state <= STREAM_READ;
                end
            end

            STREAM_READ: begin
                m_axis_tvalid <= 1'b0;
                m_axis_tlast <= 1'b0;

                if (test_status != TEST_3) begin
                    adc1_rd_en <= 1'b0;
                    stream_state <= STREAM_IDLE;
                end
                else begin
                    adc1_rd_en <= 1'b1;
                    adc1_rd_addr <= {1'b0, stream_rd_addr};
                    stream_state <= STREAM_WAIT;
                end
            end

            STREAM_WAIT: begin
                adc1_rd_en <= 1'b0;

                if (test_status != TEST_3) begin
                    stream_state <= STREAM_IDLE;
                end
                else begin
                    stream_state <= STREAM_PRESENT;
                end
            end

            STREAM_PRESENT: begin
                adc1_rd_en <= 1'b0;

                if (test_status != TEST_3) begin
                    m_axis_tvalid <= 1'b0;
                    m_axis_tlast <= 1'b0;
                    stream_state <= STREAM_IDLE;
                end
                else begin
                    // One 16-bit LTC2208 sample per 32-bit DMA word.
                    m_axis_tdata <= {16'b0, adc1_rd_data[15:0]};
                    m_axis_tvalid <= 1'b1;
                    m_axis_tlast <= stream_last_addr;
                    stream_state <= STREAM_SEND;
                end
            end

            STREAM_SEND: begin
                adc1_rd_en <= 1'b0;

                if (test_status != TEST_3) begin
                    m_axis_tvalid <= 1'b0;
                    m_axis_tlast <= 1'b0;
                    stream_state <= STREAM_IDLE;
                end
                else if (axis_fire) begin
                    m_axis_tvalid <= 1'b0;
                    m_axis_tlast <= 1'b0;

                    if (stream_last_addr) begin
                        stream_done <= 1'b1;
                        stream_state <= STREAM_DONE;
                    end
                    else begin
                        stream_rd_addr <= stream_rd_addr + 1'b1;
                        stream_state <= STREAM_READ;
                    end
                end
            end

            STREAM_DONE: begin
                adc1_rd_en <= 1'b0;
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

(* ASYNC_REG = "TRUE" *) reg start_meta = 1'b0;
(* ASYNC_REG = "TRUE" *) reg start_sync = 1'b0;
reg start_sync_d = 1'b0;
reg capture_busy = 1'b0;
reg capture_done = 1'b0;
wire start_pulse;
reg idle_status_capture = 1'b1;

assign start_pulse = start_sync && !start_sync_d;
// Bit 3 exposes the AXI-stream backpressure state.  It is particularly
// useful on hardware: TEST_3 + STREAM_SEND + !TREADY means that the DMA path
// is not accepting the pending sample.
assign ps_status = {cko_seen, ofa_sync[1], stream_state[1:0], m_axis_tready,
                    capture_done, capture_busy, idle_status_capture};

// Register the only combinational status expression before it enters the AXI
// GPIO's built-in XPM_CDC_ARRAY_SINGLE synchronizer.
always @(posedge sys_main_clk) begin
    if (sys_reset) begin
        idle_status_capture <= 1'b1;
    end
    else begin
        idle_status_capture <= (test_status == TEST_4) && !capture_busy;
    end
end

always @(posedge sys_main_clk) begin
    if (sys_reset) begin
        test_status <= TEST_4;
        start_meta <= 1'b0;
        start_sync <= 1'b0;
        start_sync_d <= 1'b0;
        capture_busy <= 1'b0;
        capture_done <= 1'b0;
    end
    else begin
        start_meta <= ps_capture_start;
        start_sync <= start_meta;
        start_sync_d <= start_sync;

        // A start pulse while busy is an explicit abort request from the PS.
        // This lets software recover from a DMA/AXIS timeout without having
        // to reconfigure the FPGA.  The stream and capture state machines see
        // TEST_4 on the following clock and return to their idle states.
        if (start_pulse && (test_status != TEST_4)) begin
            test_status <= TEST_4;
            capture_busy <= 1'b0;
            capture_done <= 1'b0;
        end
        else begin
            case (test_status)
                TEST_1: begin
                    if (capture_queue_done) begin
                        test_status <= TEST_2;
                    end
                end

                TEST_2: begin
                    test_status <= TEST_3;
                end

                TEST_3: begin
                    if (stream_done) begin
                        test_status <= TEST_4;
                        capture_busy <= 1'b0;
                        capture_done <= 1'b1;
                    end
                end

                TEST_4: begin
                    if (start_pulse) begin
                        test_status <= TEST_1;
                        capture_busy <= 1'b1;
                        capture_done <= 1'b0;
                    end
                end

                default: begin
                    test_status <= TEST_4;
                    capture_busy <= 1'b0;
                end
            endcase
        end
    end
end

endmodule
