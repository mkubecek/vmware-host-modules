/*********************************************************
 * Copyright (c) 2006-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * cpuidInfo.h -
 *
 *      Routines for handling CPUID information in linked lists & arrays.
 */

#ifndef _CPUIDINFO_H
#define _CPUIDINFO_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_basic_defs.h"
#include "vm_basic_types.h"
#include "vm_basic_asm.h"
#include "x86cpuid.h"
#ifdef VM_X86_ANY
#include "x86cpuid_asm.h"
#endif
#include "msrCache.h"
#include "community_source.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CPUID_VENDOR_STR_LEN 13 // Vendor string is 12 characters + NULL
#define CPUID_INFO_OVFL_REALLOC_INC 10
#define CPUID_INFO_HEAP_MAX_SIZE (10 * MB)

typedef enum CpuidLeafStorage {
#define CPUIDLEVEL(t, s, v, c, h) CPUID_LEAF_STORAGE_##s = (c == 0 ? 1 : c),
   CPUID_KNOWN_LEVELS
#undef CPUIDLEVEL
} CpuidLeafStorage;

typedef enum CpuidIndex {
   CPUID_KNOWN_COUNT = 0
#define CPUIDLEVEL(t, s, v, c, h) + CPUID_LEAF_STORAGE_##s
   CPUID_KNOWN_LEVELS
#undef CPUIDLEVEL
} CpuidIndex;

typedef enum CpuidMaxChildren {
#define CPUIDLEVEL(t, s, v, c, h) CPUID_MAX_CHILD_##s = c,
   CPUID_KNOWN_LEVELS
#undef CPUIDLEVEL
} CpuidMaxChildren;

typedef struct CpuidInfoData {
#define CPUIDLEVEL(t, s, v, c, h) \
        CPUIDRegsUnion leaf_##s[CPUID_LEAF_STORAGE_##s];
   CPUID_KNOWN_LEVELS
#undef CPUIDLEVEL
} CpuidInfoData;

