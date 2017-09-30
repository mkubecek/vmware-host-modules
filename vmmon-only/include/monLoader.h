/*********************************************************
 * Copyright (C) 2015-2017 VMware, Inc. All rights reserved.
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
 * monLoader.h --
 *
 *      Describes the monitor loader, its header and support code for
 *      processing this header.
 *
 *
 * Overview
 * ========
 *
 * Before the monitor or its in-context bootstrap code can run, a monitor
 * address space must be created and partially populated.  This address space
 * contains code and data statically built, empty but allocated space, shared
 * and run-time initialized content.  The monitor loader header regularizes
 * encoding of address space information, allowing a common representation and
 * common code to be re-used for different contexts (vmmon for hosted, vmkernel
 * for ESX, and the VMX while deprivileging of bootstrap is ongoing).
 *
 * The Header
 * ==========
 *
 * The monitor loader header contains a sequence of entries describing content.
 * Each entry has a start and end VPN, an optional content source, permission
 * flags to be applied when mapping, an optional subIndex specifying a shared
 * region (for shared content types) and a flag specifying whether to process
 * this entry for all VCPUs or just the bootstrap processor.
 *
 * Content types
 * =============
 *
 * ADDRSPACE: Must be first.  Describes the address space in which all other
 * entries reside.  Page tables from L4 to L1 will be allocated, in order,
 * eagerly.  These page tables will be wired with the permissions of this entry
 * from L4 to L1.  This allows easy access to the L1E for any VPN in the space.
 *
 * ML_CONTENT_PAGETABLE_Lx: Maps the page tables for a given level.  It must be
 * exactly the size of the tables preallocated by ADDRSPACE for the level.
 *
 * ML_CONTENT_ALLOCZERO: Allocates new MPNs, zeroes the pages and maps them.
 *
 * ML_CONTENT_COPY: Allocates new MPNs, maps them and copies from the specified
 * source.
 *
 * ML_CONTENT_SHARE: Memory provided by user or host kernel and mapped into the
 * monitor address space.  The subIndex field specifies which region, as
 * multiple regions may be shared for each source.
 *
 * Processing
 * ==========
 *
 * The monitor loader can be built in different contexts.  Each context must
 * provide implementations of callout functions.  To fully build the context,
 * callouts for memory allocation, access and reporting of resources will be
 * added, as will accounting functionality.
 *
 * Callouts
 * ========
 * 
 * Memory-accessing callouts take a Vcpuid as memory is tracked per-VCPU.
 *
 * MonLoaderCallout_AllocMPN(MPN): Allocate a new MPN.
 * MonLoaderCallout_CleanUp(): Release temporary MonLoader callout resources.
 * MonLoaderCallout_CopyFromBlob(blobOffset, size, MPN): Copy blob contents.
 * MonLoaderCallout_FillPage(pattern, mpn): fill a page with a pattern.
 * MonLoaderCallout_GetPTE(MPN, index, *pte): Get indexth PTE in a PT MPN.
 * MonLoaderCallout_GetPageRoot(vcpu): Get a VCPU's page root.
 * MonLoaderCallout_ImportPage(MPN): Import an MPN for tracking and future use.
 * MonLoaderCallout_Init(): Initialize MonLoader callouts.
 * MonLoaderCallout_GetSharedHostPage(subIdx, page): Get shared host page MPN.
 * MonLoaderCallout_GetSharedUserPage(subIdx, page): Get shared user page MPN.
 * MonLoaderCallout_SetEntrypoint(): Sets the entry code/stack for execution.
 *
 * Building vs Importing
 * =====================
 *
 * While legacy VMX is still responsible for building part of the monitor
 * context, the monitor loader must cooperate.  Rather than building the
 * context itself, the monitor loader verifies its environment matches its
 * header's expectations.  This includes verifying sufficient allocations,
 * permissions of page table wiring and page table self-mapping.
 *
 * Errors
 * ======
 *
 * To avoid ASSERTs across contexts, errors are returned and clean-up is done
 * carefully if processing fails at any stage.  Some errors are debug-only and
 * essentially verify invariants for debug builds.  The line in the table at
 * which the error was encountered is returned for logging and debugging.
 *
 * Compatibility
 * =============
 *
 * bootstrap-offsets.pl relies on MonLoaderContentType, MonLoaderSourceType and
 * the structure of MonLoaderEntry to determine and set blob offsets for
 * ML_CONTENT_COPY + ML_SOURCE_BLOB.  If these are changed, the script should
 * be updated accordingly.
 */

