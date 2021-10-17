/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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
 * addrlayout_table.h --
 *
 * Named elements in the monitor's address space of fixed size and location.
 * All units are 4 kilobyte pages.  Also used for vmmlayout output.
 *
 * The monitor's address space is populated three different ways:
 * 1) For elements of a fixed size and location, at compile-time.
 * 2) For well-known program sections and the shared area, at link-time.
 * 3) Dynamically, by the allocator, during bootstrapping and monitor power-on.
 *
 * This file describes all of the monitor's main 64MB address space.  All of
 * (1) is described specifically.  Ranges for (2) and (3) are accounted for,
 * but their contents are described generally here.
 *
 * The monitor's address space is organized into container regions which are an
 * integer number of 2MB "large" pages.  Within a container region, items
 * comprised of one or more 4KB pages may be described.
 *
 * All container regions and items are described by a name and a size in pages.
 * No two items may have the same name nor may two regions.
 *
 * REGION(region-name, number-of-pages)
 *   ITEM(item-name,   number-of-pages)     // first item in region
 *   ITEM(item-name,   number-of-pages)     // second item in region
 * <...>
 */


/*
 * Monitor .rodata/.text.  Mapped large (reducing TLB pressure) and read-only.
 */
REGION(MONITOR_READONLY,        1024)
  ITEM(MONITOR_READONLY_LINKER, 1024) // Used by linker.

/*
 * Monitor .data/.bss.  Mapped large (reducing TLB pressure) and read-write.
 */
REGION(MONITOR_DATA,            512)
  ITEM(MON_STACK_PAGES,           8) // Monitor stack.
  ITEM(MON_IDT,                   1) // BS/normal IDT (used for all but SVM).
  ITEM(MONITOR_DATA,            503) // Used by linker and TC.

/*
 * The monitor's translation cache.  This object starts at the page boundary
 * following the end of the used space in MONITOR_DATA and extends through
 * the TC_REGION.
 */
REGION(TC_REGION,              1024)
  ITEM(TC_BLOCK,               1024)

/*
 * The monitor's pages for architectural state, stacks, page tables, shared
 * pages, and dynamic allocation.  This region also contains the linked shared
 * area and other contents.
 * Mapped small (allowing non-present mappings and sharing) and read-write.
 */
REGION(MONITOR_MISC,           5632)
#ifdef VMX86_SERVER
  ITEM(GUARD_PAGE,                2) // Reserved (for symmetry with hosted).
#else
  ITEM(CROSS_PAGE_DATA,           1) // Cross page data (RW).
  ITEM(CROSS_PAGE_CODE,           1) // Cross page code (RX).
#endif
  ITEM(GDT_AND_TASK,              1) // GDT and Task State Segment.

  ITEM(MON_PAGE_TABLE_L5,         1) // Monitor page root, if 5-Level PT used.
  ITEM(MON_PAGE_TABLE_L4,         1) // Monitor page root, if 4-Level PT used.
  ITEM(MON_PAGE_TABLE_L3,         1) // Monitor L3 page table.
  ITEM(MON_PAGE_TABLE_L2,         1) // Monitor L2 page table.
  ITEM(MON_PAGE_TABLE_L1,        32) // Monitor L1 page tables.

  ITEM(HOST_APIC,                 1) // Physical APIC.
  ITEM(GUEST_APIC,                1) // Guest APIC.

  ITEM(DF_GUARD_PAGE,             1) // Double Fault stack guard page.
  ITEM(DF_STACK_PAGES,            1) // Double Fault stack; need ~1600 bytes.

  ITEM(MC_GUARD_PAGE,             1) // Machine Check stack guard page.
  ITEM(MC_STACK_PAGES,            1) // Machine Check stack.

  ITEM(NMI_GUARD_PAGE,            1) // NMI stack guard page.
  ITEM(NMI_STACK_PAGES,           4) // NMI stack.  Profiling requires a larger
                                     // stack than MC/DF.

  ITEM(HV_SWITCH,                 1) // SVM switch page
  ITEM(HV_CURRENT_VMCB,           1) // SVM current VMCB
  ITEM(HV_STD_NATIVE_VMCB,        1) // SVM/VT standard VMCB/VMCS
  ITEM(HV_AUX_NATIVE_VMCB,        1) // SVM/VT auxiliary VMCB/VMCS
  ITEM(VHV_GUEST_VMCB,            1) // VSVM/VVT guest VMCB/VMCS
  ITEM(HV_SEV_VMSA,               1) // SVM SEV-ES state save area
  ITEM(VPROBE_MON_RELOC,          1) // VProbe monitor reloc page.
  ITEM(GART_LIST_PAGES,          48) // PhysMem vmm gart list pages.
  ITEM(GART_ALT_LIST_PAGES,      48) // Alternate vmm gart pages
                                     // (for invalidation).
  ITEM(GART_BF_PAGES,             8) // Gart Bloom filter memory pages.
  ITEM(VVT_GUEST_VIRT_APIC,       1) // Inner guest virtual APIC page.
  ITEM(HT_STATE_MAP,              8) // Information used by the SecureHT module
  ITEM(SHARED_RW_DATA,         5462) // R/W shared data, including the shared
                                     // area and stat vars. Any remaining pages
                                     // are left to the dynamic allocator, which
                                     // extends throughout the entire reclaimed
                                     // bootstrap region.
/*
 * Bootstrap-used space.  This space is reclaimed when bootstrap is done, and
 * is used by the allocator thereafter.  The allocator starts at the first
 * unused page in the SHARED_RW_DATA item from the MONITOR_MISC region and
 * extends through to the end of the VMM address space once the bootstrap region
 * is reclaimed.
 */
REGION(BOOTSTRAP,              8192)
  ITEM(BS_TXT,                   17) // bootstrap's .text
  ITEM(BS_RODATA,                10) // bootstrap's .rodata
  ITEM(BS_DATA,                  10) // bootstrap's .data
  ITEM(BS_BSS,                   10) // bootstrap's .bss
  ITEM(BS_HEADER,                 1) // MonLoaderHeader mapping
  ITEM(BS_VCPU_L1PT_RANGE,       32) // VCPU L1PT mappings
  ITEM(BS_VCPU_L1PT_PT,           1) // L1PT that maps a VCPU's L1 page tables
  ITEM(BS_INIT_POOL,              1) // pool for initializing VMM pages
  ITEM(BS_DYNAMIC_ALLOC,       1880) // bs_alloc's dynamic allocator pages
  ITEM(MON_IDT_TMP,               1) // temporary addr for normal IDT on bsp
  ITEM(IDT_BOOTSTRAP_STUBS,       2) // bootstrap IDT gate stubs
  ITEM(BS_PER_VM_VMX,           300) // vmm64's shared_per_vm_vmx
  ITEM(BS_PER_VCPU,               8) // vmm64's shared_per_vcpu for VCPU 0
  ITEM(BS_PER_VCPU_VMX,         136) // vmm64's shared_per_vcpu_vmx for VCPU 0
  ITEM(VMM_MODULES,            5783) // ~22.6MB for unlinked VMM modules
