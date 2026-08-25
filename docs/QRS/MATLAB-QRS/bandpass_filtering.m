function[ecg_out2]= bandpass_filtering(ecg)
%% Low pass filtering %%

b=[1 0 0 0 0 0 -2 0 0 0 0 0 1];
a=[1 -2 1] * 32;

%% creating digital filter (Direct form II)
hd1=dfilt.df2(b,a);

%% filtering ECG signal using LPF %%

ecg_out1=filter(hd1,ecg);
ecg_out1=ecg_out1 - mean(ecg_out1);
ecg_out1=ecg_out1/max(abs(ecg_out1));

%% High pass filtering %%

b2=[-1,zeros(1,15),32,-32,zeros(1,14),1];
a2=[1,-1] * 32;

%% creating digital filter (Direct form II)
hd2=dfilt.df2(b2,a2);

%% filtering the ECG signal from HPF

ecg_out2 =filter(hd2,ecg_out1);
ecg_out2= [ecg_out2(1:40)*0.25;ecg_out2(41:end)];
ecg_out2 = ecg_out2/max(abs(ecg_out2));
end