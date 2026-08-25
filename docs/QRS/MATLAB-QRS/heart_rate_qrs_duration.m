function[HT,dur]=heart_rate_qrs_duration(R_loc,t,slen,w,fs)
%% Calculation of HEART RATE %%
HT=ceil((length(R_loc)*60)/t(end)); % calculate Heart rate
%% Calculation of QRS Duration %%
ecg6 = zeros(slen,1);
ecg6(w)=1;
ecg7 = diff([ecg6;0]);

x1= find(ecg7>0);
y1 = find(ecg7<0);

z1=y1-x1;
dur=mean(z1)*(1/fs); % Calculate QRS duration
end