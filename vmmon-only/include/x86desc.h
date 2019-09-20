/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#ifndef _X86DESC_H_
#define _X86DESC_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "x86segdescrs.h"
#include "vm_assert.h"

/*
 * Symbolic names for various offsets used to construct segment descriptors.
 */
/* Lower dword */
#define X86DESC_BASE_LO_SHIFT  16
#define X86DESC_LIMIT_LO_SHIFT  0
/* Upper dword */
#define X86DESC_BASE_HI_SHIFT  24
#define X86DESC_GRAN_SHIFT     23
#define X86DESC_DB_SHIFT       22
#define X86DESC_LONG_SHIFT     21
#define X86DESC_AVL_SHIFT      20
#define X86DESC_LIMIT_HI_SHIFT 16
#define X86DESC_P_SHIFT        15
#define X86DESC_DPL_SHIFT      13
#define X86DESC_S_SHIFT        12
#define X86DESC_TYPE_SHIFT      8
#define X86DESC_BASE_MID_SHIFT  0

#define X86DESC_TYPE_WIDTH      4


/* 
 * Descriptors store a 32-bit or 64-bit segment base in 3 parts
 * (low, mid, high) and the 20-bit limit in 2 parts (low, high).
 * The following macros extract these components from the original
 * base and limit.
 */

#define BASE_LO(_dw)              ((uint32)((_dw) & 0xffff))
#define BASE_MID(_dw)             ((uint32)(((_dw) >> 16) & 0xff))
#define BASE_HI(_dw)              ((uint32)(((_dw) >> 24) & 0xff))
#define BASE64_LO(_qw)            ((uint64)((_qw) & 0xffffff))
#define BASE64_MID(_qw)           ((uint64)(((_qw) >> 24) & 0xff))
#define BASE64_HI(_qw)            ((uint64)((_qw) >> 32))
#define LIMIT_LO(_dw)             ((uint32)((_dw) & 0xffff)) /* Descriptor */
#define LIMIT_HI(_dw)             ((uint32)(((_dw) >> 16) & 0xf))
#define OFFSET_LO(_dw)            ((uint32)((_dw) & 0xffff)) /* CallGate */
#define OFFSET_HI(_dw)            ((uint32)(((_dw) >> 16) & 0xffff))

/*
 * Accessor functions for descriptors.  
 * 
 * Note: The fields of a descriptor should always be accessed with the
 * following functions.  ANSI C specifies that any expression
 * involving integer types smaller than an int have all the variables
 * automatically promoted to a *signed* int.  This means that
 * expressions that use the fields of a descriptor directly will
 * treat them as signed quantities.  This could have unwanted effects,
 * e.g. base and limit added together could result in a negative
 * quantity.  The functions that read the bitfields always return an
 * unsigned integer so as to avoid any potential signed/unsigned
 * problems.  Functions to write the bitfields are also provided for
 * consistency.  
 */

static INLINE uint32 Desc_Type(const Descriptor *d)     { return d->type; }
static INLINE uint32 Desc_S(const Descriptor *d)        { return d->S; }
static INLINE uint32 Desc_DPL(const Descriptor *d)      { return d->DPL; }
static INLINE uint32 Desc_Present(const Descriptor *d)  { return d->present; }
static INLINE uint32 Desc_AVL(const Descriptor *d)      { return d->AVL; }
static INLINE uint32 Desc_LongMode(const Descriptor *d) { return d->longmode; }
static INLINE uint32 Desc_DB(const Descriptor *d)       { return d->DB; }
static INLINE uint32 Desc_Gran(const Descriptor *d)     { return d->gran; }

static INLINE uint32
Desc64_Type(const Descriptor64 *d)
{
   return (uint32)d->type;
}

static INLINE uint32
Desc64_S(const Descriptor64 *d)
{
   return (uint32)d->S;
}

static INLINE uint32
Desc64_DPL(const Descriptor64 *d)
{
   return (uint32)d->DPL;
}

static INLINE uint32
Desc64_Present(const Descriptor64 *d)
{
   return (uint32)d->present;
}

static INLINE uint32
Desc64_AVL(const Descriptor64 *d)
{
   return (uint32)d->AVL;
}

static INLINE uint32
Desc64_Gran(const Descriptor64 *d)
{
   return (uint32)d->gran;
}

static INLINE uint32
Desc64_ExtAttrs(const Descriptor64 *d)
{
   return (uint32)d->ext_attrs;
}

static INLINE LA32
Desc_GetBase(const Descriptor *d)
{
   return ((unsigned)d->base_hi  << 24) |
          ((unsigned)d->base_mid << 16) | d->base_lo;
}

