#include "../SDK/NetworkFilter.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <algorithm>
#include <fstream>
#include <functional>
namespace {
struct Socket {SOCKET value=INVALID_SOCKET;~Socket(){if(value!=INVALID_SOCKET)closesocket(value);}};
bool Roundtrip(bool ipv6,bool udp,unsigned short* selected=nullptr,const std::function<void(unsigned short)>& beforeConnect={}){
 int family=ipv6?AF_INET6:AF_INET;Socket server,client,accepted;
 server.value=socket(family,udp?SOCK_DGRAM:SOCK_STREAM,0);client.value=socket(family,udp?SOCK_DGRAM:SOCK_STREAM,0);
 if(server.value==INVALID_SOCKET||client.value==INVALID_SOCKET)throw std::runtime_error("Loopback socket allocation failed.");
 DWORD timeout=500;setsockopt(server.value,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
 sockaddr_storage storage{};int length=ipv6?sizeof(sockaddr_in6):sizeof(sockaddr_in);
 if(ipv6){auto* a=reinterpret_cast<sockaddr_in6*>(&storage);a->sin6_family=AF_INET6;a->sin6_addr=in6addr_loopback;DWORD only=1;setsockopt(server.value,IPPROTO_IPV6,IPV6_V6ONLY,reinterpret_cast<const char*>(&only),sizeof(only));}
 else{auto* a=reinterpret_cast<sockaddr_in*>(&storage);a->sin_family=AF_INET;a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);}
 if(bind(server.value,reinterpret_cast<sockaddr*>(&storage),length)||getsockname(server.value,reinterpret_cast<sockaddr*>(&storage),&length))throw std::runtime_error("Loopback bind failed.");
 auto port=ntohs(ipv6?reinterpret_cast<sockaddr_in6*>(&storage)->sin6_port:reinterpret_cast<sockaddr_in*>(&storage)->sin_port);
 if(selected)*selected=port;
 if(!udp&&listen(server.value,4))throw std::runtime_error("Loopback listen failed.");
 if(beforeConnect)beforeConnect(port);
 if(udp){if(sendto(client.value,"NF",2,0,reinterpret_cast<sockaddr*>(&storage),length)!=2)return false;char buffer[8]{};return recv(server.value,buffer,sizeof(buffer),0)==2&&buffer[0]=='N'&&buffer[1]=='F';}
 u_long on=1;ioctlsocket(client.value,FIONBIO,&on);
 int connected=connect(client.value,reinterpret_cast<sockaddr*>(&storage),length);
 if(connected&&WSAGetLastError()!=WSAEWOULDBLOCK)return false;
 fd_set writable,errors;FD_ZERO(&writable);FD_ZERO(&errors);FD_SET(client.value,&writable);FD_SET(client.value,&errors);timeval wait{0,500000};
 if(select(0,nullptr,&writable,&errors,&wait)<=0||FD_ISSET(client.value,&errors))return false;
 int error=0;int bytes=sizeof(error);if(getsockopt(client.value,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&bytes)||error)return false;
 fd_set readable;FD_ZERO(&readable);FD_SET(server.value,&readable);timeval acceptWait{0,500000};if(select(0,&readable,nullptr,nullptr,&acceptWait)<=0)return false;
 accepted.value=accept(server.value,nullptr,nullptr);if(accepted.value==INVALID_SOCKET)return false;
 if(send(client.value,"NF",2,0)!=2)return false;char buffer[8]{};setsockopt(accepted.value,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
 return recv(accepted.value,buffer,sizeof(buffer),0)==2&&buffer[0]=='N'&&buffer[1]=='F';
}
void Require(bool value,const char* detail){if(!value)throw std::runtime_error(detail);}
int SelfTest(){
 NetworkFilter::Session session;session.Start();unsigned cases=0;
 for(bool v6:{false,true})for(bool udp:{false,true}){
  auto cidr=v6?L"::1/128":L"127.0.0.0/8";auto protocol=udp?17u:6u;
  Require(Roundtrip(v6,udp),"Monitor-only loopback failed.");++cases;
  auto rule=NetworkFilter::Rule(100+cases,1,protocol,0,cidr,GetCurrentProcessId());
  auto before=session.Status();session.SetRules({rule});
  Require(!Roundtrip(v6,udp),"A matching outbound rule did not block loopback traffic.");
  Require(session.Status().Blocked>before.Blocked,"Missing kernel block evidence.");++cases;
  auto events=session.Read();Require(std::any_of(events.begin(),events.end(),[&](const auto& e){return e.Decision==1&&e.RuleId==rule.Id&&e.ProcessId==GetCurrentProcessId();}),"Missing attributed block event.");
  auto exception=rule;exception.Id=999;exception.Action=0;
  session.SetRules({exception,rule});Require(Roundtrip(v6,udp),"Ordered continue exception failed.");++cases;
  session.SetRules({});Require(Roundtrip(v6,udp),"Clearing policy did not restore traffic.");++cases;
  before=session.Status();
  Require(!Roundtrip(v6,udp,nullptr,[&](unsigned short port){auto inbound=NetworkFilter::Rule(200+cases,1,protocol,0,cidr,GetCurrentProcessId(),NF_INBOUND);inbound.LocalFirst=inbound.LocalLast=port;session.SetRules({inbound});}),"A matching inbound authorization was not blocked.");
  Require(session.Status().Blocked>before.Blocked,"Missing inbound kernel block evidence.");++cases;
  session.SetRules({});Require(Roundtrip(v6,udp),"Clearing inbound rule did not restore traffic.");++cases;
 }
 std::vector<double> timings;timings.reserve(200);
 for(unsigned i=0;i<200;++i){auto begin=std::chrono::steady_clock::now();Require(Roundtrip(false,false),"Loopback stress failed.");timings.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count());}
 std::sort(timings.begin(),timings.end());auto stats=session.Status();
 session.Stop();session.Start();Require(session.Status().RuleCount==0,"Policy survived Stop/Start.");session.Stop();
 std::cout<<"{\"success\":true,\"cases\":"<<cases<<",\"tcp_stress\":200,\"blocked\":"<<stats.Blocked<<",\"packets_in\":"<<stats.PacketsIn<<",\"packets_out\":"<<stats.PacketsOut<<",\"ring_dropped\":"<<stats.Dropped<<",\"loopback_setup_transfer_us_p50\":"<<timings[100]<<",\"loopback_setup_transfer_us_p95\":"<<timings[190]<<",\"scope\":\"Observed local socket setup and transfer; not driver-only overhead or wire latency\"}\n";
 return 0;
}
}
int wmain(int argc,wchar_t** argv){
 WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))return 1;
 struct Cleanup{~Cleanup(){WSACleanup();}} cleanup;
 try{
  if(argc==2&&std::wstring(argv[1])==L"--self-test")return SelfTest();
  if(argc==4&&std::wstring(argv[1])==L"--hold-block"){
   unsigned long port=std::stoul(argv[2]);if(!port||port>65535)throw std::invalid_argument("Invalid test port.");
   NetworkFilter::Session session;session.Start();session.SetRules({NetworkFilter::Rule(1,1,6,static_cast<unsigned>(port),L"127.0.0.0/8")});
   std::ofstream ready(argv[3]);ready<<"ready";ready.close();Sleep(30000);return 0;
  }
  if(argc!=3||std::wstring(argv[1])!=L"--monitor"){std::cout<<"NetworkFilterCLI --monitor SECONDS | --self-test\nMonitoring starts without blocking rules. Run as administrator.\n";return 0;}
  const unsigned seconds=static_cast<unsigned>(std::stoul(argv[2]));if(!seconds||seconds>3600)throw std::invalid_argument("Duration must be 1 to 3600 seconds.");
  NetworkFilter::Session session;session.Start();const auto until=GetTickCount64()+seconds*1000ull;
  std::wcout<<L"sequence,pid,direction,protocol,remote,decision,rule\n";
  while(GetTickCount64()<until){for(const auto& e:session.Read())std::wcout<<e.Sequence<<L','<<e.ProcessId<<L','<<unsigned(e.Direction)<<L','<<unsigned(e.Protocol)<<L','<<NetworkFilter::Address(e)<<L','<<(e.Decision?L"BLOCK":L"CONTINUE")<<L','<<e.RuleId<<L'\n';Sleep(20);}
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