#ifndef _MON_LOADER
#define _MON_LOADER

#include "vm_basic_types.h"
#include "x86paging_common.h"
#include "x86types.h" /* PT_L1E */
#include "vcpuid.h"   /* Vcpuid */

#define ML_NAME_MAX 16

/* ML perms are simple and abbreviated. */

#define ML_PERM_RWX (PTE_P | PTE_RW)
#define ML_PERM_RW  (PTE_P | PTE_RW | PTE_NX)
#define ML_PERM_RO  (PTE_P |          PTE_NX)
#define ML_PERM_RX   PTE_P

#define ML_PERM_MASK (PTE_P| PTE_RW | PTE_NX | PTE_US)

#define ML_PERMS_MATCH(x,p) (((x) & ML_PERM_MASK) == ((p) & ML_PERM_MASK))

#define LOADER_HEADER_MAGIC 0x8675309E98675309

typedef enum {
   ML_CONTENT_INVALID = 0,
   ML_CONTENT_ADDRSPACE,     /* The static address space. */
   ML_CONTENT_ALLOCZERO,     /* Allocate, zero-fill and map. */
   ML_CONTENT_COPY,          /* Copy data from external source. */
   ML_CONTENT_PAGETABLE_L4,  /* Mappings for level 4 page tables in AS. */
   ML_CONTENT_PAGETABLE_L3,  /* Mappings for level 3 page tables in AS. */
   ML_CONTENT_PAGETABLE_L2,  /* Mappings for level 2 page tables in AS. */
   ML_CONTENT_PAGETABLE_L1,  /* Mappings for level 1 page tables in AS. */
   ML_CONTENT_SHARE,         /* Share data from external source. */
} MonLoaderContentType;

#define CONTENT_TO_PTLEVEL(x) (x == ML_CONTENT_PAGETABLE_L4 ? 4 : \
                               x == ML_CONTENT_PAGETABLE_L3 ? 3 : \
                               x == ML_CONTENT_PAGETABLE_L2 ? 2 : \
                               x == ML_CONTENT_PAGETABLE_L1 ? 1 : \
                               0)

/* Sources of content, for pages not zeroed, unmapped or otherwise special. */
typedef enum MonLoaderSourceType {
   ML_SOURCE_INVALID  = 0x0,
   ML_SOURCE_NONE,
   ML_SOURCE_BLOB,
   ML_SOURCE_USER,
   ML_SOURCE_HOST,
} MonLoaderSourceType;

/*
 * NOTE: When modifying MonLoaderEntry, corresponding changes must be
 * made to vmcore/make/misc/bootstrap-offsets.pl.
 */
typedef struct {
   MonLoaderContentType content;           /* Content type. */
   MonLoaderSourceType  source;            /* Source, if any, for content. */
   VPN                  monVPN;            /* Destination in monitor AS. */
   uint64               monPages;          /* Size in pages. */
   uint64               flags;             /* PTE permissions */
   unsigned             allocs;            /* MPNs allocated (all VCPUs). */
   /* Set for source ML_SOURCE_BLOB: */
   struct blobSrc {
      uint64 offset; /* offset within the blob, in bytes. */
      uint64 size;   /* size of content, in bytes. */
   } blobSrc;
   uint64               bspOnly;           /* Process only on BSP. */
   uint64               subIndex;          /* Region ID for ML_CONTENT_COPY. */
   uint64               procVmmon;         /* Processed by the vmmon on hosted. */
} MonLoaderEntry;


/* Packed for easy consumption by bootstrap-offsets.pl. */
#pragma pack(push, 1)
typedef struct {
   uint64         magic;
   uint32         entrySize;
   uint32         count;
   /* cs:rip */
   uint16         codeSelector;
   VA64           codeEntrypoint;
   /* ss:rip */
   uint16         stackSelector;
   VA64           stackEntrypoint;
   MonLoaderEntry entries[];
} MonLoaderHeader;
#pragma pack(pop)

/* Environment context structure, defined by the environment. */
struct MonLoaderEnvContext;
typedef struct MonLoaderEnvContext MonLoaderEnvContext;

