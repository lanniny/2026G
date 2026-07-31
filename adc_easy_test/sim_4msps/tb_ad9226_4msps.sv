`timescale 1ns / 1ps

/*
 * End-to-end PL stimulus for requirement 1.
 *
 * The synthetic conditioned signal is:
 *   1.0 V DC
 *   + 100 mV peak at 50 kHz
 *   +  25 mV peak at 100 kHz
 *
 * The LTC2208 module model follows its documented offset-binary transfer:
 *   zero code             = 32767.5
 *   input volts per code  = 9 V / 65535
 *
 * ltc2208_capture receives 64 MS/s ADC codes, decimates by 16, buffers 8192
 * samples and streams the frame over AXI4-Stream.  The resulting samples
 * are written to pl_samples.txt for the native PS algorithm test.
 */

module tb_ad9226_4msps;
    localparam real RAW_FS_HZ = 64000000.0;
    localparam real TWO_PI = 6.28318530717958647692;
    localparam real ZERO_CODE = 32767.5;
    localparam real INPUT_VOLTS_PER_CODE = 9.0 / 65535.0;

    reg sys_clk = 1'b0;
    reg sys_rst_n = 1'b0;
    reg ps_capture_start = 1'b0;
    reg m_axis_tready = 1'b1;
    reg [15:0] adc_data = 16'd32768;

    wire adc_cki;
    wire adc_shdn;
    wire adc_cko = adc_cki;
    wire axis_clk;
    wire [31:0] axis_data;
    wire [3:0] axis_keep;
    wire axis_valid;
    wire axis_last;
    wire [7:0] ps_status;

    integer raw_sample_index = 0;
    integer output_sample_count = 0;
    integer output_file;
    integer adc_code_integer;
    integer ready_divider = 0;
    real time_seconds;
    real input_voltage;

    always #5 sys_clk = ~sys_clk;

    ltc2208_capture dut (
        .SYS_CLK_IN(sys_clk),
        .SYS_RST_N_IN(sys_rst_n),
        .LTC2208_CKI(adc_cki),
        .LTC2208_SHDN(adc_shdn),
        .m_axis_aclk(axis_clk),
        .m_axis_tdata(axis_data),
        .m_axis_tkeep(axis_keep),
        .m_axis_tvalid(axis_valid),
        .m_axis_tlast(axis_last),
        .m_axis_tready(m_axis_tready),
        .ps_capture_start(ps_capture_start),
        .ps_status(ps_status),
        .LTC2208_CKO(adc_cko),
        .LTC2208_OFA(1'b0),
        .LTC2208_DATA(adc_data)
    );

    /*
     * Change data on the falling edge so it is stable at the DUT's rising
     * capture edge.  Quantization is the only error deliberately added.
     */
    always @(negedge axis_clk) begin
        time_seconds = raw_sample_index / RAW_FS_HZ;
        input_voltage =
            1.0 +
            0.100 * $cos(TWO_PI * 50000.0 * time_seconds) +
            0.025 * $cos(TWO_PI * 100000.0 * time_seconds);

        adc_code_integer =
            $rtoi(ZERO_CODE +
                  input_voltage / INPUT_VOLTS_PER_CODE +
                  0.5);

        if (adc_code_integer < 0)
            adc_code_integer = 0;
        if (adc_code_integer > 65535)
            adc_code_integer = 65535;

        adc_data <= adc_code_integer[15:0];
        raw_sample_index <= raw_sample_index + 1;
    end

    /*
     * Add repeatable backpressure.  This checks that the DUT holds TVALID,
     * TDATA and TLAST until the simulated DMA accepts each word.
     */
    always @(posedge axis_clk) begin
        if (!sys_rst_n) begin
            ready_divider <= 0;
            m_axis_tready <= 1'b1;
        end
        else begin
            ready_divider <= ready_divider + 1;
            m_axis_tready <= (ready_divider[2:0] != 3'b111);
        end
    end

    always @(posedge axis_clk) begin
        if (axis_valid && m_axis_tready) begin
            if (axis_keep !== 4'hf) begin
                $fatal(1, "TKEEP error at output sample %0d",
                       output_sample_count);
            end

            if (axis_data[31:16] !== 16'b0) begin
                $fatal(1, "Upper AXI data bits are nonzero at sample %0d",
                       output_sample_count);
            end

            if (axis_last !== (output_sample_count == 8191)) begin
                $fatal(1, "TLAST error at output sample %0d",
                       output_sample_count);
            end

            $fwrite(output_file, "%0d\n", axis_data[15:0]);
            output_sample_count <= output_sample_count + 1;

            if (axis_last) begin
                $fclose(output_file);
                $display("PL_SIM_PASS samples=%0d status=0x%02x",
                         output_sample_count + 1, ps_status);
                #200;
                $finish;
            end
        end
    end

    initial begin
        output_file = $fopen("pl_samples.txt", "w");
        if (output_file == 0) begin
            $fatal(1, "Could not create pl_samples.txt");
        end

        #300;
        sys_rst_n = 1'b1;

        if (adc_shdn !== 1'b0) begin
            $fatal(1, "LTC2208 SHDN must remain low");
        end

        repeat (12) @(posedge axis_clk);
        ps_capture_start = 1'b1;
        repeat (4) @(posedge axis_clk);
        ps_capture_start = 1'b0;
    end

    initial begin
        #5000000;
        $fatal(1,
               "PL simulation timeout: samples=%0d status=0x%02x",
               output_sample_count, ps_status);
    end
endmodule
