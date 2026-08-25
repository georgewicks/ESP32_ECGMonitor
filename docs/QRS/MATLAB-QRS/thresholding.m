function[Q_val,Q_loc,R_val,R_loc,S_val,S_loc,TH,w]=thresholding(ecg,ecg5)
%% Thresholding operation %%
TH = mean(ecg5)*max(ecg5); % Set threshold
w=(ecg5>(TH));

x=find(diff([0 w']) == 1); % Finding location of 0 to 1 transition
y=find(diff([w' 0]) == -1); % Finding location of 1 to 0 transition

%% cancelling the delay due to LOW PASS FILTER and HIGH PASS FILTER %%
x=x-(6+16); % 6 DELAY BY LPF & 16 DELAY BY HPF
y=y-(6+16);

%% Detect Q,R,S points %%
for i=1:length(x)
    %% R Locations %%
    [R_val(i),R_loc(i)]=max(ecg(x(i):y(i)));
    R_loc(i) = R_loc(i)-1 + x(i); % adding offset
    
    %% Q Locations %%
    [Q_val(i),Q_loc(i)]=min(ecg(R_loc(i):-1:R_loc(i)-8));
    Q_loc(i) = R_loc(i)-Q_loc(i)+1; % adding offset
    
    
    %% S Locations %%
    [S_val(i),S_loc(i)]=min(ecg(R_loc(i):R_loc(i)+10));
    S_loc(i) = R_loc(i)+S_loc(i)-1; % adding offset
end

