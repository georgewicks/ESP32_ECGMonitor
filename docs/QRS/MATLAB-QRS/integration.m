function [ecg5]=integration(ecg_out4)
%% moving window integration operation %%

ecg_out4pad = [zeros(1,29) ecg_out4' zeros(1,29)];

for i=30:length(ecg_out4pad)-29
    ecg_int(i-29) = sum(ecg_out4pad(i-29:i))/30;
end

ecg5 = ecg_int';
ecg5 =ecg5/max(abs(ecg5));
end
