function varargout = Main_Project(varargin)
% MAIN_PROJECT MATLAB code for Main_Project.fig
%      MAIN_PROJECT, by itself, creates a new MAIN_PROJECT or raises the existing
%      singleton*.
%
%      H = MAIN_PROJECT returns the handle to a new MAIN_PROJECT or the handle to
%      the existing singleton*.
%
%      MAIN_PROJECT('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in MAIN_PROJECT.M with the given input arguments.
%
%      MAIN_PROJECT('Property','Value',...) creates a new MAIN_PROJECT or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before Main_Project_OpeningFcn gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to Main_Project_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES

% Edit the above text to modify the response to help Main_Project

% Last Modified by GUIDE v2.5 06-Jul-2021 22:07:44

% Begin initialization code - DO NOT EDIT
gui_Singleton = 1;
gui_State = struct('gui_Name',       mfilename, ...
    'gui_Singleton',  gui_Singleton, ...
    'gui_OpeningFcn', @Main_Project_OpeningFcn, ...
    'gui_OutputFcn',  @Main_Project_OutputFcn, ...
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


% --- Executes just before Main_Project is made visible.
function Main_Project_OpeningFcn(hObject, eventdata, handles, varargin)
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to Main_Project (see VARARGIN)

% Choose default command line output for Main_Project
handles.output = hObject;

% Update handles structure
guidata(hObject, handles);

% UIWAIT makes Main_Project wait for user response (see UIRESUME)
% uiwait(handles.figure1);


% --- Outputs from this function are returned to the command line.
function varargout = Main_Project_OutputFcn(hObject, eventdata, handles)
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
ecg=ecg-mean(ecg);
ecg=ecg/max(abs(ecg));
fs=str2double(get(handles.edit4, 'String'));
slen=length(ecg);
t=(1:slen)/fs;
P1=subplot(6,1,1);
plot(t,ecg);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Original ECG Signal');

%% Bandpass filtering %%
ecg_out2=bandpass_filtering(ecg);
P2=subplot(6,1,2);
plot(t,ecg_out2);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('BPF Block Output');
%% Derivative Operation %%
ecg_out3=differentiator(ecg_out2);
P3=subplot(6,1,3);
plot(t,ecg_out3);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Derivative Block Output');
%% squaring operation %%
ecg_out4=squaring(ecg_out3);
P4=subplot(6,1,4);
plot(t,ecg_out4);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Squaring Block Output');

%% Integration operation %%
ecg5=integration(ecg_out4);
P5=subplot(6,1,5);
plot(t,ecg5);
xlim([0,10]);
grid on;
grid minor;
hold on;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Integration Block Output');
%% Thresholding operation %%
[Q_val,Q_loc,R_val,R_loc,S_val,S_loc,TH,w]=thresholding(ecg,ecg5);
P6=subplot(6,1,6);
plot(t,ecg);
xlim([0,10]);
grid on;
grid minor;
hold on;
plot(t(Q_loc),Q_val,'b+');
plot(t(R_loc),R_val,'r*');
plot(t(S_loc),S_val,'go');
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('ECG Signal with QRS Locations after Thresholding');
legend('ECG','Q','R','S');
hold off;
set(handles.edit3,'string',TH);
disp(['Threshold Value = ' num2str(TH) ' mV']);

%% Calculation of Heart Rate and QRS Duration %%
[HT,dur]=heart_rate_qrs_duration(R_loc,t,fs,slen,w);

set(handles.edit1,'string',dur); % Display QRS Duration
disp(['QRS Duration for ecg = ' num2str(dur) ' sec']);

set(handles.edit2,'string',HT); % Display Heart Rate
disp(['Heart Rate for ecg = ' num2str(HT) ' beats/min']);

%% calculation of FORM FACTOR and R-To-R Interval
[FF,RR,RR_loc]=FF_RR_calculations(Q_loc,ecg,R_loc,fs);
%% SAVING PROGRESS TO WORKSPACE %%
assignin('base','P1',P1);
assignin('base','P2',P2);
assignin('base','P3',P3);
assignin('base','P4',P4);
assignin('base','P5',P5);
assignin('base','P6',P6);
assignin('base','t',t);
assignin('base','ecg',ecg);
assignin('base','ecg_out2',ecg_out2);
assignin('base','ecg_out3',ecg_out3);
assignin('base','ecg_out4',ecg_out4);
assignin('base','ecg5',ecg5);
assignin('base','Q_loc',Q_loc);
assignin('base','R_loc',R_loc);
assignin('base','S_loc',S_loc);
assignin('base','Q_val',Q_val);
assignin('base','R_val',R_val);
assignin('base','S_val',S_val);
assignin('base','TH',TH);
assignin('base','RR',RR);
assignin('base','FF',FF);
assignin('base','fs',fs);
assignin('base','RR_loc',RR_loc);
% --- Executes on mouse press over axes background.
function axes1_ButtonDownFcn(hObject, eventdata, handles)
% hObject    handle to axes1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)



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


% --- Executes on button press in pushbutton2.
function pushbutton2_Callback(hObject, eventdata, handles)
% hObject    handle to pushbutton2 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
%% Retriving from Workspace %%
P1 = evalin('base','P1');
P2 = evalin('base','P2');
P3 = evalin('base','P3');
P4 = evalin('base','P4');
P5 = evalin('base','P5');
P6 = evalin('base','P6');
%% CLEARING ALL %%
cla(P1);
cla(P2);
cla(P3);
cla(P4);
cla(P5);
cla(P6);
reset='';
set(handles.edit1,'string',reset);
set(handles.edit2,'string',reset);
set(handles.edit3,'string',reset);
set(handles.edit4,'string',reset);

clc;
evalin('base', 'clear variables');


% --- Executes on button press in FF_RR_PLOT.
function FF_RR_PLOT_Callback(hObject, eventdata, handles)
% hObject    handle to FF_RR_PLOT (see GCBO)
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
RR_loc = evalin('base','RR_loc');
%% plot with FF values and RR intervals %%
figure;
plot(t,ecg,t(Q_loc),Q_val,'+',t(R_loc),R_val,'*',t(S_loc),S_val,'o');
hold on;
end1 = find(Q_loc,1,'last'); % Q locations within considered time
f1=max(ecg)-0.02; %location in plot for FF interval values
f2=max(ecg)-0.19; %location in plot for RR interval values
% text(min(t)-0.5,f1,'FF Values -->'); % text 'FF' in plot
% text(min(t)-0.5,f2,'RR Values -->'); % text 'FF' in plot
for j=2:(end1-1)
    text(t(Q_loc(j)),f1,num2str(FF(j))); % writing FF values in the plot
    RR_average(j)=floor((R_loc(j)+R_loc(j+1))/2);
    text(t(RR_average(j)),f2,num2str(RR(j))); % writing RR values in the plot
end
assignin('base','RR_average',RR_average);
legend('ECG','Q','R','S');
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('ECG Signal with Form Factor and R-R interval');

%% n is the width of the axis 'window'
n=10;
a=gca;
% Set appropriate axis limits and settings
set(gcf,'doublebuffer','on');
%% This avoids flickering when updating the axis
set(a,'xlim',[0 n]);
set(a,'ylim',[min(ecg) max(ecg)+1]);

% Generate constants for use in uicontrol initialization
pos=get(a,'position');
Newpos=[pos(1) pos(2)-0.1 pos(3) 0.05];
%% This will create a slider which is just underneath the axis
%% but still leaves room for the axis labels above the slider
xmax=max(t);
S=['set(gca,''xlim'',get(gcbo,''value'')+[0 ' num2str(n) '])'];
%% Setting up callback string to modify XLim of axis (gca)
%% based on the position of the slider (gcbo)

% Creating Uicontrol
h=uicontrol('style','slider',...
    'units','normalized','position',Newpos,...
    'callback',S,'min',0,'max',xmax-n);


% --- Executes on selection change in popupmenu1.
function popupmenu1_Callback(hObject, eventdata, handles)
% hObject    handle to popupmenu1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Hints: contents = cellstr(get(hObject,'String')) returns popupmenu1 contents as cell array
%        contents{get(hObject,'Value')} returns selected item from popupmenu1


% --- Executes during object creation, after setting all properties.
function popupmenu1_CreateFcn(hObject, eventdata, handles)
% hObject    handle to popupmenu1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called

% Hint: popupmenu controls usually have a white background on Windows.
%       See ISPC and COMPUTER.
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end


% --- Executes on button press in original_ecg.
function original_ecg_Callback(hObject, eventdata, handles)
% hObject    handle to original_ecg (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
t = evalin('base','t');
ecg = evalin('base','ecg');
figure;
plot(t,ecg);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Original ECG Signal');

% --- If Enable == 'on', executes on mouse press in 5 pixel border.
% --- Otherwise, executes on mouse press in 5 pixel border or over original_ecg.
function original_ecg_ButtonDownFcn(hObject, eventdata, handles)
% hObject    handle to original_ecg (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)


% --- Executes on button press in BPF_OUTPUT.
function BPF_OUTPUT_Callback(hObject, eventdata, handles)
% hObject    handle to BPF_OUTPUT (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
t = evalin('base','t');
ecg_out2 = evalin('base','ecg_out2');
figure;
plot(t,ecg_out2);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('BPF Block Output');


% --- Executes on button press in DERIVATIVE_OUTPUT.
function DERIVATIVE_OUTPUT_Callback(hObject, eventdata, handles)
% hObject    handle to DERIVATIVE_OUTPUT (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
t = evalin('base','t');
ecg_out3 = evalin('base','ecg_out3');
figure;
plot(t,ecg_out3);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Derivative Block Output');


% --- Executes on button press in SQUARING_OUTPUT.
function SQUARING_OUTPUT_Callback(hObject, eventdata, handles)
% hObject    handle to SQUARING_OUTPUT (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
t = evalin('base','t');
ecg_out4 = evalin('base','ecg_out4');
figure;
plot(t,ecg_out4);
xlim([0,10]);
grid on;
grid minor;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Squaring Block Output');


% --- Executes on button press in INTEGRATION_OUTPUT.
function INTEGRATION_OUTPUT_Callback(hObject, eventdata, handles)
% hObject    handle to INTEGRATION_OUTPUT (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
t = evalin('base','t');
ecg5 = evalin('base','ecg5');
TH = evalin('base','TH');
figure;
plot(t,ecg5);
xlim([0,10]);
grid on;
grid minor;
hold on;
show_val=yline(TH,'--b','Threshold','Linewidth',2);
show_val.LabelHorizontalAlignment = 'left';
text(t(1),TH,num2str(TH));
hold off;
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('Integration Block Output');


% --- Executes on button press in QRS_AFTER_THRESHOLDING.
function QRS_AFTER_THRESHOLDING_Callback(hObject, eventdata, handles)
% hObject    handle to QRS_AFTER_THRESHOLDING (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
t = evalin('base','t');
TH = evalin('base','TH');
ecg = evalin('base','ecg');
Q_loc = evalin('base','Q_loc');
R_loc = evalin('base','R_loc');
S_loc = evalin('base','S_loc');
Q_val = evalin('base','Q_val');
R_val = evalin('base','R_val');
S_val = evalin('base','S_val');
figure;
plot(t,ecg);
xlim([0,10]);
grid on;
grid minor;
hold on;
plot(t(Q_loc),Q_val,'b+');
plot(t(R_loc),R_val,'r*');
plot(t(S_loc),S_val,'go');
show_val=yline(TH,'--b','Threshold','Linewidth',2);
show_val.LabelHorizontalAlignment = 'left';
text(t(1),TH,num2str(TH));
xlabel('Time (seconds)')
ylabel('Signal Amplitude');
title('ECG Signal with QRS Locations after Thresholding');
legend('ECG','Q','R','S');
hold off;



function edit3_Callback(hObject, eventdata, handles)
% hObject    handle to edit3 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Hints: get(hObject,'String') returns contents of edit3 as text
%        str2double(get(hObject,'String')) returns contents of edit3 as a double


% --- Executes during object creation, after setting all properties.
function edit3_CreateFcn(hObject, eventdata, handles)
% hObject    handle to edit3 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called

% Hint: edit controls usually have a white background on Windows.
%       See ISPC and COMPUTER.
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end



function edit4_Callback(hObject, eventdata, handles)
% hObject    handle to edit4 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Hints: get(hObject,'String') returns contents of edit4 as text
%        str2double(get(hObject,'String')) returns contents of edit4 as a double


% --- Executes during object creation, after setting all properties.
function edit4_CreateFcn(hObject, eventdata, handles)
% hObject    handle to edit4 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called

% Hint: edit controls usually have a white background on Windows.
%       See ISPC and COMPUTER.
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end
