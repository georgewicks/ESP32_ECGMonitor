function [ecg_out3]=differentiator(ecg_out2)
%% derivative operator %% 

b3= [2 1 0 -1 -2];
a3 = [1] *8;

%% creating digital filter (Direct form II)

hd3 = dfilt.df2(b3,a3);

%% filtering the ECG signal from derivative operator %%

ecg_out3 = filter(hd3,ecg_out2);
%% Normalization %%
ecg_out3=ecg_out3 - mean(ecg_out3);
ecg_out3 = ecg_out3/max(abs(ecg_out3));
end
