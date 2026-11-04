#ifndef NF_PRIVATE_RULES_H
#define NF_PRIVATE_RULES_H
/* Same bounded matcher is exercised by offline tests and the kernel. */
static __inline int NfValidRule(const NF_RULE* r){
    unsigned i,bytes;
    if(!r->Id || (r->Family!=0&&r->Family!=4&&r->Family!=6) || r->Direction>2 || r->Action>1)return 0;
    if(r->Reserved[0]||r->Reserved[1]||r->Reserved[2])return 0;
    if((r->LocalFirst==0)!=(r->LocalLast==0)||(r->RemoteFirst==0)!=(r->RemoteLast==0))return 0;
    if(r->LocalFirst>r->LocalLast||r->RemoteFirst>r->RemoteLast)return 0;
    if((r->LocalFirst||r->RemoteFirst)&&r->Protocol!=6&&r->Protocol!=17)return 0;
    if(r->RemotePrefix>(r->Family==4?32:r->Family==6?128:0))return 0;
    bytes=r->Family==4?4:r->Family==6?16:0;
    for(i=bytes;i<16;++i)if(r->RemoteAddress[i])return 0;
    /* Reject CIDRs with nonzero host bits, avoiding misleading UI policies. */
    for(i=0;i<bytes;++i){
        unsigned start=i*8,mask=r->RemotePrefix>=start+8?255:r->RemotePrefix<=start?0:256-(1u<<(8-(r->RemotePrefix-start)));
        if(r->RemoteAddress[i]&~mask)return 0;
    }
    return 1;
}
static __inline int NfMatch(const NF_RULE* r,const NF_EVENT* e){
    unsigned i;
    if(r->Family&&r->Family!=e->Family)return 0;
    if(r->Direction&&r->Direction!=e->Direction)return 0;
    if(r->ProcessId&&(!(e->Flags&NF_FLAG_PID)||r->ProcessId!=e->ProcessId))return 0;
    if(r->Protocol&&(!(e->Flags&NF_FLAG_PROTOCOL)||r->Protocol!=e->Protocol))return 0;
    if(r->LocalFirst&&(!(e->Flags&NF_FLAG_PORTS)||e->LocalPort<r->LocalFirst||e->LocalPort>r->LocalLast))return 0;
    if(r->RemoteFirst&&(!(e->Flags&NF_FLAG_PORTS)||e->RemotePort<r->RemoteFirst||e->RemotePort>r->RemoteLast))return 0;
    if(r->RemotePrefix){
        if(!(e->Flags&NF_FLAG_REMOTE_ADDRESS))return 0;
        for(i=0;i<16&&i*8<r->RemotePrefix;++i){
            unsigned bits=r->RemotePrefix-i*8,mask=bits>=8?255:256-(1u<<(8-bits));
            if((e->RemoteAddress[i]&mask)!=r->RemoteAddress[i])return 0;
        }
    }
    return 1;
}
#endif