static INLINE LA64
Desc64_GetBase(const Descriptor64 *d)
{
   return ((uint64)d->base_hi << 32)  |
          ((uint64)d->base_mid << 24) | (uint64)d->base_lo;
}

static INLINE LA32
Desc64_GetBaseHi(const Descriptor64 *d)
{
   return (LA32)d->base_hi;
}

static INLINE VA32
Desc_GetLimit(const Descriptor *d)
{
   return ((unsigned)d->limit_hi << 16) | d->limit_lo;
}

static INLINE VA32
Desc64_GetLimit(const Descriptor64 *d)
{
   return (VA32)(((unsigned)d->limit_hi << 16) | d->limit_lo);
}

static INLINE Bool
Desc_EqualIgnoreAccessed(const Descriptor *d1, const Descriptor *d2)
{
   const DescriptorUnion *du1 = (const DescriptorUnion*) d1;
   const DescriptorUnion *du2 = (const DescriptorUnion*) d2;
   uint32 mask = ~0u;
   if (Desc_S(d1)) {
      mask = ~(1u << 8);
   }
   return du1->word[0] == du2->word[0] &&
      (du1->word[1] & mask) == (du2->word[1] & mask);
}

static INLINE Bool
Desc64_EqualIgnoreAccessed(const Descriptor64 *d1, const Descriptor64 *d2)
{
   const Descriptor64Union *du1 = (const Descriptor64Union*) d1;
   const Descriptor64Union *du2 = (const Descriptor64Union*) d2;
   uint32 mask = ~0u;
   if (Desc64_S(d1)) {
      mask = ~(1u << 8);
   }
   return du1->word[0] == du2->word[0] &&
          (du1->word[1] & mask) == (du2->word[1] & mask) &&
          du1->qword[1] == du2->qword[1];
}

static INLINE void Desc_SetType(Descriptor *d, uint32 val)     { d->type     = val; }
static INLINE void Desc_SetS(Descriptor *d, uint32 val)        { d->S        = val; }
static INLINE void Desc_SetDPL(Descriptor *d, uint32 val)      { d->DPL      = val; }
static INLINE void Desc_SetPresent(Descriptor *d, uint32 val)  { d->present  = val; }
static INLINE void Desc_SetDB(Descriptor *d, uint32 val)       { d->DB       = val; }
static INLINE void Desc_SetLongmode(Descriptor *d, uint32 val) { d->longmode = val; }
static INLINE void Desc_SetGran(Descriptor *d, uint32 val)     { d->gran     = val; }

static INLINE void Desc64_SetType(Descriptor64 *d, uint32 val)    { d->type     = val; }
static INLINE void Desc64_SetS(Descriptor64 *d, uint32 val)       { d->S        = val; }
static INLINE void Desc64_SetDPL(Descriptor64 *d, uint32 val)     { d->DPL      = val; }
static INLINE void Desc64_SetPresent(Descriptor64 *d, uint32 val) { d->present  = val; }
static INLINE void Desc64_SetGran(Descriptor64 *d, uint32 val)    { d->gran     = val; }

static INLINE void
Desc_SetBase(Descriptor *d, LA32 newBase)
{
   d->base_hi  = BASE_HI(newBase);
   d->base_mid = BASE_MID(newBase);
   d->base_lo  = BASE_LO(newBase);
   ASSERT(Desc_GetBase(d) == newBase);
}

static INLINE void
Desc64_SetBase(Descriptor64 *d, LA64 newBase)
{
   d->base_hi  = (uint32)BASE64_HI(newBase);
   d->base_mid = (uint32)BASE64_MID(newBase);
   d->base_lo  = (uint32)BASE64_LO(newBase);
   ASSERT(Desc64_GetBase(d) == newBase);
}

static INLINE void
Desc_SetLimit(Descriptor *d, VA32 newLimit)
{
   d->limit_lo = LIMIT_LO(newLimit);
   d->limit_hi = LIMIT_HI(newLimit);
   ASSERT(Desc_GetLimit(d) == newLimit);
}

static INLINE void
Desc64_SetLimit(Descriptor64 *d, VA32 newLimit)
{
   d->limit_lo = LIMIT_LO(newLimit);
   d->limit_hi = LIMIT_HI(newLimit);
   ASSERT(Desc64_GetLimit(d) == newLimit);
}
/*
 *-----------------------------------------------------------------------------
 *
 * Desc_SetDescriptor --
 *
 *      Set a descriptor with the specified properties.
 *
 *      NOTE: the implementation of Desc_SetDescriptor() assumes a 
 *      little-endian byte order.  
 *
 *-----------------------------------------------------------------------------
 */
