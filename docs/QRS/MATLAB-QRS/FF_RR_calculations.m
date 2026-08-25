function[FF,RR,RR_loc]=FF_RR_calculations(Q_loc,ecg,R_loc,fs)
%% Calculation of FF (Form Factor) %%
for i=1:(length(Q_loc)-1)
    sig = ecg(Q_loc(i):Q_loc(i)+80); % 80 represents span of 400ms (one ECG cycle)
    sig_d = diff(sig); % first derivative
    var1 = var(sig); % signal variance
    var2 = var(sig_d); % variance of first derivative
    var2d = var(diff(sig_d)); % variance of second derivative
    Mx = sqrt(var2/var1);
    Mxd = sqrt(var2d/var2);
    FF(i) = Mxd/ Mx;
end
%% Calculation of RR interval %%
RR_loc=[diff(R_loc),0]; % R-R Duration in Samples
RR=RR_loc/fs; % R-R Duration in Seconds