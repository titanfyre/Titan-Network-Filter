/* Original Titanfyre ALE policy and IP accounting callout. */
#include <ntddk.h>
#include <wdf.h>
#define NDIS630 1
#pragma warning(push)
#pragma warning(disable:4201)
#include <ndis.h>
#include <fwpsk.h>
#pragma warning(pop)
#include <initguid.h>
#include <fwpmk.h>
#include "../SDK/NetworkFilter.h"
#include "Ring.h"
#include "Rules.h"

C_ASSERT(sizeof(NF_EVENT)==344);
C_ASSERT(sizeof(NF_STATUS)==112);

DEFINE_GUID(GUID_NF_INTERFACE,0x46daaf11,0x2408,0x4882,0xaf,0x6b,0x53,0xd5,0x21,0x39,0xa6,0x14);
static const GUID Keys[8]={
 {0x6a780000,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780001,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780002,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780003,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780004,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780005,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780006,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}},
 {0x6a780007,0x1127,0x493d,{0xa3,0xc2,0x9f,0x10,0xd4,0x85,0xca,0x31}}
};
typedef struct NF_DEVICE {
    KSPIN_LOCK Lock;
    EX_RUNDOWN_REF Rundown;
    WDFFILEOBJECT Owner;
    NF_EVENT Ring[NF_RING_CAPACITY];
    NF_U64 Frequency,Generation;
    NF_U32 Options,RuleCount;
    NF_RULE Rules[NF_RULE_CAPACITY];
    NF_U64 BytesIn,BytesOut,PacketsIn,PacketsOut,Blocked,Continued;
    NF_RING_INDEX Index;
    
    UINT32 Callouts[8];
    BOOLEAN Registered[8],Active,Online;
} NF_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NF_DEVICE, Context)
typedef struct NF_FILE { BOOLEAN Closing; } NF_FILE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NF_FILE, FileContext)
static PVOID volatile Singleton;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD DeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP DeviceCleanup;
EVT_WDF_FILE_CLEANUP FileCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DeviceControl;