static INLINE void 
Desc_SetDescriptor(Descriptor *d, LA32 base, VA32 limit, uint32 type, 
                   uint32 S, uint32 DPL, uint32 present, uint32 DB, 
                   uint32 gran)
{
   DescriptorUnion *desc = (DescriptorUnion *) d;

   desc->word[0] = BASE_LO(base)   << X86DESC_BASE_LO_SHIFT  |
                   LIMIT_LO(limit) << X86DESC_LIMIT_LO_SHIFT;

   desc->word[1] = BASE_HI(base)   << X86DESC_BASE_HI_SHIFT  | 
                   gran            << X86DESC_GRAN_SHIFT     | 
                   DB              << X86DESC_DB_SHIFT       | 
                   LIMIT_HI(limit) << X86DESC_LIMIT_HI_SHIFT | 
                   present         << X86DESC_P_SHIFT        | 
                   DPL             << X86DESC_DPL_SHIFT      |
                   S               << X86DESC_S_SHIFT        | 
                   type            << X86DESC_TYPE_SHIFT     | 
                   BASE_MID(base)  << X86DESC_BASE_MID_SHIFT;

   /* 
    * Assert that all the fields were properly filled in.
    */

   ASSERT(Desc_GetBase(d) == base);
   ASSERT(Desc_GetLimit(d) == limit);
   ASSERT(Desc_Type(d) == type);
   ASSERT(Desc_S(d) == S);
   ASSERT(Desc_DPL(d) == DPL);
   ASSERT(Desc_Present(d) == present);
   ASSERT(Desc_DB(d) == DB);
   ASSERT(Desc_Gran(d) == gran);
   ASSERT(d->AVL == 0);
   ASSERT(d->longmode == 0);
}


/*
 * Accessor functions that operate directly on a descriptor.  These
 * are included only for backwards compatibility with existing macros.  
 */

static INLINE uint32 DESC_TYPE(Descriptor d)       { return d.type; }
static INLINE uint32 DESC_S(Descriptor d)          { return d.S; }
static INLINE uint32 DESC_DPL(Descriptor d)        { return d.DPL; }
static INLINE uint32 DESC_PRESENT(Descriptor d)    { return d.present; }

#define DT_CODE(_d)               ( DESC_S(_d) && (DESC_TYPE(_d) & 0x8) == 0x8)
#define DT_CONFORMING_CODE(_d)    ( DESC_S(_d) && (DESC_TYPE(_d) & 0xc) == 0xc)
#define DT_NONCONFORMING_CODE(_d) ( DESC_S(_d) && (DESC_TYPE(_d) & 0xc) == 0x8)
#define DT_READABLE_CODE(_d)      ( DESC_S(_d) && (DESC_TYPE(_d) & 0xa) == 0xa)
#define DT_DATA(_d)               ( DESC_S(_d) && (DESC_TYPE(_d) & 0x8) == 0x0)
#define DT_WRITEABLE_DATA(_d)     ( DESC_S(_d) && (DESC_TYPE(_d) & 0xa) == 0x2)
#define DT_EXPAND_DOWN(_d)        ( DESC_S(_d) && (DESC_TYPE(_d) & 0xc) == 0x4)
#define DT_CALL_GATE(_d)          (!DESC_S(_d) && (DESC_TYPE(_d) & 0x7) == 0x4)
#define DT_CALL_GATE32(_d)        (!DESC_S(_d) && (DESC_TYPE(_d) & 0xf) == 0xc)
#define DT_LDT(_d)                (!DESC_S(_d) && (DESC_TYPE(_d) & 0xf) == 0x2)
#define DT_TASK_GATE(_d)          (!DESC_S(_d) && (DESC_TYPE(_d) & 0xf) == 0x5)
#define DT_TSS(_d)                (!DESC_S(_d) && (DESC_TYPE(_d) & 0x5) == 0x1)
#define DT_AVAIL_TSS(_d)          (!DESC_S(_d) && (DESC_TYPE(_d) & 0x7) == 0x1)

#define DT64_TSS(_d)              (!Desc64_S(&_d) && (Desc64_Type(&_d) & 0xd) == 0x9)
#define DT64_AVAIL_TSS(_d)        (!Desc64_S(&_d) && Desc64_Type(&_d) == 0x9)
#define DT64_LDT(_d)              (!Desc64_S(&_d) && Desc64_Type(&_d) == 0x2)

#define DT_ACCESS                 0x1
#define DT_32BIT                  0x8
#define DT_TSS_BUSY               0x2

