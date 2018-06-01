clc
clear all
close all

load('ECG_waves.mat');
% ar = [ECG120r; ECG180r; ECG40r; ECG80r; ...
%       ECGdistr; ECG_speedupr; ECG_var_ampr];

x = ECGdistr;
fs = 1000;             %#sampling rate
f0 = 50;                %#notch frequency
fn = fs/2;              %#Nyquist frequency
freqRatio = f0/fn;      %#ratio of notch freq. to Nyquist freq.

notchWidth = 0.1;       %#width of the notch

%Compute zeros
zeros = [exp( sqrt(-1)*pi*freqRatio ), exp( -sqrt(-1)*pi*freqRatio )];

%#Compute poles
poles = (1-notchWidth) * zeros;

figure;
zplane(zeros.', poles.');

b = poly( zeros ); %# Get moving average filter coefficients
a = poly( poles ); %# Get autoregressive filter coefficients

figure;
freqz(b,a,32000,fs)

%#filter signal x
y = filter(b,a,x);
normed = (y-min(y)) / (max(y)-min(y)) * 5;

plot(normed(1:5000))