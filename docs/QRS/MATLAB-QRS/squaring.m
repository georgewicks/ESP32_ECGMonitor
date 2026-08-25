function [ecg_out4]=squaring(ecg_out3)
%% squaring operation %%

ecg_out4 = ecg_out3.^2;
ecg_out4 = ecg_out4/max(abs(ecg_out4));
end