#define DATA_DESC                 0x2
#define CODE_DESC                 0xa
#define LDT_DESC                  0x2
#define TASK_DESC                 0x9  // TSS available
#define TASK_DESC_BUSY            0xb  // TSS busy
#define TASK16_DESC               0x1  // 16-bit TSS available
#define TASK16_DESC_BUSY          0x3  // 16-bit TSS busy


/*
 *-----------------------------------------------------------------------------
 *
 * Desc_SetSystemDescriptor64 --
 *
 *      Set a 16 byte long mode system descriptor with the specified properties.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Desc_SetSystemDescriptor64(Descriptor *d, uint64 base, uint32 limit,
                           uint32 type, uint32 DPL, uint32 present,
                           uint32 DB, uint32 gran)
{
   uint32 baseHi = (uint32)(base >> 32);
   uint32 baseLo = (uint32)base;

   /*
    * The first half of a 16-byte descriptor is a valid 8-byte descriptor
    * so allow TASK16_DESC
    */
   ASSERT(type == TASK_DESC || type == TASK_DESC_BUSY ||
          type == TASK16_DESC || type == TASK16_DESC_BUSY || type == LDT_DESC);

   Desc_SetDescriptor(d, baseLo, limit, type, 0, DPL, present, DB, gran);
   *(uint32 *)(++d) = baseHi; // High 32 bits of base.
   *(((uint32 *)d) + 1) = 0;  // Bits 8-12 of highest word are 0, rest ignored.
}


/*
 *----------------------------------------------------------------------
 * Desc_DBSize --
 *   Given descriptor, return the code/stack size that it specifies.
 *----------------------------------------------------------------------
 */
static INLINE unsigned
Desc_DBSize(const Descriptor *desc)
{
   /*
    * Code/stack size is determined by the D/B bit; bit 22 of the 2nd word.
    * Shift the bit to position 1, mask it out, add 2.
    *
    * Result: 2 or 4.
    */
   unsigned cSz = ((((const uint32 *)desc)[1] >> 21) & 2) + 2;
   ASSERT(cSz == (Desc_DB(desc) ? 4U : 2U));
   return cSz;
}


/*
 *----------------------------------------------------------------------
 *
 * Desc_ExpandedLimit --
 *
 *      Return the limit in bytes of the descriptor.  This is correct for both
 *      expand up and expand down limits.  For expand down limits, the page 
 *      corresponding to the limit << 12 is *not* included in the segment when
 *      the granularity bit is set.  This makes it correct to append 0xfff to
 *      make sure accesses to this first page raise a GP.  
 *
 * Results:
 *      The limit in bytes of the descriptor.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE VA32
Desc_ExpandedLimit(const Descriptor *d)
{
   VA32 limit = Desc_GetLimit(d);
   if (Desc_Gran(d)) {
      limit <<= 12;
      limit |= 0xfff;
   }
   return limit;
}

static INLINE VA32
Desc64_ExpandedLimit(const Descriptor64 *d)
{
   VA32 limit = Desc64_GetLimit(d);
   if (Desc64_Gran(d)) {
      limit <<= 12;
      limit |= 0xfff;
   }
   return limit;
}

/*
 *----------------------------------------------------------------------
 *
 * Desc_PackLimit --
 *
 *      Convert the limit of a descriptor into a 21-bit packed representation
 *
 * Results:
 *      packed representation of the limit
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint32
Desc_PackLimit(VA32 limit)
{
   if (limit < (1 << 20)) {
      return limit;
   } else {
      ASSERT((limit & 0xfff) == 0xfff);
      return (limit >> 12) | (1 << 20);
   }
}

/*
 *----------------------------------------------------------------------
 *
 * Desc_UnpackLimit --
 *
 *      Decode the representation of the limit as encoded by Desk_PackLimit
 *
 * Results:
 *      unpacked representation of the limit
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE VA
Desc_UnpackLimit(uint32 limit)
{
   ASSERT(limit < (1 << 21));
   if (limit & (1 << 20)) {
      limit = (limit << 12) | 0xfff;
   }
   return limit;
}


/*
 * For expand-down segments, valid offsets range from limit+1 to 
 * 0xffff or 0xffffffff, depending on the D/B bit in the descriptor.  
 */
static INLINE Bool
Desc_InBoundsExpandDown(VA vaddr, VA limit, uint32 size, VA supremum)
{
   ASSERT(supremum == 0xffff || supremum == 0xffffffff);
   return vaddr > limit && vaddr <= supremum && size - 1 <= supremum - vaddr;
}


