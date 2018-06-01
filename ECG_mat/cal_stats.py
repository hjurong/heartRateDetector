# -*- coding: utf-8 -*-
"""
Created on Tue May 26 10:13:03 2015

@author: JURONG
"""

import numpy as np
import scipy.io as sio
import matplotlib.pyplot as plt



data_dict = sio.loadmat("ECG_waves.mat")
# keys = ['ECG_var_ampr', 'ECG40r', 'ECGdistr', 
#         'ECG_speedupr', 'ECG120r', 'ECG180r', 'ECG80r']
keys = ['ECG_var_ampr']
scale = 0.0049

for key in keys:
    data = data_dict[key].T
    normed = (data - data.min()) / (data.max() - data.min()) * 5

    NN_diff_mean = np.uint16(0)
    N_beats = np.uint16(0)
    beat_cnt = 1
    time2cnt = np.uint16(0)
    NN_mean = np.uint16(0)
    MNN = 0
    NN_current = np.uint16(0)
    SDNN = np.uint16(0)
    RMSSD = np.uint16(0)
    SDSD = np.uint16(0)
    NN50 = np.uint16(0)
    THRES_mean = np.uint16(0)
    new_THRES = np.uint16(0)
    THRES_SD = np.uint16(0)
    
    window_len = 20
    window = np.uint16(np.zeros((window_len,1)))
    moving_avg = np.uint16(2**16-1)
    delta = np.uint16(0)
    current_max = np.uint16(2**16-1)
    SWITCHED = np.uint8(0)
    
    moving = []
    current = []
    diff = []
    detected = []


    for cnt, point in enumerate(normed):

        time1cnt = np.uint16(0)
        
        sec = cnt / 1000.     
        
        converted = np.uint16(point / scale)
        j = np.uint16(cnt)

        if cnt <= window_len:
            index = j % window_len
            window[index] = converted
            delta += converted
        else:
            index = j % window_len
            delta = delta + converted - window[index]
            window[index] = converted
            moving_avg = delta / window_len
            moving.append(moving_avg)

        if cnt < 400:
            if moving_avg < current_max:
                current_max = moving_avg
        elif cnt == 400:
            new_THRES = current_max + 22

        else:
            
            if cnt > 3000:
                break
#            print moving_avg, new_THRES
            
            if sec % 15 == 0:
                print "\nHR = ", 1000 * 60 / NN_mean, NN_mean
                print "SDNN = ", np.sqrt(abs(SDNN / (N_beats-3))), SDNN    
                print "num beats = ", N_beats, beat_cnt
                print "SDSD = ", np.sqrt(abs(SDSD / (N_beats-3))), SDSD
                print "NN_diff_M = ", NN_diff_mean
                print "RMSSD = ", np.sqrt(RMSSD), RMSSD
                print "NN50 = ", NN50
                print "pNN50 = ", NN50*100/(N_beats-2)
                
                
                beat_cnt = 1;
                NN_mean = 0;
                NN_diff_mean = 0;
#
#                NN_diff_mean = np.uint16(0)
#                beat_cnt = 1
#                NN_mean = np.uint16(0)
#                SDNN = np.uint16(0)
#                RMSSD = np.uint16(0)
#                SDSD = np.uint16(0)
                
            
            
            if moving_avg < new_THRES:
                
                detected.append(-1)
                if  SWITCHED == 0:
                    SWITCHED = 1
                    N_beats += 1


                    if N_beats < 2:
#                        time2cnt = np.uint64(cnt/1000.*15625)
                        time2cnt = np.uint64(cnt/1000.*15625) / 16
                    else:
                        time1cnt = time2cnt;
                        time2cnt = np.uint64(cnt/1000.*15625) / 16

                        if N_beats < 3:
                            assert time2cnt > time1cnt
                            NN_current = np.uint16(time2cnt - time1cnt)

                        else:
                            beat_cnt += 1
                            
                            NN_prev = NN_current
                            NN_current = np.int16(time2cnt - time1cnt)
                            
                            current.append(NN_current)
                            
                            NN_diff = abs(NN_current - NN_prev)
                            
                            diff.append(NN_diff)
                            
                            delta1 = NN_diff**2 - RMSSD
                            RMSSD += delta1 / (N_beats-2)
                            
                            delta1 = NN_diff - NN_diff_mean
                            NN_diff_mean += delta1 / (beat_cnt-1)
                            SDSD += delta1*(NN_diff - NN_diff_mean)

                            if NN_diff > 50:
                                NN50 += 1
                            
                                                        
                            
                            delta1 = np.int16(NN_current - NN_mean)
                            
                            NN_mean += delta1 / (beat_cnt-1)
#                            if delta1*(NN_current - NN_mean) > 1000:
                                #print delta1*(NN_current - NN_mean), NN_current, NN_mean, delta1, "ee"
                                #break
                            SDNN = SDNN + delta1*(NN_current - NN_mean)
                            
                            assert RMSSD < 2**15

            
            else:
                SWITCHED = 0
                detected.append(0)



fig = plt.figure()
ax = fig.add_subplot(111)
time = np.linspace(0.4, 3, 2600)
wtime = np.linspace(0, 3, 3000)
ax.plot(time, detected, label="detected\nheart beat")
ax.plot(wtime, normed[:3000], label="Input signal")
ax.plot(time, [new_THRES*scale-0.1]*2600, label="Threshold")
ax.axvspan(0, 0.4, facecolor='r', alpha=0.4)
ax.set_xlabel("time (sec)")
ax.set_ylabel("signal amplitude")
ax.annotate("region in which\ncomputation of\nthreshold occurs", 
            xy=(0.1, 4), xycoords="data")
ax.annotate("signal exceeds\nthreshold", 
            xy=(1.2, -0.5), xycoords="data")
ax.set_title("Thresholding Algorithm")
plt.legend()

