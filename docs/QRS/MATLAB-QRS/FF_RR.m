function varargout = FF_RR(varargin)
% FF_RR MATLAB code for FF_RR.fig
%      FF_RR, by itself, creates a new FF_RR or raises the existing
%      singleton*.
%
%      H = FF_RR returns the handle to a new FF_RR or the handle to
%      the existing singleton*.
%
%      FF_RR('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in FF_RR.M with the given input arguments.
%
%      FF_RR('Property','Value',...) creates a new FF_RR or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before FF_RR_OpeningFcn gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to FF_RR_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES

% Edit the above text to modify the response to help FF_RR

% Last Modified by GUIDE v2.5 05-Jul-2021 04:38:01

% Begin initialization code - DO NOT EDIT
gui_Singleton = 1;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @FF_RR_OpeningFcn, ...
                   'gui_OutputFcn',  @FF_RR_OutputFcn, ...
                   'gui_LayoutFcn',  [] , ...
                   'gui_Callback',   []);
if nargin && ischar(varargin{1})
    gui_State.gui_Callback = str2func(varargin{1});
end

if nargout
    [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
else
    gui_mainfcn(gui_State, varargin{:});
end
% End initialization code - DO NOT EDIT


% --- Executes just before FF_RR is made visible.
function FF_RR_OpeningFcn(hObject, eventdata, handles, varargin)
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to FF_RR (see VARARGIN)

% Choose default command line output for FF_RR
handles.output = hObject;

% Update handles structure
guidata(hObject, handles);

% UIWAIT makes FF_RR wait for user response (see UIRESUME)
% uiwait(handles.figure1);


% --- Outputs from this function are returned to the command line.
function varargout = FF_RR_OutputFcn(hObject, eventdata, handles) 
% varargout  cell array for returning output args (see VARARGOUT);
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Get default command line output from handles structure
varargout{1} = handles.output;


% --- Executes on button press in pushbutton1.
function pushbutton1_Callback(hObject, eventdata, handles)
% hObject    handle to pushbutton1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
%% read ECG data %%
[filename,filepath]=uigetfile({'*.*','All Files'},...
    'Select Data File 1');
cd(filepath);
ecg=load([filepath, filename]);
guidata(hObject, handles);
%% Set parameters %%
ecg=ecg-mean(ecg);
ecg=ecg/max(abs(ecg));
fs=200;
slen=length(ecg);
t=(1:slen)/fs;

% axes(handles.axes1);
% plot(t,ecg);
% xlim([0,8]);
% xlabel('Time (seconds)')
% ylabel('Signal Amplitude');
% title('Original ECG Signal');
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


%% derivative operator %%

b3= [2 1 0 -1 -2];
a3 = [1 ] *8;

%% creating digital filter (Direct form II)

hd3 = dfilt.df2(b3,a3);

%% filtering the ECG signal from derivative operator %%

ecg_out3 = filter(hd3,ecg_out2);
% ecg_out = ecg_out3/max(abs(ecg_out3));


%% squaring operation %%

ecg_out4 = ecg_out3.^2;
ecg_out4 = ecg_out4/max(abs(ecg_out4));

%% moving window integration operation %%

ecg_out4pad = [zeros(1,29) ecg_out4' zeros(1,29)];

for i=30:length(ecg_out4pad)-29
    ecg5(i-29) = sum(ecg_out4pad(i-29:i))/30;
end

ecg5 = ecg5';
%% thresholding operation %%

TH = mean(ecg5);
ecg6 = zeros(slen,1);
w=(ecg5>(TH));
ecg6(w) = 1;
x=find(diff([0 w']) == 1);
y=find(diff([w' 0]) == -1);

%% cancelling the delay due to LOW PASS and HIGH PASS %%

x=x-(6+16);
y=y-(6+16);

%% detect R,Q,S, points %%
for i=1:length(x)
    [R_val(i),R_loc(i)]=max(ecg(x(i):y(i)));
    R_loc(i) = R_loc(i)-1 + x(i);
    
    [Q_val(i),Q_loc(i)]=min(ecg(R_loc(i):-1:R_loc(i)-8));
    Q_loc(i) = R_loc(i) - Q_loc(i)+1;
    
    [S_val(i),S_loc(i)]=min(ecg(R_loc(i):R_loc(i)+10));
    S_loc(i) = R_loc(i) + S_loc(i)-1;
end



%% calculation of QRS duration and heart rate %%

count=0;
for i=1:slen-1
    if(ecg6(i) == 0 && ecg6(i+1)>=1 && ecg6(i+2) >=1)
        count = count + 1;
    end
end

ecg7 = diff([ecg6;0]);

x1= find(ecg7>0);
y1 = find(ecg7<0);

z1=y1-x1;
dur=mean(z1)*(1/fs);
HT=count*3;
%% calculation of FORM FACTOR %%
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
RR=[diff(R_loc),0];
%% plot with FF values and RR intervals %%
plot(t,ecg,t(Q_loc),Q_val,'^',t(R_loc),R_val,'*',t(S_loc),S_val,'o');
% xlim([0,8]);
hold on;
axis([0.05 10.005 -1 1]);
end1 = find(Q_loc < 10.005*fs,1,'last'); % Q locations within considered time
f1=max(ecg)-0.02; %location in plot for FF interval values
f2=max(ecg)-0.19; %location in plot for RR interval values
% text(min(t)-0.5,f1,'FF Values -->'); % text 'FF' in plot
% text(min(t)-0.5,f2,'RR Values -->'); % text 'FF' in plot
for j=2:(end1-1)
    text(t(Q_loc(j)),f1,num2str(FF(j))); % writing FF values in the plot
    text(t(Q_loc(j)),f2,num2str(RR(j))); % writing RR values in the plot
end
legend('ECG','Q','R','S');
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('ECG Signal with Form Factor and R-R interval');

assignin('base','t',t);
assignin('base','ecg',ecg);
assignin('base','Q_loc',Q_loc);
assignin('base','R_loc',R_loc);
assignin('base','S_loc',S_loc);
assignin('base','Q_val',Q_val);
assignin('base','R_val',R_val);
assignin('base','S_val',S_val);
assignin('base','FF',FF);
assignin('base','RR',RR);
assignin('base','end1',end1);
assignin('base','fs',fs);
% --- Executes on mouse press over axes background.
function axes1_ButtonDownFcn(hObject, eventdata, handles)
% hObject    handle to axes1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)



% --- Executes on button press in pushbutton2.
function pushbutton2_Callback(hObject, eventdata, handles)
% hObject    handle to pushbutton2 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

%% retriving parameter values %%
t = evalin('base','t');
ecg = evalin('base','ecg');
Q_loc = evalin('base','Q_loc');
R_loc = evalin('base','R_loc');
S_loc = evalin('base','S_loc');
Q_val = evalin('base','Q_val');
R_val = evalin('base','R_val');
S_val = evalin('base','S_val');
FF = evalin('base','FF');
RR = evalin('base','RR');
fs = evalin('base','fs');
%% plot with FF values and RR intervals %%
m = str2double(get(handles.edit1, 'String'));
n = str2double(get(handles.edit2, 'String'));
assignin('base','m',m);
assignin('base','n',n);
axes(handles.axes1)
set(handles.axes1, 'XLim', [m, n])
plot(t,ecg,t(Q_loc),Q_val,'^',t(R_loc),R_val,'*',t(S_loc),S_val,'o');
hold on;
% axis([m n -1 1]);
end1 = find(Q_loc < n*fs,1,'last');
f1=max(ecg)-0.02; %location in plot for FF interval values
f2=max(ecg)-0.19; %location in plot for RR interval values
% text(min(t)-0.5,f1,'FF Values -->'); % text 'FF' in plot
% text(min(t)-0.5,f2,'RR Values -->'); % text 'FF' in plot
for j=2:(end1-1)
    text(t(Q_loc(j)),f1,num2str(FF(j))); % writing FF values in the plot
    text(t(Q_loc(j)),f2,num2str(RR(j))); % writing RR values in the plot
end
legend('ECG','Q','R','S');
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('ECG Signal with Form Factor and R-R interval');



function edit1_Callback(hObject, eventdata, handles)
% hObject    handle to edit1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Hints: get(hObject,'String') returns contents of edit1 as text
%        str2double(get(hObject,'String')) returns contents of edit1 as a double


% --- Executes during object creation, after setting all properties.
function edit1_CreateFcn(hObject, eventdata, handles)
% hObject    handle to edit1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called

% Hint: edit controls usually have a white background on Windows.
%       See ISPC and COMPUTER.
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end



function edit2_Callback(hObject, eventdata, handles)
% hObject    handle to edit2 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Hints: get(hObject,'String') returns contents of edit2 as text
%        str2double(get(hObject,'String')) returns contents of edit2 as a double


% --- Executes during object creation, after setting all properties.
function edit2_CreateFcn(hObject, eventdata, handles)
% hObject    handle to edit2 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called

% Hint: edit controls usually have a white background on Windows.
%       See ISPC and COMPUTER.
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end


% --- Executes on button press in pushbutton3.
function pushbutton3_Callback(hObject, eventdata, handles)
% hObject    handle to pushbutton3 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
cla(handles.axes1,'reset');
xmin_reset='';
xmax_reset='';
set(handles.edit1,'string',xmin_reset);
set(handles.edit2,'string',xmax_reset);
clc;