static VOID Continue(FWPS_CLASSIFY_OUT0* result){
    if(result->rights & FWPS_RIGHT_ACTION_WRITE)result->actionType=FWP_ACTION_CONTINUE;
}
static BOOLEAN U32(const FWPS_INCOMING_VALUES0* v,UINT32 index,UINT32* out){
    if(index>=v->valueCount||v->incomingValue[index].value.type!=FWP_UINT32)return FALSE;
    *out=v->incomingValue[index].value.uint32;return TRUE;
}
static BOOLEAN U16(const FWPS_INCOMING_VALUES0* v,UINT32 index,NF_U16* out){
    if(index>=v->valueCount||v->incomingValue[index].value.type!=FWP_UINT16)return FALSE;
    *out=v->incomingValue[index].value.uint16;return TRUE;
}
static VOID Address(const FWPS_INCOMING_VALUES0* v,UINT32 index,BOOLEAN v6,NF_U8* out,NF_U32 flag,NF_EVENT* e){
    const FWP_VALUE0* value;
    if(index>=v->valueCount)return;
    value=&v->incomingValue[index].value;
    if(v6){
        if(value->type!=FWP_BYTE_ARRAY16_TYPE||value->byteArray16==NULL)return;
        RtlCopyMemory(out,value->byteArray16->byteArray16,16);
    }else{
        UINT32 a;
        if(value->type!=FWP_UINT32)return;
        a=value->uint32;out[0]=(UCHAR)(a>>24);out[1]=(UCHAR)(a>>16);out[2]=(UCHAR)(a>>8);out[3]=(UCHAR)a;
    }
    e->Flags|=flag;
}
static VOID NTAPI Classify(const FWPS_INCOMING_VALUES0* fixed,const FWPS_INCOMING_METADATA_VALUES0* meta,
    VOID* layerData,const VOID* classifyContext,const FWPS_FILTER1* filter,UINT64 flowContext,FWPS_CLASSIFY_OUT0* result){
    WDFDEVICE device;
    NF_DEVICE* c;
    NF_EVENT event;
    UINT32 local=MAXULONG,remote=MAXULONG,lp=MAXULONG,rp=MAXULONG,protocol=MAXULONG,flags=MAXULONG;
    BOOLEAN v6=FALSE;
    KIRQL old;
    NF_U64 generation;

    NF_U32 options;
    UNREFERENCED_PARAMETER(layerData);UNREFERENCED_PARAMETER(classifyContext);
    UNREFERENCED_PARAMETER(filter);UNREFERENCED_PARAMETER(flowContext);
    Continue(result);
    device=(WDFDEVICE)InterlockedCompareExchangePointer(&Singleton,NULL,NULL);
    if(device==NULL)return;
    c=Context(device);
    if(!ExAcquireRundownProtection(&c->Rundown))return;
    KeAcquireSpinLock(&c->Lock,&old);
    if(!c->Active){KeReleaseSpinLock(&c->Lock,old);ExReleaseRundownProtection(&c->Rundown);return;}
    generation=c->Generation;
    options=c->Options;
    KeReleaseSpinLock(&c->Lock,old);
    if(fixed->layerId==FWPS_LAYER_INBOUND_IPPACKET_V4||fixed->layerId==FWPS_LAYER_INBOUND_IPPACKET_V6||
       fixed->layerId==FWPS_LAYER_OUTBOUND_IPPACKET_V4||fixed->layerId==FWPS_LAYER_OUTBOUND_IPPACKET_V6){
        BOOLEAN inbound=fixed->layerId==FWPS_LAYER_INBOUND_IPPACKET_V4||fixed->layerId==FWPS_LAYER_INBOUND_IPPACKET_V6;
        NF_U64 bytes=0,packets=0;
        if(layerData){NET_BUFFER* nb;
            for(nb=NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData);nb;nb=NET_BUFFER_NEXT_NB(nb)){
                bytes+=NET_BUFFER_DATA_LENGTH(nb);++packets;
                if(inbound&&FWPS_IS_METADATA_FIELD_PRESENT(meta,FWPS_METADATA_FIELD_IP_HEADER_SIZE))bytes+=meta->ipHeaderSize;
            }
        }
        KeAcquireSpinLock(&c->Lock,&old);
        if(c->Active&&c->Generation==generation){
            if(inbound){c->BytesIn+=bytes;c->PacketsIn+=packets;}else{c->BytesOut+=bytes;c->PacketsOut+=packets;}
        }
        KeReleaseSpinLock(&c->Lock,old);ExReleaseRundownProtection(&c->Rundown);return;
    }
    if(!(result->rights&FWPS_RIGHT_ACTION_WRITE)){ExReleaseRundownProtection(&c->Rundown);return;}
    RtlZeroMemory(&event,sizeof(event));
    event.TimestampQpc=(NF_U64)KeQueryPerformanceCounter(NULL).QuadPart;
    switch(fixed->layerId){
#define NF_FIELDS(prefix) local=prefix##_IP_LOCAL_ADDRESS;remote=prefix##_IP_REMOTE_ADDRESS;lp=prefix##_IP_LOCAL_PORT;rp=prefix##_IP_REMOTE_PORT;protocol=prefix##_IP_PROTOCOL
    case FWPS_LAYER_ALE_AUTH_CONNECT_V4:event.Stage=NF_STAGE_AUTHORIZE;event.Direction=NF_OUTBOUND;NF_FIELDS(FWPS_FIELD_ALE_AUTH_CONNECT_V4);flags=FWPS_FIELD_ALE_AUTH_CONNECT_V4_FLAGS;break;
    case FWPS_LAYER_ALE_AUTH_CONNECT_V6:event.Stage=NF_STAGE_AUTHORIZE;event.Direction=NF_OUTBOUND;v6=TRUE;NF_FIELDS(FWPS_FIELD_ALE_AUTH_CONNECT_V6);flags=FWPS_FIELD_ALE_AUTH_CONNECT_V6_FLAGS;break;
    case FWPS_LAYER_ALE_AUTH_RECV_ACCEPT_V4:event.Stage=NF_STAGE_AUTHORIZE;event.Direction=NF_INBOUND;NF_FIELDS(FWPS_FIELD_ALE_AUTH_RECV_ACCEPT_V4);flags=FWPS_FIELD_ALE_AUTH_RECV_ACCEPT_V4_FLAGS;break;
    case FWPS_LAYER_ALE_AUTH_RECV_ACCEPT_V6:event.Stage=NF_STAGE_AUTHORIZE;event.Direction=NF_INBOUND;v6=TRUE;NF_FIELDS(FWPS_FIELD_ALE_AUTH_RECV_ACCEPT_V6);flags=FWPS_FIELD_ALE_AUTH_RECV_ACCEPT_V6_FLAGS;break;
    default:ExReleaseRundownProtection(&c->Rundown);return;
#undef NF_FIELDS
    }
    event.Family=v6?6:4;
    if(FWPS_IS_METADATA_FIELD_PRESENT(meta,FWPS_METADATA_FIELD_PROCESS_ID)){event.ProcessId=meta->processId;event.Flags|=NF_FLAG_PID;}
    event.Generation=generation;
    if((options&NF_OPTION_INCLUDE_PATHS)&&FWPS_IS_METADATA_FIELD_PRESENT(meta,FWPS_METADATA_FIELD_PROCESS_PATH)&&meta->processPath&&meta->processPath->data){
        UINT32 length=meta->processPath->size/sizeof(NF_U16);
        if(length>NF_PATH_CAPACITY-1){length=NF_PATH_CAPACITY-1;event.Flags|=NF_FLAG_PATH_TRUNCATED;}
        RtlCopyMemory(event.Application,meta->processPath->data,length*sizeof(NF_U16));
        while(length&&event.Application[length-1]==0)--length;
        event.PathLength=(NF_U16)length;event.Flags|=NF_FLAG_PATH;
    }
    Address(fixed,local,v6,event.LocalAddress,NF_FLAG_LOCAL_ADDRESS,&event);
    Address(fixed,remote,v6,event.RemoteAddress,NF_FLAG_REMOTE_ADDRESS,&event);
    if(protocol<fixed->valueCount&&fixed->incomingValue[protocol].value.type==FWP_UINT8){event.Protocol=fixed->incomingValue[protocol].value.uint8;event.Flags|=NF_FLAG_PROTOCOL;}
    /* At ALE layers ICMP type/code alias these fields. Export real transport
       ports only when a valid TCP/UDP protocol and both ports are available. */
    if((event.Flags&NF_FLAG_PROTOCOL)&&(event.Protocol==6||event.Protocol==17)){
        NF_U16 localPort,remotePort;
        if(U16(fixed,lp,&localPort)&&U16(fixed,rp,&remotePort)){
            event.LocalPort=localPort;event.RemotePort=remotePort;event.Flags|=NF_FLAG_PORTS;
        }
    }
    {UINT32 value;if(U32(fixed,flags,&value)&&(value&FWP_CONDITION_FLAG_IS_REAUTHORIZE))event.Flags|=NF_FLAG_REAUTHORIZE;}
    KeAcquireSpinLock(&c->Lock,&old);
    if(c->Active&&c->Generation==generation){
        ULONG i,slot;
        for(i=0;i<c->RuleCount;++i)if(NfMatch(&c->Rules[i],&event)){
            event.RuleId=c->Rules[i].Id;event.Decision=c->Rules[i].Action;break;
        }
        if(event.Decision==NF_ACTION_BLOCK){result->actionType=FWP_ACTION_BLOCK;result->rights&=~FWPS_RIGHT_ACTION_WRITE;++c->Blocked;}
        else ++c->Continued;
        slot=NfRingPush(&c->Index);
        event.Sequence=c->Index.Produced;
        c->Ring[slot]=event;
    }
    KeReleaseSpinLock(&c->Lock,old);
    ExReleaseRundownProtection(&c->Rundown);
}
static NTSTATUS NTAPI Notify(FWPS_CALLOUT_NOTIFY_TYPE type,const GUID* key,const FWPS_FILTER1* filter){
    UNREFERENCED_PARAMETER(type);UNREFERENCED_PARAMETER(key);UNREFERENCED_PARAMETER(filter);return STATUS_SUCCESS;
}
VOID FileCleanup(WDFFILEOBJECT file){
    NF_DEVICE* c=Context(WdfFileObjectGetDevice(file));KIRQL old;
    KeAcquireSpinLock(&c->Lock,&old);FileContext(file)->Closing=TRUE;
    if(c->Owner==file){c->Active=FALSE;c->Owner=NULL;c->Options=0;c->RuleCount=0;c->Index.Head=c->Index.Count=0;}
    KeReleaseSpinLock(&c->Lock,old);
}
VOID DeviceControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code){
    NF_DEVICE* c=Context(WdfIoQueueGetDevice(queue));
    NF_REQUEST* header=NULL;
    NF_STATUS* status=NULL;
    NF_BATCH* batch=NULL;
    NTSTATUS result=STATUS_INVALID_DEVICE_REQUEST;
    size_t bytes=0;
    WDFFILEOBJECT owner=WdfRequestGetFileObject(request);
    KIRQL old;
    if(code==NF_IOCTL_POLICY){
        NF_POLICY* policy=NULL;ULONG i,j;
        if(input!=sizeof(NF_POLICY)||output!=0){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
        result=WdfRequestRetrieveInputBuffer(request,sizeof(NF_POLICY),(PVOID*)&policy,NULL);
        if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
        if(policy->Magic!=NF_MAGIC||policy->Version!=NF_VERSION||policy->Size!=sizeof(*policy)||policy->Count>NF_RULE_CAPACITY){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
        for(i=0;i<policy->Count;++i){
            if(!NfValidRule(&policy->Rules[i])){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
            for(j=0;j<i;++j)if(policy->Rules[i].Id==policy->Rules[j].Id){WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;}
        }
        KeAcquireSpinLock(&c->Lock,&old);
        if(!c->Active||c->Owner!=owner||!owner||FileContext(owner)->Closing)result=STATUS_ACCESS_DENIED;
        else{
            RtlCopyMemory(c->Rules,policy->Rules,policy->Count*sizeof(NF_RULE));c->RuleCount=policy->Count;++c->Generation;result=STATUS_SUCCESS;
        }
        KeReleaseSpinLock(&c->Lock,old);WdfRequestComplete(request,result);return;
    }
    if(input!=sizeof(NF_REQUEST)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
    result=WdfRequestRetrieveInputBuffer(request,sizeof(NF_REQUEST),(PVOID*)&header,NULL);
    if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
    if(header->Magic!=NF_MAGIC||header->Version!=NF_VERSION||header->Size!=sizeof(NF_REQUEST)||
       (code==NF_IOCTL_START?(header->Options&~NF_OPTION_INCLUDE_PATHS):header->Options)){
        WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;
    }
    if(code==NF_IOCTL_STATUS){
        if(output!=sizeof(NF_STATUS)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
        result=WdfRequestRetrieveOutputBuffer(request,sizeof(NF_STATUS),(PVOID*)&status,NULL);
        if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
        RtlZeroMemory(status,sizeof(*status));
    }else if(code==NF_IOCTL_READ){
        if(output!=sizeof(NF_BATCH)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
        result=WdfRequestRetrieveOutputBuffer(request,sizeof(NF_BATCH),(PVOID*)&batch,NULL);
        if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
        RtlZeroMemory(batch,sizeof(*batch));
    }else if(output!=0){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
    KeAcquireSpinLock(&c->Lock,&old);
    result=STATUS_SUCCESS;
    if(!c->Online)result=STATUS_DEVICE_NOT_READY;
    else if(owner==NULL||FileContext(owner)->Closing)result=STATUS_FILE_CLOSED;
    else if(code==NF_IOCTL_START){
        if(c->Active||(c->Owner&&c->Owner!=owner))result=STATUS_DEVICE_BUSY;
        else{c->Owner=owner;c->Active=TRUE;++c->Generation;c->Options=header->Options;
            c->RuleCount=0;c->BytesIn=c->BytesOut=c->PacketsIn=c->PacketsOut=c->Blocked=c->Continued=0;
            NfRingReset(&c->Index);
        }
    }else if(code==NF_IOCTL_STOP){
        if(c->Owner&&c->Owner!=owner)result=STATUS_ACCESS_DENIED;
        else{c->Active=FALSE;c->Owner=NULL;c->Options=0;c->RuleCount=0;c->Index.Head=c->Index.Count=0;}
    }else if(code==NF_IOCTL_STATUS&&status!=NULL){
        status->Magic=NF_MAGIC;status->Version=NF_VERSION;status->Size=sizeof(*status);status->Active=c->Active?1:0;
        status->QpcFrequency=c->Frequency;status->Produced=c->Index.Produced;status->Dropped=c->Index.Dropped;
        status->Available=c->Index.Count;status->Capacity=NF_RING_CAPACITY;status->Options=c->Options;status->RuleCount=c->RuleCount;
        status->BytesIn=c->BytesIn;status->BytesOut=c->BytesOut;status->PacketsIn=c->PacketsIn;status->PacketsOut=c->PacketsOut;
        status->Blocked=c->Blocked;status->Continued=c->Continued;status->Generation=c->Generation;bytes=sizeof(*status);
    }else if(code==NF_IOCTL_READ&&batch!=NULL){
        if(!c->Active||c->Owner!=owner)result=STATUS_ACCESS_DENIED;
        else{ULONG i;batch->Magic=NF_MAGIC;batch->Version=NF_VERSION;batch->Size=sizeof(*batch);batch->Dropped=c->Index.Dropped;
            batch->Count=min(c->Index.Count,NF_READ_CAPACITY);
            for(i=0;i<batch->Count;++i){batch->Events[i]=c->Ring[NfRingPop(&c->Index)];}
            bytes=sizeof(*batch);
        }
    }else result=STATUS_INVALID_DEVICE_REQUEST;
    KeReleaseSpinLock(&c->Lock,old);
    WdfRequestCompleteWithInformation(request,result,NT_SUCCESS(result)?bytes:0);
}
VOID DeviceCleanup(WDFOBJECT object){
    NF_DEVICE* c=Context(object);
    ULONG i;
    KIRQL old;
    KeAcquireSpinLock(&c->Lock,&old);c->Active=FALSE;c->Online=FALSE;KeReleaseSpinLock(&c->Lock,old);
    /* No flow contexts or asynchronous operations are ever created. The only
       registration/unregistration thread is the PnP lifetime thread. */
    for(i=0;i<8;++i)if(c->Registered[i]){
        NTSTATUS status;
        LARGE_INTEGER retry;
        retry.QuadPart=-100000; /* PASSIVE_LEVEL exceptional-path retry: 10 ms. */
        /* Never free a device while WFP still owns callback entry points. There
           are no flow contexts, and registration is not concurrent with cleanup;
           BUSY/IN_USE are therefore not normal outcomes. Unexpected subsystem
           failure retains the device rather than allowing a use-after-unload. */
        do{
            status=FwpsCalloutUnregisterById0(c->Callouts[i]);
            if(!NT_SUCCESS(status)&&status!=STATUS_FWP_CALLOUT_NOT_FOUND){
                (VOID)KeDelayExecutionThread(KernelMode,FALSE,&retry);
            }
        }while(!NT_SUCCESS(status)&&status!=STATUS_FWP_CALLOUT_NOT_FOUND);
        c->Registered[i]=FALSE;
    }
    ExWaitForRundownProtectionRelease(&c->Rundown);
    InterlockedCompareExchangePointer(&Singleton,NULL,(PVOID)object);
}
NTSTATUS DeviceAdd(WDFDRIVER driver,PWDFDEVICE_INIT init){
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_FILEOBJECT_CONFIG files;
    WDF_IO_QUEUE_CONFIG queue;
    WDFQUEUE q;
    NF_DEVICE* c;
    ULONG i;
    LARGE_INTEGER frequency;
    NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    UNREFERENCED_PARAMETER(driver);
    WdfDeviceInitSetDeviceType(init,NF_DEVICE_TYPE);
    WdfDeviceInitSetExclusive(init,TRUE);
    WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN|FILE_AUTOGENERATED_DEVICE_NAME,FALSE);
    status=WdfDeviceInitAssignSDDLString(init,&sddl);if(!NT_SUCCESS(status))return status;
    WDF_FILEOBJECT_CONFIG_INIT(&files,WDF_NO_EVENT_CALLBACK,WDF_NO_EVENT_CALLBACK,FileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,NF_FILE);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;
    WdfDeviceInitSetFileObjectConfig(init,&files,&attributes);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,NF_DEVICE);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;attributes.EvtCleanupCallback=DeviceCleanup;
    status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
    c=Context(device);KeInitializeSpinLock(&c->Lock);ExInitializeRundownProtection(&c->Rundown);
    (VOID)KeQueryPerformanceCounter(&frequency);c->Frequency=(NF_U64)frequency.QuadPart;
    if(InterlockedCompareExchangePointer(&Singleton,device,NULL)!=NULL)return STATUS_OBJECT_NAME_COLLISION;
    status=WdfDeviceCreateDeviceInterface(device,&GUID_NF_INTERFACE,NULL);if(!NT_SUCCESS(status))return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);queue.EvtIoDeviceControl=DeviceControl;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;
    status=WdfIoQueueCreate(device,&queue,&attributes,&q);if(!NT_SUCCESS(status))return status;
    for(i=0;i<8;++i){
        FWPS_CALLOUT1 callout;RtlZeroMemory(&callout,sizeof(callout));callout.calloutKey=Keys[i];callout.classifyFn=Classify;callout.notifyFn=Notify;
        status=FwpsCalloutRegister1(WdfDeviceWdmGetDeviceObject(device),&callout,&c->Callouts[i]);if(!NT_SUCCESS(status))return status;
        c->Registered[i]=TRUE;
    }
    c->Online=TRUE;return STATUS_SUCCESS;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry){
    WDF_DRIVER_CONFIG config;WDF_OBJECT_ATTRIBUTES attributes;
    WDF_DRIVER_CONFIG_INIT(&config,DeviceAdd);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelPassive;
    return WdfDriverCreate(object,registry,&attributes,&config,WDF_NO_HANDLE);
}