/* For expand-up segments, valid offsets range from 0 to limit. */
static INLINE Bool
Desc_InBoundsExpandUp(VA vaddr, VA limit, uint32 size)
{
   return vaddr <= limit && size - 1 <= limit - vaddr;
}


/* Interrupt Gates
 *
 */

typedef struct InterruptGate32 {
   unsigned   offset_lo : 16;
   unsigned   segment   : 16;
   unsigned   unused    : 5;
   unsigned   zero      : 3;
   unsigned   type      : 5;
   unsigned   DPL       : 2;
   unsigned   present   : 1;
   unsigned   offset_hi : 16;
} InterruptGate32;

/*
 * Call Gates.
 */

typedef struct Gate {
   unsigned   offset_lo : 16;
   unsigned   segment   : 16;
   unsigned   params    : 5;
   unsigned   unused    : 3;
   unsigned   type      : 5;
   unsigned   DPL       : 2;
   unsigned   present   : 1;
   unsigned   offset_hi : 16;
} Gate;

#define GATE_OFFSET(_gate)       (((_gate).offset_hi << 16) | (_gate).offset_lo)

#define GATE_OFFSET_LO(_dw)      (((int)(_dw)) & 0xffff)
#define GATE_OFFSET_HI(_dw)      ((((int)(_dw)) >> 16) & 0xffff)

#define CALL_GATE                0x04
#define TASK_GATE                0x05
#define INTER_GATE               0x0e
#define TRAP_GATE                0x0f
#define INTER_GATE_16            0x06
#define TRAP_GATE_16             0x07

#define GT_CALL(_gate)           (((_gate).type & 0x17) == 0x04)
#define GT_TASK(_gate)           (((_gate).type & 0x1f) == 0x05)
#define GT_INTR(_gate)           (((_gate).type & 0x17) == 0x06)
#define GT_TRAP(_gate)           (((_gate).type & 0x17) == 0x07)
#define GT_32BIT                 0x08
#define GT_32BIT_INTR            0xe
#define GT_32BIT_TRAP            0xf

#define GT_64BIT_INTR            0xe
#define GT_64BIT_TRAP            0xf
#define GT_64BIT_CALL            0xc
#define GT64_INTR(_gate)         ((_gate).type == GT_64BIT_INTR)
#define GT64_TRAP(_gate)         ((_gate).type == GT_64BIT_TRAP)
#define GT64_CALL(_gate)         ((_gate).type == GT_64BIT_CALL)


static INLINE VA
CallGate_GetOffset(const Gate *cg)
{
   return ((unsigned)cg->offset_hi << 16) | cg->offset_lo;
}

static INLINE void
CallGate_SetOffset(Gate *cg, VA32 offset)
{
   cg->offset_lo = GATE_OFFSET_LO(offset);
   cg->offset_hi = GATE_OFFSET_HI(offset);
   ASSERT(CallGate_GetOffset(cg) == offset);
}

/*
 * Long mode interrupt/trap Gates.
 */

typedef struct Gate64 {
   unsigned   offset_0_15  : 16;
   unsigned   segment      : 16;
   unsigned   ist          : 3;
   unsigned   reserved0    : 5;
   unsigned   type         : 5;
   unsigned   DPL          : 2;
   unsigned   present      : 1;
   unsigned   offset_16_31 : 16;
   unsigned   offset_32_63 : 32;
   unsigned   reserved1    : 32;
} Gate64;

#define GATE64_OFFSET_HI(_gate)  ((uint64)(_gate).offset_32_63 << 32)
#define GATE64_OFFSET(_gate)     ((uint64)(_gate).offset_32_63 << 32 | \
                                  (uint64)(_gate).offset_16_31 << 16 | \
                                  (uint64)(_gate).offset_0_15)

/*
 * Descriptor Table Registers.
 */


/* 
 * Need to pack the DTR struct so the offset starts right after the
 * limit.  
 */
#pragma pack(push, 1)
typedef struct DTR32 {
   uint16 limit;
   uint32 offset;
} DTR32;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct DTR64 {
   uint16 limit;
   uint64 offset;
} DTR64;
#pragma pack(pop)

#if defined VMM || defined FROBOS_LONGMODE || defined VM_X86_64
typedef DTR64 DTR;
#else
typedef DTR32 DTR;
#endif

typedef union DTRWords32 {
   DTR32 dtr;
   uint32 word[2];
} DTRWords32;

typedef union DTRWords64 {
   DTR64 dtr;
   uint64 word[2];
} DTRWords64;

#endif //_X86DESC_H_
