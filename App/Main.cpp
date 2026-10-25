#include "../SDK/NetworkFilter.h"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <deque>
#include <fstream>
#include <memory>
#include <sstream>
#include <cmath>
namespace {
constexpr COLORREF Blue=RGB(38,96,245),Ink=RGB(20,30,48),Muted=RGB(91,105,125),Paper=RGB(245,247,251);
HWND window{},eventsView{},rulesView{},processBox{},portBox{},cidrBox{},directionBox{},protocolBox{},actionBox{},startButton{},stopButton{};
HFONT body{},heading{};HBRUSH background{},white{};bool active=false,preview=false;unsigned nextId=1;
NetworkFilter::Session session;std::vector<NF_RULE> rules;std::deque<NF_EVENT> events;
NF_STATUS status{},previous{};std::wstring note=L"Ready. Start monitoring, then add rules. No blocking policy is loaded.";
std::deque<double> rates;ULONGLONG lastUpdate=0;double incoming=0,outgoing=0;
void Text(HDC dc,int x,int y,const std::wstring& value,COLORREF color=Ink,HFONT font=nullptr){SelectObject(dc,font?font:body);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);TextOutW(dc,x,y,value.c_str(),static_cast<int>(value.size()));}
void Box(HDC dc,RECT rect,COLORREF color,int radius=14){HBRUSH brush=CreateSolidBrush(color);auto old=SelectObject(dc,brush);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,rect.left,rect.top,rect.right,rect.bottom,radius,radius);SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(brush);}
std::wstring Value(HWND control){std::array<wchar_t,256> data{};GetWindowTextW(control,data.data(),static_cast<int>(data.size()));return data.data();}
unsigned Number(HWND control,unsigned maximum){auto s=Value(control);if(s.empty())return 0;unsigned long long n=0;for(auto digit:s){if(digit<L'0'||digit>L'9')throw std::invalid_argument("PID and port must contain only digits.");n=n*10+static_cast<unsigned>(digit-L'0');if(n>maximum)throw std::invalid_argument("PID or port is out of range.");}return static_cast<unsigned>(n);}
void Error(const std::exception& e){std::string text=e.what();note=std::wstring(text.begin(),text.end());InvalidateRect(window,nullptr,FALSE);}
void Cell(HWND list,int row,int column,const std::wstring& value){LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=row;item.iSubItem=column;item.pszText=const_cast<wchar_t*>(value.c_str());if(!column)ListView_InsertItem(list,&item);else ListView_SetItem(list,&item);}
void Event(const NF_EVENT& e){
 if(events.size()==500){events.pop_front();ListView_DeleteItem(eventsView,0);}events.push_back(e);int row=ListView_GetItemCount(eventsView);
 Cell(eventsView,row,0,std::to_wstring(e.Sequence));Cell(eventsView,row,1,(e.Flags&NF_FLAG_PID)?std::to_wstring(e.ProcessId):L"Unknown");
 Cell(eventsView,row,2,e.Direction==NF_INBOUND?L"Inbound":L"Outbound");Cell(eventsView,row,3,e.Protocol==6?L"TCP":e.Protocol==17?L"UDP":std::to_wstring(e.Protocol));
 Cell(eventsView,row,4,NetworkFilter::Address(e));Cell(eventsView,row,5,e.Decision?L"BLOCKED":L"Continue");Cell(eventsView,row,6,e.RuleId?std::to_wstring(e.RuleId):L"Default");
}
void ListRules(){
 ListView_DeleteAllItems(rulesView);
 for(std::size_t i=0;i<rules.size();++i){const auto& r=rules[i];int row=static_cast<int>(i);
  Cell(rulesView,row,0,std::to_wstring(r.Id));Cell(rulesView,row,1,r.Action?L"Block":L"Continue");Cell(rulesView,row,2,r.ProcessId?std::to_wstring(r.ProcessId):L"Any process");
  Cell(rulesView,row,3,r.Direction==NF_INBOUND?L"Inbound":r.Direction==NF_OUTBOUND?L"Outbound":L"Both");
  Cell(rulesView,row,4,r.Protocol==6?L"TCP":r.Protocol==17?L"UDP":L"Any");Cell(rulesView,row,5,r.RemoteFirst?std::to_wstring(r.RemoteFirst):L"Any port");
  NF_EVENT e{};e.Family=r.Family;e.Flags=NF_FLAG_REMOTE_ADDRESS;std::copy(std::begin(r.RemoteAddress),std::end(r.RemoteAddress),e.RemoteAddress);
  Cell(rulesView,row,6,r.Family?NetworkFilter::Address(e)+L"/"+std::to_wstring(r.RemotePrefix):L"Any address");
 }
}
HWND Control(const wchar_t* type,const wchar_t* text,int id,DWORD style=0){auto h=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,10,10,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(body),TRUE);return h;}
HWND Button(const wchar_t* text,int id){return Control(L"BUTTON",text,id,BS_OWNERDRAW);}
void Combo(HWND box,const std::initializer_list<const wchar_t*>& values,int selected){for(auto text:values)SendMessageW(box,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));SendMessageW(box,CB_SETCURSEL,selected,0);}
HWND List(const std::initializer_list<std::pair<const wchar_t*,int>>& columns){
 HWND list=Control(WC_LISTVIEWW,L"",0,LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS);
 ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);unsigned i=0;
 for(const auto& col:columns){LVCOLUMNW column{};column.mask=LVCF_WIDTH|LVCF_TEXT;column.cx=col.second;column.pszText=const_cast<wchar_t*>(col.first);ListView_InsertColumn(list,static_cast<int>(i++),&column);}return list;
}
void Layout(){RECT r{};GetClientRect(window,&r);int width=r.right;
 MoveWindow(startButton,width-288,22,130,38,TRUE);MoveWindow(stopButton,width-148,22,124,38,TRUE);
 MoveWindow(GetDlgItem(window,3),width-174,264,150,36,TRUE);MoveWindow(GetDlgItem(window,4),width-174,308,150,34,TRUE);
 MoveWindow(processBox,24,314,100,32,TRUE);MoveWindow(directionBox,136,314,120,160,TRUE);MoveWindow(protocolBox,268,314,90,160,TRUE);
 MoveWindow(portBox,370,314,100,32,TRUE);MoveWindow(cidrBox,482,314,std::max(150,width-834),32,TRUE);MoveWindow(actionBox,width-340,314,150,160,TRUE);
 MoveWindow(rulesView,24,360,width-214,104,TRUE);MoveWindow(GetDlgItem(window,5),width-174,360,150,34,TRUE);
 MoveWindow(GetDlgItem(window,6),width-174,406,150,34,TRUE);MoveWindow(eventsView,24,514,width-48,std::max(90,static_cast<int>(r.bottom)-580),TRUE);
 MoveWindow(GetDlgItem(window,7),width-174,474,150,30,TRUE);
}
void Stop(){session.Stop();active=false;EnableWindow(startButton,!preview);EnableWindow(stopButton,FALSE);note=L"Stopped. All rules removed; normal traffic restored.";}
void Export(){
 wchar_t path[MAX_PATH]=L"network-events.csv";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFilter=L"CSV file\0*.csv\0\0";dialog.lpstrFile=path;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"csv";dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
 if(!GetSaveFileNameW(&dialog))return;
 std::ofstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot create the selected export file.");
 file<<"sequence,pid,direction,protocol,remote,decision,rule\n";
 for(const auto& e:events){auto wide=NetworkFilter::Address(e);std::string address;for(auto digit:wide)address.push_back(static_cast<char>(digit));file<<e.Sequence<<','<<e.ProcessId<<','<<unsigned(e.Direction)<<','<<unsigned(e.Protocol)<<','<<address<<','<<(e.Decision?"BLOCK":"CONTINUE")<<','<<e.RuleId<<'\n';}
 if(!file)throw std::runtime_error("CSV export failed while writing.");note=L"Displayed event metadata exported. No packet payload or application paths exported.";
}
void Paint(HDC dc){
 RECT r{};GetClientRect(window,&r);FillRect(dc,&r,background);Box(dc,{0,0,r.right,82},Ink,0);
 Text(dc,24,15,L"Titan Network Filter",RGB(255,255,255),heading);Text(dc,25,49,preview?L"OFFLINE PREVIEW / synthetic data / no driver access":L"Kernel connection policy / IPv4 + IPv6 / live IP accounting",RGB(192,207,230));
 int card=(r.right-72)/3;
 const std::array<std::wstring,3> labels{L"Observed IP bytes",L"Blocked authorizations",L"Dropped log records"};
 const std::array<std::wstring,3> values{std::to_wstring(status.BytesIn+status.BytesOut),std::to_wstring(status.Blocked),std::to_wstring(status.Dropped)};
 for(int i=0;i<3;++i){int x=24+i*(card+12);Box(dc,{x,98,x+card,176},RGB(255,255,255));Text(dc,x+16,108,labels[i],Muted);Text(dc,x+16,133,values[i],Blue,heading);}
 Box(dc,{24,188,r.right-24,250},RGB(255,255,255));Text(dc,40,202,L"IN "+std::to_wstring(static_cast<unsigned long long>(incoming))+L" B/s    OUT "+std::to_wstring(static_cast<unsigned long long>(outgoing))+L" B/s",Blue);
 if(rates.size()>1){const int left=r.right/2,right=r.right-42;double maximum=1;for(auto v:rates)maximum=std::max(maximum,v);HPEN pen=CreatePen(PS_SOLID,2,Blue);auto old=SelectObject(dc,pen);for(std::size_t i=0;i<rates.size();++i){int x=left+static_cast<int>(i)*(right-left)/63,y=238-static_cast<int>(rates[i]/maximum*38);if(i)LineTo(dc,x,y);else MoveToEx(dc,x,y,nullptr);}SelectObject(dc,old);DeleteObject(pen);}
 Text(dc,24,268,L"Connection rules",Ink,heading);Text(dc,24,292,L"PID (0 = any)     Direction           Protocol      Remote port      Remote IP / CIDR (empty = any)                       Action",Muted);
 Text(dc,24,476,L"Authorization events",Ink,heading);
 Text(dc,24,r.bottom-54,note,Muted);Text(dc,24,r.bottom-29,L"First matching rule wins. Continue still respects Windows Firewall. Rules affect new / reauthorized connections.",Muted);
}
LRESULT CALLBACK Proc(HWND h,UINT message,WPARAM w,LPARAM l){
 switch(message){
 case WM_CREATE:{
  body=CreateFontW(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");heading=CreateFontW(24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");background=CreateSolidBrush(Paper);white=CreateSolidBrush(RGB(255,255,255));
  window=h;startButton=Button(L"Start monitoring",1);stopButton=Button(L"Stop / restore",2);EnableWindow(stopButton,FALSE);
  Button(L"Add rule",3);Button(L"Remove selected",4);Button(L"Clear all rules",5);Button(L"Driver setup",6);Button(L"Export CSV",7);
  processBox=Control(L"EDIT",L"0",10,WS_BORDER|ES_NUMBER);portBox=Control(L"EDIT",L"0",11,WS_BORDER|ES_NUMBER);cidrBox=Control(L"EDIT",L"",12,WS_BORDER|ES_AUTOHSCROLL);
  for(auto box:{processBox,portBox,cidrBox})SendMessageW(box,EM_SETLIMITTEXT,128,0);
  directionBox=Control(L"COMBOBOX",L"",13,CBS_DROPDOWNLIST);Combo(directionBox,{L"Both",L"Inbound",L"Outbound"},2);
  protocolBox=Control(L"COMBOBOX",L"",14,CBS_DROPDOWNLIST);Combo(protocolBox,{L"Any",L"TCP",L"UDP"},1);
  actionBox=Control(L"COMBOBOX",L"",15,CBS_DROPDOWNLIST);Combo(actionBox,{L"Continue",L"Block"},1);
  rulesView=List({{L"ID",45},{L"Action",80},{L"Process",110},{L"Direction",100},{L"Protocol",80},{L"Port",80},{L"Remote CIDR",230}});
  eventsView=List({{L"Sequence",85},{L"PID",85},{L"Direction",105},{L"Protocol",85},{L"Remote endpoint",300},{L"Decision",120},{L"Rule",80}});
  if(preview){EnableWindow(startButton,FALSE);for(int id:{3,4,5})EnableWindow(GetDlgItem(h,id),FALSE);status.BytesIn=481920;status.BytesOut=84320;status.Blocked=12;note=L"Synthetic preview. Start the installed driver for actual measurements.";for(unsigned i=0;i<12;++i){NF_EVENT e{};e.Sequence=i+1;e.ProcessId=1042+i;e.Family=4;e.Direction=2;e.Protocol=6;e.RemotePort=443;e.RemoteAddress[0]=203;e.RemoteAddress[1]=0;e.RemoteAddress[2]=113;e.RemoteAddress[3]=static_cast<NF_U8>(i+1);e.Flags=NF_FLAG_PID|NF_FLAG_PORTS|NF_FLAG_REMOTE_ADDRESS;e.Decision=i%3==0?1:0;e.RuleId=e.Decision?1:0;Event(e);}for(unsigned i=0;i<64;++i)rates.push_back(40000+20000*std::sin(i*.2));}
  SetTimer(h,1,250,nullptr);Layout();return 0;
 }
 case WM_SIZE:Layout();InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={1120,740};return 0;
 case WM_COMMAND:if(HIWORD(w)==BN_CLICKED)try{
  switch(LOWORD(w)){
   case 1:session.Start();try{session.SetRules(rules);}catch(...){session.Stop();throw;}active=true;lastUpdate=GetTickCount64();previous={};events.clear();ListView_DeleteAllItems(eventsView);EnableWindow(startButton,FALSE);EnableWindow(stopButton,TRUE);note=L"Monitoring active. Blocking rules are evaluated directly in the kernel.";break;
   case 2:Stop();break;
   case 3:{
    if(rules.size()==NF_RULE_CAPACITY)throw std::runtime_error("Maximum of 64 rules reached.");
    unsigned selected=static_cast<unsigned>(SendMessageW(protocolBox,CB_GETCURSEL,0,0));unsigned protocol=selected==1?6u:selected==2?17u:0u;
    auto candidate=rules;candidate.push_back(NetworkFilter::Rule(nextId,static_cast<unsigned>(SendMessageW(actionBox,CB_GETCURSEL,0,0)),protocol,Number(portBox,65535),Value(cidrBox),Number(processBox,0xFFFFFFFFu),static_cast<unsigned>(SendMessageW(directionBox,CB_GETCURSEL,0,0))));
    if(active)session.SetRules(candidate);rules=std::move(candidate);++nextId;ListRules();note=active?L"Policy replaced atomically. New / reauthorized connections use these rules.":L"Rule prepared. Start monitoring to enable this policy.";break;
   }
   case 4:{int selected=ListView_GetNextItem(rulesView,-1,LVNI_SELECTED);if(selected<0)throw std::runtime_error("Select the rule to remove.");auto candidate=rules;candidate.erase(candidate.begin()+selected);if(active)session.SetRules(candidate);rules=std::move(candidate);ListRules();break;}
   case 5:if(active)session.SetRules({});rules.clear();ListRules();note=L"All rules cleared. Connections continue under the remaining Windows policy.";break;
   case 6:{wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);std::wstring setup=path;setup.resize(setup.find_last_of(L"\\/")+1);setup+=L"NetworkFilterSetup.exe";if(reinterpret_cast<INT_PTR>(ShellExecuteW(h,L"open",setup.c_str(),nullptr,nullptr,SW_SHOW))<=32)throw std::runtime_error("Place the signed NetworkFilterSetup.exe beside this application. Available after Microsoft signing.");break;}
   case 7:Export();break;
  }
  InvalidateRect(h,nullptr,FALSE);
 }catch(const std::exception& e){Error(e);}return 0;
 case WM_TIMER:if(active)try{for(unsigned batch=0;batch<8;++batch){auto items=session.Read();for(const auto& e:items)Event(e);if(items.size()<NF_READ_CAPACITY)break;}status=session.Status();auto now=GetTickCount64();double seconds=std::max(.001,double(now-lastUpdate)/1000.);incoming=double(status.BytesIn-previous.BytesIn)/seconds;outgoing=double(status.BytesOut-previous.BytesOut)/seconds;previous=status;lastUpdate=now;if(rates.size()==64)rates.pop_front();rates.push_back(incoming+outgoing);InvalidateRect(h,nullptr,FALSE);}catch(const std::exception& e){Stop();Error(e);}return 0;
 case WM_DRAWITEM:{auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(item->CtlType!=ODT_BUTTON)break;bool enabled=IsWindowEnabled(item->hwndItem)!=FALSE;Box(item->hDC,item->rcItem,enabled?Blue:RGB(219,225,236),10);wchar_t text[80]{};GetWindowTextW(item->hwndItem,text,80);SelectObject(item->hDC,body);SetTextColor(item->hDC,enabled?RGB(255,255,255):Muted);SetBkMode(item->hDC,TRANSPARENT);DrawTextW(item->hDC,text,-1,&item->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(item->itemState&ODS_FOCUS){RECT focus=item->rcItem;InflateRect(&focus,-4,-4);DrawFocusRect(item->hDC,&focus);}return TRUE;}
 case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT&&reinterpret_cast<HWND>(w)!=h){wchar_t type[40]{};GetClassNameW(reinterpret_cast<HWND>(w),type,40);if(!_wcsicmp(type,L"Button")||!_wcsicmp(type,L"ComboBox")){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}break;
 case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:SetTextColor(reinterpret_cast<HDC>(w),Ink);SetBkColor(reinterpret_cast<HDC>(w),RGB(255,255,255));return reinterpret_cast<LRESULT>(white);
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT paint{};HDC dc=BeginPaint(h,&paint);RECT r{};GetClientRect(h,&r);HDC buffer=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto old=SelectObject(buffer,bitmap);Paint(buffer);BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(h,&paint);return 0;}
 case WM_DESTROY:KillTimer(h,1);session.Stop();DeleteObject(body);DeleteObject(heading);DeleteObject(background);DeleteObject(white);PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show){
 SetProcessDPIAware();preview=std::wstring(command).find(L"--preview")!=std::wstring::npos;INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&controls);
 WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=instance;c.lpszClassName=L"TitanNetworkFilter";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);
 window=CreateWindowW(c.lpszClassName,L"Titan Network Filter",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1200,820,nullptr,nullptr,instance,nullptr);if(!window)return 1;ShowWindow(window,show);
 MSG message{};while(GetMessageW(&message,nullptr,0,0)>0)if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}return 0;
}
