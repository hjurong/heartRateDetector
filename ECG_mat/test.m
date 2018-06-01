%% http://stackoverflow.com/questions/11352047/finding-moving-average-from-data-points-in-python
clc
clear all
close all

load('ECG_waves.mat');
ar = [ECG120r; ECG180r; ECG40r; ECG80r; ...
      ECGdistr; ECG_speedupr; ECG_var_ampr];

%% https://decibel.ni.com/content/docs/DOC-32800

%%

THRES = 3.85;
SWITCHED = 0;
fs = 1000; % time == N / fs

for i=1:7
    
    fprintf('\n\n\n....Num=%d...\n\n\n',i);
    normed = (ar(i,:)-min(ar(i,:))) / (max(ar(i,:))-min(ar(i,:))) * 5;
%     ar(i,:) = normed;
    
    NN_diff_mean = 0;
    N_beats = 0;
    time2cnt = 0;
    NN_mean = 0;
    NN_current = 0;
    SDNN = 0;
    RMSSD = 0;
    SDSD = 0;
    NN50 = 0;
    THRES_mean = THRES;
    new_THRES = THRES;
    THRES_SD = 0;
    
    window = zeros(1,20);
    moving_avg = 10;
    delta = 0;
    current_max = 10;
    
    
    
    
    
   for j =1:length(normed)
       
       if (j <= 20)
           index = mod(j,20)+1; 
           window(index) = normed(j);
           delta = delta + normed(j);
           
       else
           index = mod(j,20)+1; % 1 --> 20
           delta = delta - window(index) + normed(j);
           window(index) = normed(j);
           moving_avg = delta / 20;
           
           
       end
       
       if (j < 400)
           if (moving_avg < current_max)
              current_max = moving_avg; 
              idx = j;
           end
       elseif j == 400
           new_THRES = current_max + 0.1;
       end
       
       
       if (moving_avg < new_THRES && j > 400)
           
%            delta0 = moving_avg - THRES_mean;
%            THRES_mean = THRES_mean + delta0 / (j);
%            THRES_SD = THRES_SD + delta0*(moving_avg-THRES_mean);
%            new_THRES = THRES_mean + 0.05;
           
           
           if (SWITCHED == 0)
               
               N_beats = N_beats + 1;

               if (N_beats < 2)
                  time2cnt = (j) / fs; % time in sec

               else 
                  time1cnt = time2cnt;
                  time2cnt = (j) / fs;

                  if (N_beats < 3)
                     NN_current = time2cnt - time1cnt;
                     
                     assert(NN_current >= 0, 'NN_current negative');
                     
%                      delta1 = NN_current - NN_mean;
%                      NN_mean = NN_mean + delta1 / (N_beats-1);
%                      SDNN = SDNN + delta1*(NN_current - NN_mean);

                  else
                      NN_prev = NN_current;
                      NN_current = time2cnt - time1cnt;
                      
                     
                      NN_diff = abs(NN_current - NN_prev);
                      assert(NN_diff >= 0, 'NN_diff negative');
                      
                      RMSSD = RMSSD + (NN_diff * NN_diff);

                      if (NN_diff > 50e-3)
                          
                          NN50 = NN50 + 1;
                
                      end

                      delta1 = NN_current - NN_mean;
                      NN_mean = NN_mean + delta1 / (N_beats-1);
                      SDNN = SDNN + delta1*(NN_current - NN_mean);

                      delta2 = NN_diff - NN_diff_mean;
                      NN_diff_mean = NN_diff_mean + delta2 / (N_beats-2);
                      
                      
                      SDSD = SDSD + delta2*(NN_diff - NN_diff_mean);
                  end

               end



               SWITCHED = 1;
           end

       else 
           SWITCHED = 0;

       end

       %% print some data every second ==> j = 1*fs = 1000
       if (mod(j, fs) == 0 && mod(j/fs, 100)==0)

           fprintf('\n\ncurrent time: %dsec\n', j/fs);
           fprintf('number of beats: %d\n', N_beats);
           fprintf('heart rate (beats/min): %d\n', N_beats/(j/fs)*60);
           fprintf('MNN: %d\n', NN_mean);
           fprintf('STDNN: %d\n', SDNN/(N_beats-3-1));
           fprintf('RMSSD: %d\n', sqrt(RMSSD/(N_beats-3-1)));
           fprintf('NN50: %d\n', NN50);
           fprintf('pNN50: %d\n', NN50/N_beats);
           fprintf('SDSD: %d\n', SDSD / (N_beats-3- 1));
           fprintf('current THRES: %d\n', new_THRES);

       end

   end
   
end