#define CPUID_DATA_INDEX(l) \
   (offsetof(CpuidInfoData, leaf_##l) / sizeof(CPUIDRegsUnion))

typedef struct CpuidNode {
   uint32 eaxIn, ecxIn;     // Two input parameters.
   CPUIDRegsUnion data;     // Resulting registers.
} CpuidNode;

typedef struct CpuidInfoOvfl {
   unsigned ct, allocCt;
   CpuidNode node[0]; // Extensible.
} CpuidInfoOvfl;

typedef struct CpuidInfo {
   CpuidVendor vendor;
   char vendorStr[CPUID_VENDOR_STR_LEN];
   CpuidNode node[CPUID_KNOWN_COUNT];
   CpuidInfoOvfl *ovfl;
} CpuidInfo;

typedef struct CpuidInfoReserve {
   union {
      uint8 dummy[sizeof(CpuidInfo) + sizeof(CpuidInfoOvfl) +
                  100 * sizeof(CpuidNode)];
      uint64 dummy2; /* 8 byte alignment for Fusion. */
   } u;
} CpuidInfoReserve;

typedef struct CpuidInfoIter {
   unsigned nextFixed, nextOvfl;
   unsigned nextOvflEax, nextOvflEcx;
   Bool moreOvfl;
} CpuidInfoIter;

#define CPUID_INFO_OVFL_ADJACENT ((CpuidInfoOvfl *)-1ull)

static INLINE CpuidInfoOvfl *
CpuidInfoOvflPtr(const CpuidInfo *info)
{
   ASSERT_ON_COMPILE(sizeof *info % 8 == 0); /* 8 byte alignment for Fusion. */
   return info->ovfl != CPUID_INFO_OVFL_ADJACENT ? info->ovfl :
                              (CpuidInfoOvfl *)((uintptr_t)info + sizeof *info);
}

typedef Bool (*CPUIDQueryFunction)(CPUIDQuery *query);
typedef Bool (*VHVSupportedFn)(const CpuidInfo *cpuid,
                               const MSRCache *msrCache);

uint32      CpuidInfo_FindLevel(CpuidLevel index, uint32 *subLeaves);
void        CpuidInfo_UpdateVendor(CpuidInfo *info);
CpuidVendor CpuidInfo_ComputeVendor(CPUIDRegs id0);
void        CpuidInfo_AdjustForNoMigrateMask(CpuidInfo *info);
void        CpuidInfo_RegsVendorStr(const CPUIDRegs *regs,
                                    char (*vendorStr)[CPUID_VENDOR_STR_LEN]);
CpuidNode  *CpuidInfoNodePtr(const CpuidInfo *info, uint32 eaxIn, uint32 ecxIn);

#if !defined(VMM) && !defined(GLM) && !defined(VMM_BOOTSTRAP) && !defined(COREQUERY)
Bool        CpuidInfo_IsEmpty(const CpuidInfo *info);
void        CpuidInfo_CopyToVMM(CpuidInfoReserve *dstAlloc,
                                const CpuidInfo *src);
CpuidInfo  *CpuidInfo_Clone(const CpuidInfo *src);
CpuidInfo  *CpuidInfo_FindCommonFeatures(CpuidInfo cpuids[], unsigned numCPUs);
Bool        CpuidInfo_PopulateOneCPU(CpuidInfo *cpuid,
                                     CPUIDQueryFunction queryFunc);
void        CpuidInfo_ExtractName(const CpuidInfo *cpuid, char *s, unsigned n);
CpuidInfo  *CpuidInfo_New(void);
CpuidNode  *CpuidInfo_Update(CpuidInfo *cpuid, uint32 eaxIn, uint32 ecxIn,
                             CPUIDRegs regs);
Bool        CpuidInfo_Next(const CpuidInfo *cpuid, CpuidInfoIter *iter,
                           CpuidNode **node);
Bool        CpuidInfo_VHVSupported(const CpuidInfo *cpuid,
                                   const MSRCache *msrCache);


#ifndef VMKERNEL
void        CpuidInfo_RemoveLevel(CpuidInfo *cpuid, uint32 eaxIn, uint32 ecxIn);
void        CpuidInfo_RemoveLevels(CpuidInfo *info,
                                   uint32 eaxInStart, uint32 eaxInEnd);
void        CpuidInfo_Free(CpuidInfo *info);
void        CpuidInfo_FreeArray(CpuidInfo cpuids[], unsigned numCpus);
CpuidInfo  *CpuidInfo_Populate(unsigned numCPUs, CPUIDQueryFunction queryFunc);
Bool        CpuidInfo_SimpleCPUIDQueryFunction(CPUIDQuery *query);
void        CpuidInfo_ClearXsaveSubleaf(CpuidInfo *cpuid, uint32 featureIndex);
CpuidInfo  *CpuidInfo_Synthesize(const CpuidInfo *base);
void        CpuidInfo_Normalize(CpuidInfo *cpuid, const MSRCache *msrCache,
                                Bool vhvSupported);
void        CpuidInfo_AdjustXSAVE(CpuidInfo *cpuid);
void        CpuidInfo_Query(const CpuidInfo *cpuid, CPUIDQuery *query);
#endif /* !VMKERNEL */

#ifdef VMKERNEL
void        CpuidInfo_Init(void);
void        CpuidInfo_GetHeapVPNBounds(VPN *firstVPN, PageCnt *nPages);
#endif


void CpuidInfo_IterInit(CpuidInfoIter *iter, const CpuidInfo *info);

/* Iterate through a CpuidInfo. */
#define FOR_EACH_CPUID_NODE(_info, _node)                       \
   do {                                                         \
      CpuidNode *_node;                                         \
      CpuidInfoIter _iter;                                      \
      CpuidInfo_IterInit(&_iter, _info);                        \
      while (CpuidInfo_Next(_info, &_iter, &_node)) {

#define ROF_EACH_CPUID_NODE                                     \
      }                                                         \
   } while (0)

/* Iterate through all subleaves of _eaxIn. */
#define FOR_EACH_CPUID_SUBLEAF_NODE(_info, _eaxIn, _node)          \
   do {                                                            \
      CpuidNode *_node;                                            \
      CpuidInfoIter _iter;                                         \
      CpuidInfo_IterInit(&_iter, _info);                           \
      _iter.nextFixed = CPUID_INFO_INDEX_##_eaxIn;                 \
      while (CpuidInfo_Next(_info, &_iter, &_node) &&              \
             _node->eaxIn <= _eaxIn) {                             \
         if (_node->eaxIn < _eaxIn) continue; /* Skip any ovfl. */

#define ROF_EACH_CPUID_SUBLEAF_NODE                                \
      }                                                            \
   } while (0)

/* Iterate through a CpuidInfo pair in lockstep. */
#define FOR_EACH_CPUID_NODE_PAIR(_info0, _node0, _info1, _node1)   \
   do {                                                            \
      CpuidNode *_node0, *_node1;                                  \
      CpuidInfoIter _iter0, _iter1;                                \
      CpuidInfo_IterInit(&_iter0, _info0);                         \
      CpuidInfo_IterInit(&_iter1, _info1);                         \
      while (CpuidInfo_Next(_info0, &_iter0, &_node0) |            \
             CpuidInfo_Next(_info1, &_iter1, &_node1)) {

#define ROF_EACH_CPUID_NODE_PAIR                                   \
      }                                                            \
   } while(0)
#endif /* !defined(VMM) && !defined(VMM_BOOTSTRAP) && !defined(COREQUERY) */

static INLINE unsigned
CpuidInfo_SizeOf(const CpuidInfo *info)
{
   CpuidInfoOvfl *ovfl = CpuidInfoOvflPtr(info);
   return sizeof *info +
          (ovfl == NULL ? 0 : sizeof *ovfl + ovfl->allocCt * sizeof(CpuidNode));
}

static INLINE Bool
CpuidInfo_IsVendorField(uint32 eaxIn, uint32 ecxIn, CpuidReg reg)
{
   return eaxIn == 0 && ecxIn == 0 && reg != CPUID_REG_EAX;
}

static INLINE CpuidReg
CpuidInfoEaxChk(CpuidReg reg, UNUSED_PARAM(uint32 eaxIn),
                UNUSED_PARAM(const CpuidNode *n))
{
   ASSERT(eaxIn == n->eaxIn);
   return reg;
}

static INLINE unsigned
CpuidInfoIndexChk(unsigned index, UNUSED_PARAM(uint32 ecxIn),
                  UNUSED_PARAM(uint32 ecxCnt))
{
   ASSERT(index < CPUID_KNOWN_COUNT && ecxIn < ecxCnt);
   return index;
}

#define CPUIDLEVEL(t, s, v, c, h)                   \
   CPUID_INFO_INDEX_##s = CPUID_DATA_INDEX(s),      \
   CPUID_INFO_INDEX_##v = CPUID_INFO_INDEX_##s,     \
   CPUID_LEAF_STORAGE_##v = CPUID_LEAF_STORAGE_##s,

enum {
   CPUID_KNOWN_LEVELS
};
#undef CPUIDLEVEL

#define FIELD(lvl, ecxIn, reg, bitpos, size, name, s, hwv)                     \
   CPUID_INFO_INDEX_##name = +CPUID_INFO_INDEX_##lvl +                         \
                             CPUID_INTERNAL_ECXIN_##name,                      \
   CPUID_INFO_ECX_CNT_##name = CPUID_LEAF_STORAGE_##lvl,

#define FLAG FIELD

enum {
   CPUID_FIELD_DATA
};
#undef FIELD
#undef FLAG


#define CPUID_INFO_INDEX(_n)                                                   \
   CpuidInfoIndexChk(CPUID_INFO_INDEX_##_n, CPUID_INTERNAL_ECXIN_##_n,         \
                     CPUID_INFO_ECX_CNT_##_n)

#define CPUID_INFO_FLD_REG(_n, _info)                                          \
  (_info)->node[CPUID_INFO_INDEX(_n)].data.array[CPUID_FEAT_REG(_n)]

#define CPUID_INFO_SET_EXISTS(_n, _info)                                       \
   do {                                                                        \
      ASSERT(!CpuidInfo_IsVendorField(                                         \
                (uint32)CPUID_INTERNAL_EAXIN_##_n,                             \
                (uint32)CPUID_INTERNAL_ECXIN_##_n, CPUID_FEAT_REG(_n)));       \
      (_info)->node[CPUID_INFO_INDEX(_n)].eaxIn =                              \
         (uint32)CPUID_INTERNAL_EAXIN_##_n;                                    \
      (_info)->node[CPUID_INFO_INDEX(_n)].ecxIn =                              \
         (uint32)CPUID_INTERNAL_ECXIN_##_n;                                    \
   } while (0)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_IsSet --
 *
 *      Returns TRUE if info has the CPUID flag set.  Returns FALSE if
 *      the flag is 0 or the level does not exist.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_IsSet(flag, info)                                     \
   ((CPUID_INFO_FLD_REG(flag, info) & CPUID_##flag##_MASK) != 0)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Get --
 *
 *      Returns the value of the field in info, or 0 if the level does
 *      not exist.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_Get(fld, info)                                        \
   ((CPUID_INFO_FLD_REG(fld, info) & CPUID_##fld##_MASK) >> CPUID_##fld##_SHIFT)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_SubleafGet --
 *
 *      Returns the value of the field in a CpuidNode subleaf node.
 *      Unlike CpuidInfo_Get, this overrides the ecxIn value of "field",
 *      which is useful for leaves whose subleaves have a regular format
 *      such as the cache leaves: 4 and 0xb.  The eaxIn values must match.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_SubleafGet(field, node)                                      \
   (((node)->data.array[CpuidInfoEaxChk(CPUID_FEAT_REG(field),                 \
                                        CPUID_INTERNAL_EAXIN_##field, node)] & \
     CPUID_##field##_MASK) >> CPUID_##field##_SHIFT)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Set --
 *
 *      Sets the flag in info in the fixed-size section.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_Set(flag, info)                                             \
   do {                                                                       \
      ASSERT((CPUID_##flag##_MASK & ((uint32)CPUID_##flag##_MASK - 1)) == 0); \
      CPUID_INFO_SET_EXISTS(flag, info);                                      \
      CPUID_INFO_FLD_REG(flag, info) |= CPUID_##flag##_MASK;                  \
   } while (0)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Clear --
 *
 *      Clears the flag/field in info in the fixed-size section.
 *      Unlike Set or SetTo, this does not mark leaf as existing,
 *      however, as with SetTo(zero) a subsequent read will return zero.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_Clear(flag, info)                                        \
   do {                                                                    \
      ASSERT(!CpuidInfo_IsVendorField((uint32)CPUID_INTERNAL_EAXIN_##flag, \
                                      (uint32)CPUID_INTERNAL_ECXIN_##flag, \
                                      CPUID_FEAT_REG(flag)));              \
      CPUID_INFO_FLD_REG(flag, info) &= ~CPUID_##flag##_MASK;              \
   } while (0)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_SetTo --
 *
 *      Sets the field in info in the fixed-size section.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_SetTo(field, info, val)                                 \
   do {                                                                   \
      CPUID_INFO_SET_EXISTS(field, info);                                 \
      CPUID_INFO_FLD_REG(field, info) =                                   \
         (CPUID_INFO_FLD_REG(field, info) & ~CPUID_##field##_MASK) |      \
         (((val) << CPUID_##field##_SHIFT) & CPUID_##field##_MASK);       \
   } while (0)

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_SubleafSetTo --
 *
 *      Sets the field in a CpuidInfo subleaf node to "val".
 *      Unlike CpuidInfo_SetTo, this overrides the ecxIn value of "field",
 *      which is useful for leaves whose subleaves have a regular format
 *      such as the cache leaves: 4 and 0xb.  The eaxIn values must match.
 *
 *----------------------------------------------------------------------
 */
#define CpuidInfo_SubleafSetTo(field, node, val)                          \
   do {                                                                   \
      CpuidReg reg = CpuidInfoEaxChk(CPUID_FEAT_REG(field),               \
                                     CPUID_INTERNAL_EAXIN_##field, node); \
      (node)->data.array[reg] =                                           \
         ((node)->data.array[reg] & ~CPUID_##field##_MASK) |              \
         (((val) << CPUID_##field##_SHIFT) & CPUID_##field##_MASK);       \
   } while (0)


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FindNode --
 *
 *      Const wrapper for CpuidInfoNodePtr.
 *
 *----------------------------------------------------------------------
 */
static INLINE const CpuidNode *
CpuidInfo_FindNode(const CpuidInfo *info, uint32 eaxIn, uint32 ecxIn)
{
   return CpuidInfoNodePtr(info, eaxIn, ecxIn);
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FindNodeMutable --
 *
 *      Nonconst (mutable) wrapper for CpuidInfoNodePtr
 *
 *----------------------------------------------------------------------
 */
static INLINE CpuidNode *
CpuidInfo_FindNodeMutable(CpuidInfo *info, uint32 eaxIn, uint32 ecxIn)
{
   return CpuidInfoNodePtr(info, eaxIn, ecxIn);
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Regs --
 *
 *      Find CPUIDRegs for eaxIn/ecxIn.  Return NULL if no level.
 *
 *----------------------------------------------------------------------
 */
static INLINE const CPUIDRegs *
CpuidInfo_Regs(const CpuidInfo *info, uint32 eaxIn, uint32 ecxIn)
{
   CpuidNode *n = CpuidInfoNodePtr(info, eaxIn, ecxIn);
   return n != NULL ? &n->data.regs : NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_NodeReg --
 *
 *      Get register data from a node.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_NodeReg(const CpuidNode *n, CpuidReg reg)
{
   return n->data.array[reg];
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Reg --
 *
 *      Get register data from info:eaxIn,ecxIn:reg.  If the level does not
 *      exist, zero is returned.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_Reg(const CpuidInfo *info, uint32 eaxIn, uint32 ecxIn, CpuidReg reg)
{
   CpuidNode *n = CpuidInfoNodePtr(info, eaxIn, ecxIn);
   ASSERT((unsigned)reg < CPUID_NUM_REGS);
   return n != NULL ? n->data.array[reg] : 0;
}


#if !defined(VMM) && !defined(GLM)  && !defined(VMM_BOOTSTRAP) && !defined(COREQUERY)

static INLINE CpuidNode *
CpuidInfo_Add(CpuidInfo *info, uint32 eaxIn, uint32 ecxIn, CPUIDRegs regs)
{
   ASSERT(CpuidInfoNodePtr(info, eaxIn, ecxIn) == NULL);
   return CpuidInfo_Update(info, eaxIn, ecxIn, regs);
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_RegPtr --
 *
 *      Get register data pointer from info:eaxIn,ecxIn:reg.  If the level
 *      does not exist, a new node with all registers zeroed is added for it.
 *      Ensure that vendor field is not being accessed since updating it
 *      requires updates to the cached field in info->vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32 *
CpuidInfo_RegPtr(CpuidInfo *info, uint32 eaxIn, uint32 ecxIn, CpuidReg reg)
{
   CpuidNode *n = CpuidInfoNodePtr(info, eaxIn, ecxIn);
   ASSERT(!CpuidInfo_IsVendorField(eaxIn, ecxIn, reg) &&
          (unsigned)reg < CPUID_NUM_REGS);
   if (n == NULL) {
      CPUIDRegs newRegs = { 0, 0, 0, 0 };
      n = CpuidInfo_Add(info, eaxIn, ecxIn, newRegs);
   }
   return &n->data.array[reg];
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_SetNodeReg --
 *
 *      Set register data for cpuid node.
 *
 *----------------------------------------------------------------------
 */
static INLINE void
CpuidInfo_SetNodeReg(CpuidNode *n, CpuidReg reg, uint32 val)
{
   ASSERT(!CpuidInfo_IsVendorField(n->eaxIn, n->ecxIn, reg) ||
          n->data.array[reg] == val); /* Ensure vendor field is not changed. */
   n->data.array[reg] = val;
}
#endif /* !defined(VMM) && !defined(VMM_BOOTSTRAP) && !defined(COREQUERY) */

#if defined(__GNUC__)
/* Compile time assert that: uint32 x < hi. Returns x. */
#define CPUID_INFO_ASSERT_RANGE(x, hi) \
   ({                                                                         \
      enum { AssertCpuidRngMisused = ((uint32)(x) < (uint32)(hi) ? 1 : -1) }; \
      UNUSED_TYPE(typedef char AssertCpuidRngFailed[AssertCpuidRngMisused]);  \
      (x);                                                                    \
   })
#else
#define CPUID_INFO_ASSERT_RANGE(x, hi) (x)
#endif

#define CPUID_INFO_LEAF_INDEX(_eaxIn, _ecxIn)                               \
   CpuidInfoIndexChk(CPUID_INFO_INDEX_##_eaxIn + _ecxIn,                    \
              CPUID_INFO_ASSERT_RANGE(_ecxIn, CPUID_LEAF_STORAGE_##_eaxIn), \
              CPUID_LEAF_STORAGE_##_eaxIn)

#define CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn) \
           (_info)->node[CPUID_INFO_LEAF_INDEX(_eaxIn, _ecxIn)].data

/*
 *----------------------------------------------------------------------
 *
 * CPUID_INFO_EAX -- (and EBX, ECX, EDX)
 *
 *      Get register from info:level for constant eaxIn/ecxIn.
 *      If the level does not exist, zero is returned.
 *      Use CpuidInfo_Reg if eaxIn or ecxIn is not constant.
 *
 *----------------------------------------------------------------------
 */
#define CPUID_INFO_EAX(_info, _eaxIn, _ecxIn) \
           CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.eax

#define CPUID_INFO_EBX(_info, _eaxIn, _ecxIn) \
           CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.ebx

#define CPUID_INFO_ECX(_info, _eaxIn, _ecxIn) \
           CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.ecx

#define CPUID_INFO_EDX(_info, _eaxIn, _ecxIn) \
           CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.edx

#define CPUID_INFO_REGS(_info, _eaxIn, _ecxIn) \
           CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs


#if !defined(VMM) && !defined(GLM)  && !defined(VMM_BOOTSTRAP) && !defined(COREQUERY)
#define CPUID_INFO_SET_LEAF_EXISTS(_info, _eaxIn, _ecxIn)                  \
   do {                                                                    \
      (_info)->node[CPUID_INFO_LEAF_INDEX(_eaxIn, _ecxIn)].eaxIn = _eaxIn; \
      (_info)->node[CPUID_INFO_LEAF_INDEX(_eaxIn, _ecxIn)].ecxIn = _ecxIn; \
   } while (0)

/*
 *----------------------------------------------------------------------
 *
 * CPUID_INFO_SET_EAX -- (and EBX, ECX, EDX)
 *
 *      Set register from info:level for constant eaxIn/ecxIn to val.
 *      The level must exist.  The vendor fields must not be set.
 *
 *----------------------------------------------------------------------
 */
#define CPUID_INFO_SET_EAX(_info, _eaxIn, _ecxIn, _val)         \
   do {                                                         \
      CPUID_INFO_SET_LEAF_EXISTS(_info, _eaxIn, _ecxIn);        \
      CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.eax = (_val); \
   } while (0)

#define CPUID_INFO_SET_EBX(_info, _eaxIn, _ecxIn, _val)         \
   do {                                                         \
      ASSERT(_eaxIn != 0); /* Not vendor. */                    \
      CPUID_INFO_SET_LEAF_EXISTS(_info, _eaxIn, _ecxIn);        \
      CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.ebx = (_val); \
   } while (0)

#define CPUID_INFO_SET_ECX(_info, _eaxIn, _ecxIn, _val)         \
   do {                                                         \
      ASSERT(_eaxIn != 0); /* Not vendor. */                    \
      CPUID_INFO_SET_LEAF_EXISTS(_info, _eaxIn, _ecxIn);        \
      CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.ecx = (_val); \
   } while (0)

#define CPUID_INFO_SET_EDX(_info, _eaxIn, _ecxIn, _val)         \
   do {                                                         \
      ASSERT(_eaxIn != 0); /* Not vendor. */                    \
      CPUID_INFO_SET_LEAF_EXISTS(_info, _eaxIn, _ecxIn);        \
      CPUID_INFO_LEAF(_info, _eaxIn, _ecxIn).regs.edx = (_val); \
   } while (0)

#endif /* !defined(VMM) && !defined(VMM_BOOTSTRAP) && !defined(COREQUERY) */

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Vendor --
 * CpuidInfo_VendorStr --
 * CpuidInfo_VendorIsIntel --
 * CpuidInfo_VendorIsAMD --
 * CpuidInfo_VendorIsCyrix --
 * CpuidInfo_VendorIsVia --
 * CpuidInfo_VendorIsHygon --
 *
 *      Returns the vendor enum or string for "info", or TRUE iff the
 *      vendor is Intel, AMD, Cyrix, Via, or Hygon respectively.
 *
 *----------------------------------------------------------------------
 */

#define CpuidInfo_Vendor(info) ((info)->vendor)

static INLINE const char *
CpuidInfo_VendorStr(const CpuidInfo *info)
{
   return info->vendorStr;
}

static INLINE Bool
CpuidInfo_VendorIsIntel(const CpuidInfo *info)
{
   return CpuidInfo_Vendor(info) == CPUID_VENDOR_INTEL;
}

static INLINE Bool
CpuidInfo_VendorIsAMD(const CpuidInfo *info)
{
   return CpuidInfo_Vendor(info) == CPUID_VENDOR_AMD;
}

static INLINE Bool
CpuidInfo_VendorIsCyrix(const CpuidInfo *info)
{
   return CpuidInfo_Vendor(info) == CPUID_VENDOR_CYRIX;
}

static INLINE Bool
CpuidInfo_VendorIsVia(const CpuidInfo *info)
{
   return CpuidInfo_Vendor(info) == CPUID_VENDOR_VIA;
}

static INLINE Bool
CpuidInfo_VendorIsHygon(const CpuidInfo *info)
{
   return CpuidInfo_Vendor(info) == CPUID_VENDOR_HYGON;
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_Version --
 *
 *      Returns the version for "info".
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_Version(const CpuidInfo *info)
{
   return CPUID_INFO_EAX(info, 1, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_EffectiveModel --
 *
 *      Returns the effective model for "info"
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_EffectiveModel(const CpuidInfo *info)
{
   return CPUID_EFFECTIVE_MODEL(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_EffectiveFamily --
 *
 *      Returns the effective family for "info"
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_EffectiveFamily(const CpuidInfo *info)
{
   return CPUID_EFFECTIVE_FAMILY(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_NumLoLevels --
 *
 *      Returns the number of low valid levels for "info".
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_NumLoLevels(const CpuidInfo *info)
{
   return CpuidInfo_Get(NUMLEVELS, info);
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_NumHiLevels --
 *
 *      Returns the number of high valid levels for "info".
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
CpuidInfo_NumHiLevels(const CpuidInfo *info)
{
   return CpuidInfo_Get(NUM_EXT_LEVELS, info);
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIs486 --
 *
 *      Returns TRUE if the info passed in is of family 486.  Does not
 *      check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIs486(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_486(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsP5 --
 *
 *      Returns TRUE if the info passed in is of family P5.  Does not
 *      check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsP5(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_P5(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsP6 --
 *
 *      Returns TRUE if the info passed in is of family P6.  Does not
 *      check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsP6(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_P6(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsP4 --
 *
 *      Returns TRUE if the info passed in is of family P4.  Does not
 *      check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsP4(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_PENTIUM4(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsPentiumM --
 *
 *      Returns TRUE if the info passed in is from the Pentium M
 *      architectures (includes Yonah).  Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsPentiumM(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_PENTIUM_M(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsCore --
 *
 *      Returns TRUE if the info passed in is from the Core
 *      architecture (does not include the P-M based Yonah,
 *      so Conroe/Merom/Penryn etc).
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsCore(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_CORE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsNehalem --
 *
 *      Returns TRUE if the info passed in is Nehalem architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsNehalem(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_NEHALEM(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsSandyBridge --
 *
 *      Returns TRUE if the info passed in is SandyBridge architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsSandyBridge(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_SANDYBRIDGE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsHaswell --
 *
 *      Returns TRUE if the info passed in is Haswell architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsHaswell(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_HASWELL(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsSkylake --
 *
 *      Returns TRUE if the info passed in is Skylake architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsSkylake(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_SKYLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsIceLake --
 *
 *      Returns TRUE if the info passed in is Ice Lake architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsIceLake(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_ICELAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsSapphireRapids --
 *
 *      Returns TRUE if the info passed in is Sapphire Rapids architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsSapphireRapids(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_SAPPHIRERAPIDS(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsGraniteRapids --
 *
 *      Returns TRUE if the info passed in is Granite Rapids architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsGraniteRapids(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_GRANITERAPIDS(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsSierraForest --
 *
 *      Returns TRUE if the info passed in is Sierra Forest architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsSierraForest(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_SIERRAFOREST(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsSilvermont --
 *
 *      Returns TRUE if the info passed in is Silvermont architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsSilvermont(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_SILVERMONT(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsKnightsLanding --
 *
 *      Returns TRUE if the info passed in is Knights Landing model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsKnightsLanding(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_KNIGHTS_LANDING(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsKnightsMill --
 *
 *      Returns TRUE if the info passed in is Knights Mill model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsKnightsMill(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_KNIGHTS_MILL(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsCenterton --
 *
 *      Returns TRUE if the cpuidInfo passed in is from an Atom that
 *      is a Centerton (Saltwell CPU core, AKA diamondville, pineview,
 *      cedarview). This is the Atom architecture that has "in order"
 *      execution often used in Netbooks, NAS etc, was not generally
 *      considered a server part.

 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsCenterton(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_CENTERTON(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsAvoton --
 *
 *      Returns TRUE if the cpuidInfo passed in is from an Atom that
 *      is an Avoton (which has the Silvermont CPU architecure, out of
 *      order execution etc). This is architecturally quite different
 *      and more advance than prior Atoms.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsAvoton(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_AVOTON(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsBayTrail --
 *
 *      Returns TRUE if the cpuidInfo passed in is from an Atom that
 *      is a Bay Trail (which has the Silvermont CPU architecture,
 *      similar to Avoton).
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsBayTrail(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_BAYTRAIL(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsDenverton --
 *
 *      Returns TRUE if the cpuidInfo passed in is from an Atom that
 *      is an Denverton (based on Goldmont microarchitecture).
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsDenverton(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_DENVERTON(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsSnowRidge --
 *
 *      Returns TRUE if the cpuidInfo passed in is from an Atom that
 *      is a Snow Ridge (based on Tremont microarchitecture).
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsSnowRidge(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_SNOWRIDGE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsTremont --
 *
 *      Returns TRUE if the info passed in is Tremont architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsTremont(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_TREMONT(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_IsAtom --
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_IsAtom(const CpuidInfo *info)
{
   uint32 version = CpuidInfo_Version(info);
   return CPUID_UARCH_IS_SILVERMONT(version) ||
          CPUID_MODEL_IS_DENVERTON(version)  ||
          CPUID_UARCH_IS_TREMONT(version)    ||
          CPUID_MODEL_IS_CENTERTON(version);
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsWestmere --
 *
 *      Returns TRUE if the info passed in is Westmere model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsWestmere(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_WESTMERE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsSandyBridge --
 *
 *      Returns TRUE if the info passed in is SandyBridge model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsSandyBridge(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_SANDYBRIDGE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsIvyBridge --
 *
 *      Returns TRUE if the info passed in is IvyBridge model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsIvyBridge(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_IVYBRIDGE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsHaswell --
 *
 *      Returns TRUE if the info passed in is Haswell model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsHaswell(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_HASWELL(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsBroadwell --
 *
 *      Returns TRUE if the info passed in is Broadwell model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsBroadwell(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_BROADWELL(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsSkylake --
 *
 *      Returns TRUE if the info passed in is Skylake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsSkylake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_SKYLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsKabyLake --
 *
 *      Returns TRUE if the info passed in is Kaby Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsKabyLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_KABYLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsCoffeeLake --
 *
 *      Returns TRUE if the info passed in is Coffee Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsCoffeeLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_COFFEELAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsWhiskeyLake --
 *
 *      Returns TRUE if the info passed in is Whiskey Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsWhiskeyLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_WHISKEYLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsCometLake --
 *
 *      Returns TRUE if the info passed in is Comet Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsCometLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_COMETLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsAmberLake --
 *
 *      Returns TRUE if the info passed in is Amber Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsAmberLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_AMBERLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsCascadeLake --
 *
 *      Returns TRUE if the info passed in is Cascade Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsCascadeLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_CASCADELAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsCannonLake --
 *
 *      Returns TRUE if the info passed in is Cannon Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsCannonLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_CANNONLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsRocketLake --
 *
 *      Returns TRUE if the info passed in is Rocket Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsRocketLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ROCKETLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsAlderLake --
 *
 *      Returns TRUE if the info passed in is Alder Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsAlderLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ALDERLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsSapphireRapids --
 *
 *      Returns TRUE if the info passed in is Sapphire Rapids model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsSapphireRapids(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_SAPPHIRERAPIDS(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsEmeraldRapids --
 *
 *      Returns TRUE if the info passed in is Emerald Rapids model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsEmeraldRapids(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_EMERALDRAPIDS(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsGraniteRapids --
 *
 *      Returns TRUE if the info passed in is Granite Rapids model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsGraniteRapids(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_GRANITERAPIDS(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsSierraForest --
 *
 *      Returns TRUE if the info passed in is Sierra Forest model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsSierraForest(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_SIERRAFOREST(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsMeteorLake --
 *
 *      Returns TRUE if the info passed in is Meteor Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsMeteorLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_METEORLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsLunarLake --
 *
 *      Returns TRUE if the info passed in is Lunar Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsLunarLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_LUNARLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsArrowLake --
 *
 *      Returns TRUE if the info passed in is Arrow Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsArrowLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ARROWLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsLunarLake --
 *
 *      Returns TRUE if the info passed in is Lunar Lake architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsLunarLake(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_LUNARLAKE(CpuidInfo_Version(info));
}

 /*
  *----------------------------------------------------------------------
  *
  * CpuidInfo_ModelIsClearwaterForest --
  *
  *      Returns TRUE if the info passed in is Clearwater Forest model.
  *      Does not check vendor.
  *
  *----------------------------------------------------------------------
  */
static INLINE Bool
CpuidInfo_ModelIsClearwaterForest(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_CLEARWATERFOREST(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsClearWaterForest --
 *
 *      Returns TRUE if the info passed in is Clearwater Forest architecture.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_UArchIsClearwaterForest(const CpuidInfo *info)
{
   return CPUID_UARCH_IS_CLEARWATERFOREST(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsRaptorLake --
 *
 *      Returns TRUE if the info passed in is Raptor Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsRaptorLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_RAPTORLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsTigerLake --
 *
 *      Returns TRUE if the info passed in is Tiger Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsTigerLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_TIGERLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsCooperLake --
 *
 *      Returns TRUE if the info passed in is Cooper Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsCooperLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_COOPERLAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsIceLake --
 *
 *      Returns TRUE if the info passed in is Ice Lake model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsIceLake(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ICELAKE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsK7 --
 *
 *      Returns TRUE if the info passed in is of family K7.  Does not
 *      check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsK7(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_K7(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsK8 --
 *
 *      Returns TRUE if the info passed in is K8 (but not K8L).  Does
 *      not check vendor.
 *
 *      K8: Original 64-bit AMD prior to nested paging.
 *      uArches: Hammer, Hammer NPT (adds SSE3)
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsK8(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_K8(CpuidInfo_Version(info)) ||
          CPUID_FAMILY_IS_K8MOBILE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsK8Ext --
 *
 *      Returns TRUE if the info passed in is K8 (but not K8L), and
 *      has a nonzero extended model (that is, effective model >= 0x10).
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsK8Ext(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_K8EXT(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsK8L --
 *
 *      Returns TRUE if the info passed in is of family K8L.  Does
 *      not check vendor.
 *
 *      K8L: Adds nested paging, SSE4a, POPCNT, LZCNT to K8.
 *      uArch: Greyhound and Llano (latter has integrated gfx).
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsK8L(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_K8L(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsK8Star --
 *
 *      Returns TRUE if the info passed in is of family K8 or K8L.  Does
 *      not check vendor.
 *
 *      Read the function name as "K8*", as in "K8 wildcard".
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsK8Star(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_K8STAR(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsK8Mobile --
 *
 *      Returns TRUE if the info passed in is of family K8 mobile
 *      (Hammer Griffin).
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_FamilyIsK8Mobile(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_K8MOBILE(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsBarcelona,
 * CpuidInfo_ModelIsShanghai,
 * CpuidInfo_ModelIsIstanbulMagny,
 * CpuidInfo_ModelIsPharoahHound,
 * CpuidInfo_ModelIsBulldozer
 * CpuidInfo_ModelIsKyoto
 * CpuidInfo_ModelIsPiledriver
 * CpuidInfo_ModelIsSteamroller
 * CpuidInfo_ModelIsExcavator
 *
 *
 *      Returns TRUE if the info passed in is the named model.
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
CpuidInfo_ModelIsBarcelona(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_BARCELONA(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsShanghai(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_SHANGHAI(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsIstanbulMagny(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ISTANBUL_MAGNY(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsPharoahHound(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_PHAROAH_HOUND(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsBulldozer(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_BULLDOZER(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsKyoto(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_KYOTO(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsZen(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ZEN(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsZen2(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ZEN2(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsZen3(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ZEN3(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsZen4(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ZEN4(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsZen5(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_ZEN5(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsPiledriver(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_PILEDRIVER(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsSteamroller(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_STEAMROLLER(CpuidInfo_Version(info));
}

static INLINE Bool
CpuidInfo_ModelIsExcavator(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_EXCAVATOR(CpuidInfo_Version(info));
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsLlano --
 *
 *      Returns TRUE if the info passed in is of family Llano.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsLlano(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_LLANO(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsBobcat --
 *
 *      Returns TRUE if the info passed in is of family Bobcat.
 *      (1-10 watt parts akin to Atom.  Integrated gfx.  No 3DNow.
 *      Complete uArch redo compared to K8L.  Not related to Bulldozer.)
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsBobcat(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_BOBCAT(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsBulldozer --
 *
 *      Returns TRUE if the info passed in is of family Bulldozer
 *      (uArch redo from K8L; AVX, FMA4, hyperthreading.  No 3DNow.)
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsBulldozer(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_BULLDOZER(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsKyoto --
 *
 *      Returns TRUE if the info passed in is of family Kyoto.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsKyoto(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_KYOTO(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsZen --
 *
 *      Returns TRUE if the info passed in is of family Zen.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsZen(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_ZEN(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsZen3 --
 *
 *      Returns TRUE if the info passed in is of family Zen3.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsZen3(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_ZEN3(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsZen5 --
 *
 *      Returns TRUE if the info passed in is of family Zen5.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsZen5(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_ZEN5(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_FamilyIsDhyana --
 *
 *      Returns TRUE if the info passed in is of family Dhyana.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_FamilyIsDhyana(const CpuidInfo *info)
{
   return CPUID_FAMILY_IS_DHYANA(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ModelIsDhyanaA --
 *
 *      Returns TRUE if the info passed in is of model Dhyana A.
 *
 *      Does not check vendor.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_ModelIsDhyanaA(const CpuidInfo *info)
{
   return CPUID_MODEL_IS_DHYANA_A(CpuidInfo_Version(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_UArchIsZen --
 *
 *      Returns TRUE if the info passed in is Zen architecture.
 *      Hygon's Dhyana family is also based on AMD's Zen architecture.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_UArchIsZen(const CpuidInfo *info)
{
   return (CpuidInfo_VendorIsAMD(info) &&
           (CpuidInfo_FamilyIsZen(info) || CpuidInfo_FamilyIsZen3(info) ||
            CpuidInfo_FamilyIsZen5(info))) ||
          (CpuidInfo_VendorIsHygon(info) && CpuidInfo_FamilyIsDhyana(info));
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_IsAMDArch --
 *
 *      Returns TRUE if the info passed in represents a CPU that
 *      implements AMD-like, non-architectural behavior. As Intel,
 *      AMD, and Hygon are currently the only three supported CPU
 *      vendors, it suffices to check for non-Intel CPUs.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_IsAMDArch(const CpuidInfo *info)
{
   return !CpuidInfo_VendorIsIntel(info);
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_GetXCR0Master --
 * CpuidInfo_GetXSSMaster --
 *
 *      Return the XCR0 or XSS master bit mask.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
CpuidInfo_GetXCR0Master(const CpuidInfo *cpuid)
{
   return QWORD(CPUID_INFO_EDX(cpuid, 0xd, 0), CPUID_INFO_EAX(cpuid, 0xd, 0));
}


static INLINE uint64
CpuidInfo_GetXSSMaster(const CpuidInfo *cpuid)
{
   return QWORD(CPUID_INFO_EDX(cpuid, 0xd, 1), CPUID_INFO_ECX(cpuid, 0xd, 1));
}


/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_GetMaxPhysAddrSize --
 *
 *      Return the maximum physical address size, or 36 if the CPUID
 *      field is not present.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint8
CpuidInfo_GetMaxPhysAddrSize(const CpuidInfo *cpuid)
{
   uint8 maxPhysAddrSize;

   if (CpuidInfo_NumHiLevels(cpuid) >= 0x80000008) {
      maxPhysAddrSize = CpuidInfo_Get(PHYS_BITS, cpuid);
      ASSERT(32 <= maxPhysAddrSize && maxPhysAddrSize <= 52);
   } else {
      maxPhysAddrSize = 36;
   }
   return maxPhysAddrSize;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CpuidInfo_TDXEnabled --
 *
 *      Check whether we are running in a guest VM with TDX enabled.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
CpuidInfo_TDXEnabled(const CpuidInfo *info)
{
   return CpuidInfo_VendorIsIntel(info) &&
          CpuidInfo_IsSet(HYPERVISOR, info) &&
          CpuidInfo_NumLoLevels(info) >= 0x21 &&
          CPUID_IsRawVendor(CpuidInfo_Regs(info, 0x21, 0),
                            CPUID_INTEL_TDX_VENDOR_STRING);
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_ShortLevelName --
 *
 *      Returns the short level name (eg. 81 instead of 80000001).
 *
 *----------------------------------------------------------------------
 */
extern const char *const CpuidInfo_shortLevelName[CPUID_NUM_KNOWN_LEVELS];

static INLINE const char *
CpuidInfo_ShortLevelName(CpuidLevel lvl)
{
   return CpuidInfo_shortLevelName[lvl];
}

/*
 *----------------------------------------------------------------------
 *
 * CpuidInfo_RegName --
 *
 *      Returns the cpuid register name: eax - edx.
 *
 *----------------------------------------------------------------------
 */
extern const char *const CpuidInfo_regName[CPUID_NUM_REGS];

static INLINE const char *
CpuidInfo_RegName(CpuidReg reg)
{
   return CpuidInfo_regName[reg];
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
