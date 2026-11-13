#include "SDK/NetworkFilter.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<NetworkFilter::Session> session;
NetworkFilter::Session& Get(){if(!session)session=std::make_unique<NetworkFilter::Session>();return *session;}
std::vector<NF_RULE> Rules(const Json& a){auto array=a.at("rules");Require(array.is_array() && array.size()<=NF_RULE_CAPACITY,"Too many rules");std::vector<NF_RULE> rules;for(const auto& r:array){auto rule=FromWire<NF_RULE>(r);Require(NetworkFilter::Validate(rule),"Invalid network filter rule");rules.push_back(rule);}return rules;}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto rule=FromWire<NF_RULE>(a.at("rule"));return {{"valid",NetworkFilter::Validate(rule)},{"value",EncodeWire(rule)}};}
if(op=="make_rule")return EncodeWire(NetworkFilter::Rule(Number(a,"id"),Number(a,"action",0,1),Number(a,"protocol",0,255),Number(a,"remote_port",0,65535),Wide(a.value("remote_cidr",std::string())),a.contains("pid")?Identifier(a,"pid"):0,Number(a,"direction",NF_OUTBOUND,3)));
if(op=="start"){Get().Start(a.value("include_paths",false));return Json::object();}
if(op=="set_rules"){Get().SetRules(Rules(a));return Json::object();}
if(op=="stop"){if(session)session->Stop();return Json::object();}
if(op=="status")return EncodeWire(Get().Status());
if(op=="read")return VectorWire(Get().Read());
if(op=="event_address"){auto event=FromWire<NF_EVENT>(a.at("event"));return {{"address",Utf8(NetworkFilter::Address(event,a.value("remote",true)))}};}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned titan_network_filter_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned titan_network_filter_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* titan_network_filter_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void titan_network_filter_json_free(char* response)noexcept{delete[] response;}
