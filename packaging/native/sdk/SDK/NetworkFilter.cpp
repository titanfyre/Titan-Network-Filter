#include "NetworkFilter.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <setupapi.h>
#include <fwpmu.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <system_error>
#include <cstring>
namespace NetworkFilter {
namespace {
const GUID Interface={0x46daaf11,0x2408,0x4882,{0xaf,0x6b,0x53,0xd5,0x21,0x39,0xa6,0x14}};
const GUID Provider={0x6a780010,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}};
const GUID Sublayer={0x6a780011,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}};
const GUID Keys[8]={
 {0x6a780000,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780001,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780002,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780003,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780004,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780005,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780006,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780007,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}}
};
const GUID* Layers[8]={&FWPM_LAYER_ALE_AUTH_CONNECT_V4,&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
 &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
 &FWPM_LAYER_INBOUND_IPPACKET_V4,&FWPM_LAYER_INBOUND_IPPACKET_V6,
 &FWPM_LAYER_OUTBOUND_IPPACKET_V4,&FWPM_LAYER_OUTBOUND_IPPACKET_V6};
std::string Error(DWORD e){
 const char* message=e==ERROR_ACCESS_DENIED?"Access denied; run as administrator":
 e==ERROR_NO_MORE_ITEMS||e==ERROR_FILE_NOT_FOUND?"Network filter driver is not installed or running":
 e==ERROR_SHARING_VIOLATION||e==ERROR_BUSY?"Another application owns the network filter":
 e==RPC_S_SERVER_UNAVAILABLE?"Windows Base Filtering Engine is unavailable":"Windows operation failed";
 return std::string(message)+" (Windows error "+std::to_string(e)+")";
}
[[noreturn]] void Fail(DWORD e,const char* stage){throw std::runtime_error(std::string(stage)+": "+Error(e));}
void Check(DWORD e,const char* stage){if(e)Fail(e,stage);}
HANDLE Open(){
 HDEVINFO devices=SetupDiGetClassDevsW(&Interface,nullptr,nullptr,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
 if(devices==INVALID_HANDLE_VALUE)Fail(GetLastError(),"Find driver interface");
 struct Cleanup{HDEVINFO value;~Cleanup(){SetupDiDestroyDeviceInfoList(value);}} cleanup{devices};
 SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof(iface);
 if(!SetupDiEnumDeviceInterfaces(devices,nullptr,&Interface,0,&iface))Fail(GetLastError(),"Driver not installed or not running");
 DWORD size=0;SetupDiGetDeviceInterfaceDetailW(devices,&iface,nullptr,0,&size,nullptr);
 if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||size>65536)Fail(ERROR_INVALID_DATA,"Interface size");
 std::vector<unsigned char> storage(size);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());detail->cbSize=sizeof(*detail);
 if(!SetupDiGetDeviceInterfaceDetailW(devices,&iface,detail,size,nullptr,nullptr))Fail(GetLastError(),"Read driver interface");
 HANDLE device=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(device==INVALID_HANDLE_VALUE)Fail(GetLastError(),"Open sensor (administrator rights and one active instance required)");
 return device;
}
void Control(HANDLE device,DWORD code,void* output,DWORD size,NF_U32 options=0){
 NF_REQUEST input{NF_MAGIC,NF_VERSION,sizeof(NF_REQUEST),options};DWORD received=0;
 if(!DeviceIoControl(device,code,&input,sizeof(input),output,size,&received,nullptr))Fail(GetLastError(),"Driver request");
 if(received!=size)Fail(ERROR_INVALID_DATA,"Driver response length");
}
}
bool Validate(const NF_RULE& r) noexcept{
    unsigned i,bytes;
    if(!r.Id || (r.Family!=0&&r.Family!=4&&r.Family!=6) || r.Direction>2 || r.Action>1)return 0;
    if(r.Reserved[0]||r.Reserved[1]||r.Reserved[2])return 0;
    if((r.LocalFirst==0)!=(r.LocalLast==0)||(r.RemoteFirst==0)!=(r.RemoteLast==0))return 0;
    if(r.LocalFirst>r.LocalLast||r.RemoteFirst>r.RemoteLast)return 0;
    if((r.LocalFirst||r.RemoteFirst)&&r.Protocol!=6&&r.Protocol!=17)return 0;
    if(r.RemotePrefix>(r.Family==4?32:r.Family==6?128:0))return 0;
    bytes=r.Family==4?4:r.Family==6?16:0;
    for(i=bytes;i<16;++i)if(r.RemoteAddress[i])return 0;
    /* Reject CIDRs with nonzero host bits, avoiding misleading UI policies. */
    for(i=0;i<bytes;++i){
        unsigned start=i*8,mask=r.RemotePrefix>=start+8?255:r.RemotePrefix<=start?0:256-(1u<<(8-(r.RemotePrefix-start)));
        if(r.RemoteAddress[i]&~mask)return 0;
    }
    return 1;
}
bool Validate(const NF_POLICY& p) noexcept{
 if(p.Magic!=NF_MAGIC||p.Version!=NF_VERSION||p.Size!=sizeof(p)||p.Count>NF_RULE_CAPACITY)return false;
 for(unsigned i=0;i<p.Count;++i){if(!Validate(p.Rules[i]))return false;for(unsigned j=0;j<i;++j)if(p.Rules[i].Id==p.Rules[j].Id)return false;}
 return true;
}
bool Validate(const NF_STATUS& s) noexcept{return s.Magic==NF_MAGIC&&s.Version==NF_VERSION&&s.Size==sizeof(s)&&s.Active<=1&&s.Capacity==NF_RING_CAPACITY&&s.Available<=s.Capacity&&s.QpcFrequency&&s.Dropped<=s.Produced&&!(s.Options&~NF_OPTION_INCLUDE_PATHS)&&s.RuleCount<=NF_RULE_CAPACITY;}
bool Validate(const NF_BATCH& b,std::size_t bytes) noexcept{
 if(bytes!=sizeof(b)||b.Magic!=NF_MAGIC||b.Version!=NF_VERSION||b.Size!=sizeof(b)||b.Count>NF_READ_CAPACITY)return false;
 NF_U64 previous=0;
 for(unsigned i=0;i<b.Count;++i){const auto& e=b.Events[i];
  if(!e.Sequence||e.Sequence<=previous||!e.TimestampQpc||(e.Family!=4&&e.Family!=6)||e.Stage!=NF_STAGE_AUTHORIZE||e.Direction<1||e.Direction>2||e.Decision>1||(e.Flags&~509u)||e.PathLength>=NF_PATH_CAPACITY||!e.Generation)return false;
  if(e.Decision==NF_ACTION_BLOCK&&!e.RuleId)return false;
  if(!(e.Flags&NF_FLAG_PATH)&&e.PathLength)return false;
  if((e.Flags&NF_FLAG_PATH_TRUNCATED)&&!(e.Flags&NF_FLAG_PATH))return false;
  if(e.Application[e.PathLength])return false;
  if((e.Flags&NF_FLAG_PORTS)&&(!(e.Flags&NF_FLAG_PROTOCOL)||(e.Protocol!=6&&e.Protocol!=17)))return false;
  if(!(e.Flags&NF_FLAG_PORTS)&&(e.LocalPort||e.RemotePort))return false;
  if(!(e.Flags&NF_FLAG_PROTOCOL)&&e.Protocol)return false;
  if(!(e.Flags&NF_FLAG_PID)&&e.ProcessId)return false;
  for(unsigned n=0;n<16;++n)if((!(e.Flags&NF_FLAG_LOCAL_ADDRESS)&&e.LocalAddress[n])||(!(e.Flags&NF_FLAG_REMOTE_ADDRESS)&&e.RemoteAddress[n]))return false;
  if(e.Family==4)for(unsigned n=4;n<16;++n)if(e.LocalAddress[n]||e.RemoteAddress[n])return false;
  previous=e.Sequence;
 }
 return true;
}
NF_RULE Rule(unsigned id,unsigned action,unsigned protocol,unsigned remotePort,const std::wstring& cidr,NF_U64 pid,unsigned direction){
 if(remotePort>65535||protocol>255||action>1||direction>2)throw std::invalid_argument("Invalid action, direction, protocol or port.");
 NF_RULE rule{};rule.Id=id;rule.Action=static_cast<NF_U8>(action);rule.Protocol=static_cast<NF_U8>(protocol);rule.Direction=static_cast<NF_U8>(direction);rule.ProcessId=pid;
 rule.RemoteFirst=rule.RemoteLast=static_cast<NF_U16>(remotePort);
 if(!cidr.empty()){
  const auto slash=cidr.find(L'/');const auto address=cidr.substr(0,slash);rule.Family=address.find(L':')==std::wstring::npos?4:6;
  unsigned prefix=rule.Family==4?32u:128u;
  if(slash!=std::wstring::npos){
   const auto text=cidr.substr(slash+1);prefix=0;if(text.empty()||text.size()>3)throw std::invalid_argument("Invalid CIDR prefix.");
   for(wchar_t digit:text){if(digit<L'0'||digit>L'9')throw std::invalid_argument("CIDR prefix must be a number.");prefix=prefix*10+static_cast<unsigned>(digit-L'0');}
  }
  if(prefix>(rule.Family==4?32u:128u)||InetPtonW(rule.Family==4?AF_INET:AF_INET6,address.c_str(),rule.RemoteAddress)!=1)throw std::invalid_argument("Enter a numeric IPv4 or IPv6 CIDR address.");
  rule.RemotePrefix=static_cast<NF_U8>(prefix);
  for(unsigned i=0;i<16;++i){unsigned start=i*8,mask=prefix>=start+8?255:prefix<=start?0:256-(1u<<(8-(prefix-start)));rule.RemoteAddress[i]&=static_cast<NF_U8>(mask);}
 }
 if(!Validate(rule))throw std::invalid_argument("Invalid rule. Ports require TCP or UDP, and IDs must be nonzero.");
 return rule;
}
std::wstring Address(const NF_EVENT& e,bool remote){
 if(e.Family!=4&&e.Family!=6)return L"unknown";
 const NF_U32 flag=remote?NF_FLAG_REMOTE_ADDRESS:NF_FLAG_LOCAL_ADDRESS;if(!(e.Flags&flag))return L"unavailable";
 wchar_t address[INET6_ADDRSTRLEN]{};const auto* bytes=remote?e.RemoteAddress:e.LocalAddress;
 if(!InetNtopW(e.Family==4?AF_INET:AF_INET6,const_cast<NF_U8*>(bytes),address,INET6_ADDRSTRLEN))return L"invalid";
 std::wstring result=address;
 if(e.Flags&NF_FLAG_PORTS)result=(e.Family==6?L"["+result+L"]":result)+L":"+std::to_wstring(remote?e.RemotePort:e.LocalPort);
 return result;
}
struct Session::State{
 HANDLE device=INVALID_HANDLE_VALUE,engine=nullptr;bool active=false;ULONGLONG nextProbe=0;
 void Probe(){
  if(!active||GetTickCount64()<nextProbe)return;
  FWPM_PROVIDER0* provider=nullptr;DWORD result=FwpmProviderGetByKey0(engine,&Provider,&provider);
  if(provider)FwpmFreeMemory0(reinterpret_cast<void**>(&provider));
  Check(result,"Capture WFP session was lost; stop and start capture again");
  nextProbe=GetTickCount64()+1000;
 }
};
Session::Session():state_(std::make_unique<State>()){}
Session::~Session(){Stop();}
void Session::Start(bool includePaths){
 if(state_->active)throw std::runtime_error("Capture is already active.");
 try{
  state_->device=Open();Control(state_->device,NF_IOCTL_START,nullptr,0,includePaths?NF_OPTION_INCLUDE_PATHS:0);
  FWPM_SESSION0 session{};session.flags=FWPM_SESSION_FLAG_DYNAMIC;session.txnWaitTimeoutInMSec=1000;
  Check(FwpmEngineOpen0(nullptr,RPC_C_AUTHN_WINNT,nullptr,&session,&state_->engine),"Open dynamic WFP session");
  Check(FwpmTransactionBegin0(state_->engine,0),"Begin WFP transaction");
  FWPM_PROVIDER0 provider{};provider.providerKey=Provider;provider.displayData.name=const_cast<wchar_t*>(L"Titan Network Filter");
  Check(FwpmProviderAdd0(state_->engine,&provider,nullptr),"Register own WFP provider");
  FWPM_SUBLAYER0 sublayer{};sublayer.subLayerKey=Sublayer;sublayer.providerKey=const_cast<GUID*>(&Provider);sublayer.displayData.name=const_cast<wchar_t*>(L"Titan connection policy and packet accounting");sublayer.weight=0x100;
  Check(FwpmSubLayerAdd0(state_->engine,&sublayer,nullptr),"Register own WFP sublayer");
  for(unsigned i=0;i<8;++i){
   FWPM_CALLOUT0 callout{};callout.calloutKey=Keys[i];callout.providerKey=const_cast<GUID*>(&Provider);callout.applicableLayer=*Layers[i];callout.displayData.name=const_cast<wchar_t*>(L"Titan metadata inspection");
   Check(FwpmCalloutAdd0(state_->engine,&callout,nullptr,nullptr),"Add inspection callout");
   FWPM_FILTER0 filter{};filter.displayData.name=const_cast<wchar_t*>(L"Titan connection stage observation");filter.providerKey=const_cast<GUID*>(&Provider);filter.layerKey=*Layers[i];filter.subLayerKey=Sublayer;filter.weight.type=FWP_EMPTY;filter.action.type=i<4?FWP_ACTION_CALLOUT_TERMINATING:FWP_ACTION_CALLOUT_INSPECTION;filter.action.calloutKey=Keys[i];
   Check(FwpmFilterAdd0(state_->engine,&filter,nullptr,nullptr),"Add dynamic inspection filter");
  }
  Check(FwpmTransactionCommit0(state_->engine),"Commit WFP transaction");
  state_->active=true;state_->nextProbe=0;const auto status=Status();if(!status.Active)throw std::runtime_error("Driver did not enable capture.");
 }catch(...){Stop();throw;}
}
void Session::SetRules(const std::vector<NF_RULE>& rules){
 if(!state_->active)throw std::runtime_error("Start the filter before setting rules.");
 if(rules.size()>NF_RULE_CAPACITY)throw std::invalid_argument("At most 64 rules per session.");
 NF_POLICY policy{};policy.Magic=NF_MAGIC;policy.Version=NF_VERSION;policy.Size=sizeof(policy);policy.Count=static_cast<NF_U32>(rules.size());
 std::copy(rules.begin(),rules.end(),policy.Rules);
 if(!Validate(policy))throw std::invalid_argument("Invalid policy or duplicate rule IDs.");
 state_->Probe();DWORD bytes=0;
 if(!DeviceIoControl(state_->device,NF_IOCTL_POLICY,&policy,sizeof(policy),nullptr,0,&bytes,nullptr))Fail(GetLastError(),"Replace network policy");
 if(bytes)Fail(ERROR_INVALID_DATA,"Policy response length");
}
void Session::Stop() noexcept{
 if(state_->engine){FwpmEngineClose0(state_->engine);state_->engine=nullptr;}
 if(state_->device!=INVALID_HANDLE_VALUE){NF_REQUEST input{NF_MAGIC,NF_VERSION,sizeof(NF_REQUEST),0};DWORD received=0;DeviceIoControl(state_->device,NF_IOCTL_STOP,&input,sizeof(input),nullptr,0,&received,nullptr);CloseHandle(state_->device);state_->device=INVALID_HANDLE_VALUE;}
 state_->active=false;
}
NF_STATUS Session::Status() const{
 if(state_->device==INVALID_HANDLE_VALUE)throw std::runtime_error("Capture has not been started.");
 state_->Probe();
 NF_STATUS result{};Control(state_->device,NF_IOCTL_STATUS,&result,sizeof(result));if(!Validate(result))Fail(ERROR_INVALID_DATA,"Validate driver status");return result;
}
std::vector<NF_EVENT> Session::Read(){
 if(!state_->active)throw std::runtime_error("Capture is inactive.");
 state_->Probe();
 NF_BATCH batch{};Control(state_->device,NF_IOCTL_READ,&batch,sizeof(batch));if(!Validate(batch,sizeof(batch)))Fail(ERROR_INVALID_DATA,"Validate event batch");return {batch.Events,batch.Events+batch.Count};
}
}
