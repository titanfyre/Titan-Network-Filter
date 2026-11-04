#ifndef NF_PRIVATE_RING_H
#define NF_PRIVATE_RING_H
/* Nonallocating indexing shared by the private driver and its offline test. */
typedef struct NF_RING_INDEX { NF_U32 Head,Count; NF_U64 Produced,Dropped; } NF_RING_INDEX;
static __inline void NfRingReset(NF_RING_INDEX* r){r->Head=0;r->Count=0;r->Produced=0;r->Dropped=0;}
static __inline NF_U32 NfRingPush(NF_RING_INDEX* r){
 NF_U32 slot;
 ++r->Produced;
 if(r->Count==NF_RING_CAPACITY){r->Head=(r->Head+1)%NF_RING_CAPACITY;--r->Count;++r->Dropped;}
 slot=(r->Head+r->Count)%NF_RING_CAPACITY;++r->Count;return slot;
}
static __inline NF_U32 NfRingPop(NF_RING_INDEX* r){
 NF_U32 slot;
 if(!r->Count)return NF_RING_CAPACITY;
 slot=r->Head;r->Head=(r->Head+1)%NF_RING_CAPACITY;--r->Count;return slot;
}
#endif