/* Callout prototypes */
MPN  MonLoaderCallout_AllocMPN(MonLoaderEnvContext *, Vcpuid);
void MonLoaderCallout_CleanUp(MonLoaderEnvContext *);
Bool MonLoaderCallout_CopyFromBlob(MonLoaderEnvContext *, uint64, size_t, MPN,
                                   Vcpuid);
Bool MonLoaderCallout_FillPage(MonLoaderEnvContext *, uint8, MPN, Vcpuid);
MPN  MonLoaderCallout_GetPageRoot(MonLoaderEnvContext *, Vcpuid);
Bool MonLoaderCallout_GetPTE(MonLoaderEnvContext *, MPN, unsigned, Vcpuid,
                             PT_L1E *);
Bool MonLoaderCallout_ImportPage(MonLoaderEnvContext *, MPN, Vcpuid);
Bool MonLoaderCallout_Init(void *, MonLoaderEnvContext **);
Bool MonLoaderCallout_MapMPNInPTE(MonLoaderEnvContext *, MPN, unsigned, uint64,
                                  MPN, Vcpuid);
MPN  MonLoaderCallout_GetSharedUserPage(MonLoaderEnvContext *, uint64, unsigned,
                                        Vcpuid);
MPN  MonLoaderCallout_GetSharedHostPage(MonLoaderEnvContext *, uint64, unsigned,
                                        Vcpuid);
Bool MonLoaderCallout_IsPrivileged(MonLoaderEnvContext *);
Bool MonLoaderCallout_SetEntrypoint(MonLoaderEnvContext *, uint16, VA64, uint16,
                                    VA64);
MPN  MonLoaderCallout_GetBlobMpn(MonLoaderEnvContext *, uint64);

typedef enum MonLoaderError {
   ML_OK = 0,
   ML_ERROR_ADDRSPACE_TOO_LARGE,
   ML_ERROR_ALLOC,
   ML_ERROR_ALREADY_MAPPED,
   ML_ERROR_ARGS,
   ML_ERROR_CALLOUT_INIT,
   ML_ERROR_CALLOUT_COPY,
   ML_ERROR_CALLOUT_ENTRYPOINT,
   ML_ERROR_CALLOUT_GETPTE,
   ML_ERROR_CALLOUT_MAPINPTE,
   ML_ERROR_CALLOUT_PAGEROOT_GET,
   ML_ERROR_CALLOUT_ZERO,
   ML_ERROR_CONTENT_INVALID,
   ML_ERROR_CONTEXT_INIT,
   ML_ERROR_DUPLICATE,
   ML_ERROR_INVALID_VPN,
   ML_ERROR_MAGIC,
   ML_ERROR_MAP,
   ML_ERROR_NO_ADDRSPACE,
   ML_ERROR_PAGE_TABLE_IMPORT,
   ML_ERROR_PAGE_TABLE_MAP_SIZE,
   ML_ERROR_PAGE_TABLE_VERIFY,
   ML_ERROR_SHARE,
   ML_ERROR_SIZE,
   ML_ERROR_SOURCE_INVALID,
   ML_ERROR_TABLE_MISSING,
} MonLoaderError;


/* A subindex above shared area subindices for sharing of MonLoaderHeader. */
#define MONLOADER_HEADER_IDX (NUM_SHARED_AREAS + 1)


MonLoaderError MonLoader_Process(MonLoaderHeader *header, unsigned numVCPUs,
                                 void *args, unsigned *line, Vcpuid *vcpu);


/*
 *----------------------------------------------------------------------
 *
 * MonLoader_GetFixedHeaderSize --
 *
 *      Returns the size of the fixed portion of MonLoaderHeader.
 *
 *----------------------------------------------------------------------
 */
static INLINE size_t
MonLoader_GetFixedHeaderSize(void)
{
   return sizeof(MonLoaderHeader);
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoader_GetFullHeaderSize --
 *
 *      Returns the size of the full MonLoaderHeader, including the
 *      variable size portion.
 *
 *----------------------------------------------------------------------
 */
static INLINE size_t
MonLoader_GetFullHeaderSize(MonLoaderHeader *header)
{
   return MonLoader_GetFixedHeaderSize() +
          header->count * sizeof(MonLoaderEntry);
}

#endif /* !_MON_LOADER */
