% receive_ad9226_4msps_uart.m
% MATLAB R2023b script for receiving AD9226 4 MS/s UART frames.
% Payload format from PS:
%   ASCII header line ending with LF:
%     ADC_FRAME iteration=<n> samples=8192 bytes=16384 sample_rate=4000000 order=ADC1_LE_U16
%   16384 binary bytes: 8192 uint16 little-endian ADC1 samples
%   ASCII end line:
%     ADC_END iteration=<n>

clear; clc;

serialPort = "COM11";      % Change this to your board UART port.
baudRate = 115200;        % Match the board UART baud rate.
sampleRateHz = 4e6;
frameSamples = 8192;
payloadBytes = frameSamples * 2;
adcBits = 12;
adcMidCode = 2^(adcBits - 1);
plotSamples = 2000;

s = serialport(serialPort, baudRate, "Timeout", 20);
cleanupObj = onCleanup(@() clear("s"));
flush(s);

fprintf("Waiting for ADC_FRAME on %s at %d baud...\n", serialPort, baudRate);

header = "";
while true
    line = readline(s);
    line = strip(line);
    if startsWith(line, "ADC_FRAME")
        header = line;
        break;
    end
    if strlength(line) > 0
        fprintf("skip: %s\n", line);
    end
end

fprintf("header: %s\n", header);

samplesToken = regexp(header, "samples=(\d+)", "tokens", "once");
bytesToken = regexp(header, "bytes=(\d+)", "tokens", "once");
rateToken = regexp(header, "sample_rate=(\d+)", "tokens", "once");

if ~isempty(samplesToken)
    frameSamples = str2double(samplesToken{1});
end
if ~isempty(bytesToken)
    payloadBytes = str2double(bytesToken{1});
else
    payloadBytes = frameSamples * 2;
end
if ~isempty(rateToken)
    sampleRateHz = str2double(rateToken{1});
end

raw = read(s, payloadBytes, "uint8");
if numel(raw) ~= payloadBytes
    error("Expected %d payload bytes, got %d.", payloadBytes, numel(raw));
end

endLine = strip(readline(s));
if ~startsWith(endLine, "ADC_END")
    warning("Expected ADC_END line, got: %s", endLine);
else
    fprintf("end: %s\n", endLine);
end

raw = uint8(raw(:));
lo = uint16(raw(1:2:end));
hi = uint16(raw(2:2:end));
adcCodes = bitor(lo, bitshift(hi, 8));
adcCodes = bitand(adcCodes, uint16(2^adcBits - 1));

adc = double(adcCodes) - adcMidCode;
n = (0:numel(adc)-1).';
t = n / sampleRateHz;

fprintf("Received %d samples, fs=%.3f MHz, duration=%.3f us.\n", ...
    numel(adc), sampleRateHz / 1e6, 1e6 * numel(adc) / sampleRateHz);
fprintf("Code min=%u max=%u mean=%.3f centered mean=%.3f\n", ...
    min(adcCodes), max(adcCodes), mean(double(adcCodes)), mean(adc));

fprintf("FFT bin spacing: %.6f Hz.\n", sampleRateHz / numel(adc));

figure("Name", "AD9226 4 MS/s UART Capture");
tiledlayout(3, 1);

nexttile;
countToPlot = min(plotSamples, numel(adc));
plot(t(1:countToPlot) * 1e6, adc(1:countToPlot), "b-");
grid on;
xlabel("Time (us)");
ylabel("Code - 2048");
title("ADC1 waveform at 4 MS/s");

nexttile;
plot(n(1:countToPlot), double(adcCodes(1:countToPlot)), "b-");
grid on;
xlabel("Sample index");
ylabel("ADC code");
title("Raw 12-bit ADC codes");

nexttile;
window = hann(numel(adc), "periodic");
y = adc(:) .* window;
Y = fft(y);
mag = abs(Y(1:numel(adc)/2));
magDb = 20 * log10(mag / max(mag) + eps);
f = (0:numel(mag)-1).' * sampleRateHz / numel(adc);
plot(f / 1e6, magDb, "r-");
grid on;
xlabel("Frequency (MHz)");
ylabel("Magnitude (dBFS relative)");
title("FFT magnitude");
ylim([-120 5]);

fprintf("ADC1: mean=%.3f rms=%.3f\n", mean(adc), rms(adc));
