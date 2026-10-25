#include "../SDK/NetworkFilter.h"
#include <iostream>
#include <stdexcept>
static void Check(bool result){if(!result)throw std::runtime_error("Offline rule/protocol regression");}
int main(){try{
    auto r=NetworkFilter::Rule(1,NF_ACTION_BLOCK,6,443,L"192.168.1.99/24",42);
    Check(NetworkFilter::Validate(r)&&r.RemoteAddress[3]==0&&r.RemotePrefix==24);
    for(unsigned prefix=0;prefix<=128;++prefix){auto v6=NetworkFilter::Rule(2,0,17,53,L"2001:db8::abcd/"+std::to_wstring(prefix));Check(NetworkFilter::Validate(v6));}
    for(const auto& address:{L"127.0.0.1/33",L"::1/129",L"::1/-1",L"example.com",L"::1/",L"1.2.3.4/1junk"}){
        bool refused=false;try{NetworkFilter::Rule(1,1,6,0,address);}catch(const std::invalid_argument&){refused=true;}Check(refused);
    }
    for(unsigned action=2;action<256;++action){auto bad=r;bad.Action=static_cast<NF_U8>(action);Check(!NetworkFilter::Validate(bad));}
    auto bad=r;bad.Reserved[0]=1;Check(!NetworkFilter::Validate(bad));bad=r;bad.Protocol=1;Check(!NetworkFilter::Validate(bad));
    bad=r;bad.RemoteFirst=500;bad.RemoteLast=100;Check(!NetworkFilter::Validate(bad));bad=r;bad.RemoteAddress[3]=1;Check(!NetworkFilter::Validate(bad));
    NF_POLICY policy{};policy.Magic=NF_MAGIC;policy.Version=NF_VERSION;policy.Size=sizeof(policy);policy.Count=2;policy.Rules[0]=r;policy.Rules[1]=r;Check(!NetworkFilter::Validate(policy));
    policy.Rules[1].Id=2;Check(NetworkFilter::Validate(policy));policy.Count=65;Check(!NetworkFilter::Validate(policy));
    NF_BATCH batch{};batch.Magic=NF_MAGIC;batch.Version=NF_VERSION;batch.Size=sizeof(batch);Check(NetworkFilter::Validate(batch,sizeof(batch)));
    batch.Count=1;auto& e=batch.Events[0];e.Sequence=1;e.TimestampQpc=1;e.Generation=1;e.Family=4;e.Direction=2;e.Stage=1;Check(NetworkFilter::Validate(batch,sizeof(batch)));
    e.Decision=1;Check(!NetworkFilter::Validate(batch,sizeof(batch)));e.RuleId=1;Check(NetworkFilter::Validate(batch,sizeof(batch)));
    e.Flags=2;Check(!NetworkFilter::Validate(batch,sizeof(batch)));e.Flags=0;e.Application[0]=1;Check(!NetworkFilter::Validate(batch,sizeof(batch)));
    std::cout << "CIDR, protocol, policy and malformed-wire checks passed. No network capture or filters created.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
