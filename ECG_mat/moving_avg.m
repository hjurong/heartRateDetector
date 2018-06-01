%%
clc
clear all
close all

load('ECG_waves.mat');
ar = [ECG120r; ECG180r; ECG40r; ECG80r; ...
      ECGdistr; ECG_speedupr; ECG_var_ampr; ECG_filtered];
  
window = ones(1,20) / 20;

filtered = zeros(size(ar));

for i=1:8
    normed = (ar(i,:)-min(ar(i,:))) / (max(ar(i,:))-min(ar(i,:))) * 5;
    filtered(i,:) = conv(normed,window,'same');
    figure;
    plot(filtered(i,1:5000)); hold on;
%     plot(normed(1:1000),'k');
    
    
end