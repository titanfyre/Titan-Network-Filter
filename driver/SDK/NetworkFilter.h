#ifndef TITAN_NETWORK_FILTER_H
#define TITAN_NETWORK_FILTER_H
/* Fixed, pointer-free protocol. Defaults use the native MSVC x64 ABI. */
#define NF_VERSION 1u
#define NF_DEVICE_TYPE 0x8349u
#define NF_MAGIC 0x4E46544Eu
#define NF_RING_CAPACITY 2048u
#define NF_READ_CAPACITY 64u
#define NF_PATH_CAPACITY 128u
#define NF_RULE_CAPACITY 64u
#define NF_STAGE_AUTHORIZE 1u
#define NF_FLAG_PID 1u
#define NF_FLAG_PATH 4u
#define NF_FLAG_PATH_TRUNCATED 8u
#define NF_FLAG_REAUTHORIZE 16u
#define NF_FLAG_LOCAL_ADDRESS 32u
#define NF_FLAG_REMOTE_ADDRESS 64u
#define NF_FLAG_PORTS 128u
#define NF_FLAG_PROTOCOL 256u
#define NF_OPTION_INCLUDE_PATHS 1u
#define NF_INBOUND 1u
#define NF_OUTBOUND 2u
#define NF_ACTION_CONTINUE 0u
#define NF_ACTION_BLOCK 1u
#define NF_IOCTL(function) ((NF_DEVICE_TYPE<<16)|(3u<<14)|((0x800u+(function))<<2))
#define NF_IOCTL_START NF_IOCTL(1u)
#define NF_IOCTL_STOP NF_IOCTL(2u)
#define NF_IOCTL_STATUS NF_IOCTL(3u)
#define NF_IOCTL_READ NF_IOCTL(4u)
#define NF_IOCTL_POLICY NF_IOCTL(5u)
typedef unsigned char NF_U8;
typedef unsigned short NF_U16;
typedef unsigned int NF_U32;
typedef unsigned long long NF_U64;
typedef struct NF_REQUEST { NF_U32 Magic,Version,Size,Options; } NF_REQUEST;
typedef struct NF_RULE {
    NF_U64 ProcessId; /* 0 = any; current PID, not a permanent application ID. */
    NF_U32 Id;       /* Nonzero and unique within a policy. First match wins. */
    NF_U16 LocalFirst,LocalLast,RemoteFirst,RemoteLast; /* 0/0 = any. */
    NF_U8 Family,Direction,Protocol,Action; /* 0 = any, except Action. */
    NF_U8 RemotePrefix,Reserved[3];
    NF_U8 RemoteAddress[16]; /* Network-order CIDR bytes. */
} NF_RULE;
typedef struct NF_POLICY { NF_U32 Magic,Version,Size,Count; NF_RULE Rules[NF_RULE_CAPACITY]; } NF_POLICY;
typedef struct NF_EVENT {
    NF_U64 Sequence,TimestampQpc,ProcessId,Generation;
    NF_U32 Flags,RuleId;
    NF_U16 LocalPort,RemotePort;
    NF_U8 Family,Stage,Protocol,Direction;
    NF_U8 LocalAddress[16],RemoteAddress[16];
    NF_U16 PathLength,Decision;
    NF_U16 Application[NF_PATH_CAPACITY];
} NF_EVENT;
typedef struct NF_STATUS {
    NF_U32 Magic,Version,Size,Active;
    NF_U64 QpcFrequency,Produced,Dropped;
    NF_U32 Available,Capacity,Options,RuleCount;
    NF_U64 BytesIn,BytesOut,PacketsIn,PacketsOut,Blocked,Continued,Generation;
} NF_STATUS;
typedef struct NF_BATCH { NF_U32 Magic,Version,Size,Count; NF_U64 Dropped; NF_EVENT Events[NF_READ_CAPACITY]; } NF_BATCH;
#ifdef __cplusplus
#include <memory>
#include <string>
#include <vector>
namespace NetworkFilter {
static_assert(sizeof(NF_RULE)==48 && sizeof(NF_EVENT)==344 && sizeof(NF_STATUS)==112,"Unexpected ABI packing");
bool Validate(const NF_RULE&) noexcept;
bool Validate(const NF_POLICY&) noexcept;
bool Validate(const NF_STATUS&) noexcept;
bool Validate(const NF_BATCH&,std::size_t bytes) noexcept;
NF_RULE Rule(unsigned id,unsigned action,unsigned protocol=0,unsigned remotePort=0,
             const std::wstring& remoteCidr=L"",NF_U64 pid=0,unsigned direction=NF_OUTBOUND);
std::wstring Address(const NF_EVENT&,bool remote=true);
class Session {
    struct State;
    std::unique_ptr<State> state_;
public:
    Session();
    ~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    void Start(bool includeApplicationPaths=false);
    void SetRules(const std::vector<NF_RULE>&); // Atomic replacement, <=64 rules.
    std::vector<NF_EVENT> Read();               // Nonblocking, <=64 ALE events.
    NF_STATUS Status() const;
    void Stop() noexcept;                      // Also automatic on destruction.
};
}
#endif
#endif
