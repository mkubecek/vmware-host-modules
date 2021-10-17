/*********************************************************
 * Copyright (C) 2015-2021 VMware, Inc. All rights reserved.
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
 * x86vt-vmcs-fields.h --
 *
 * VMCS fields (SDM volume 3 Appendix B).
 *
 */

#include "community_source.h"

/*
 * There are 16 groups of VMCS fields: 4 sizes crossed with 4 types.
 * The 4 sizes are 16-bit, 64-bit, 32-bit, and "natural", in that order,
 * per the enumeration of the size in the field encoding.
 * The 4 types are control, read-only data, guest-state, and host-state,
 * in that order, per the enumeration of the type in the field encoding.
 */

/*
 * Use of this table:
 * A typical consumer will define a VMCS_FIELD macro to extract and
 * collate the desired information for each VMCS field, and then it will
 * include this file to invoke that macro with the property list for each
 * VMCS field.  Some properties are represented mnemonically (e.g. _S16 for
 * "16-bit size," so that the extracted property can be defined by another
 * macro.  For example, some consumers may choose to interpret the size
 * property "_S16" as "uint16", while others may wish to interpret it as "s16."
 * For convenience, each of the 16 groups is preceded by a VMCS_GROUP_START
 * macro and followed by a VMCS_GROUP_END macro.  Similarly, each set of four
 * groups with the same size is preceded by a VMCS_SET_START macro and followed
 * by a VMCS_SET_END macro.  This file provides empty definitions of
 * VMCS_GROUP_START, VMCS_GROUP_END, VMCS_SET_START, and VMCS_SET_END if the
 * consumer has not defined them.
 */

#ifndef VMCS_GROUP_START
#define VMCS_GROUP_START(size, type)
#define LOCAL_GROUP_START
#endif

#ifndef VMCS_GROUP_END
#define VMCS_GROUP_END(size, type)
#define LOCAL_GROUP_END
#endif

#ifndef VMCS_SET_START
#define VMCS_SET_START(size)
#define LOCAL_SET_START
#endif

#ifndef VMCS_SET_END
#define VMCS_SET_END(size)
#define LOCAL_SET_END
#endif

#ifndef VMCS_UNUSED
#define VMCS_UNUSED(val, size, type, index)
#define LOCAL_UNUSED
#endif

/*
 * VMCS_FIELD(_name, _val, _size, _type, _index, _cache, _vvt, _access,
 *            _sticky)
 *    _type is one of:
 *       _TC -- control
 *       _TG -- guest-state
 *       _TH -- host-state
 *       _TD -- read-only data
 *    _size is one of:
 *       _S16 -- 16-bits
 *       _S32 -- 32-bits
 *       _S64 -- 64-bits
 *       _SN  -- natural width
 *    _cache is one of:
 *       _C  -- cached
 *       _NC -- not cached
 *    _vvt is one of:
 *       _V  -- virtualized
 *       _NV -- not virtualized
 *   _access is one of:
 *       _NA  -- no access
 *       _RW  -- read/write access
 *       _RO  -- read-only access
 *       _URW -- read/write acess for the ULM only
 *   _sticky is one of:
 *       _S  -- sticky
 *       _NS -- non-sticky
 *
 * Fields marked sticky in the current VMCS, change only via VMWRITEs.
 */


VMCS_SET_START(16)
/* 16-bit control fields. */
VMCS_GROUP_START(16, CTL)
VMCS_FIELD(VPID,                    0x0000, _S16, _TC,  0,  _C,  _V, _URW,  _S)
VMCS_FIELD(PI_NOTIFY,               0x0002, _S16, _TC,  1, _NC, _NV,  _NA,  _S)
VMCS_FIELD(EPTP_INDEX,              0x0004, _S16, _TC,  2, _NC,  _V, _URW, _NS)
VMCS_UNUSED(                        0x0006, _S16, _TC,  3)
VMCS_UNUSED(                        0x0008, _S16, _TC,  4)
VMCS_UNUSED(                        0x000A, _S16, _TC,  5)
VMCS_UNUSED(                        0x000C, _S16, _TC,  6)
VMCS_UNUSED(                        0x000E, _S16, _TC,  7)
VMCS_UNUSED(                        0x0010, _S16, _TC,  8)
VMCS_UNUSED(                        0x0012, _S16, _TC,  9)
VMCS_UNUSED(                        0x0014, _S16, _TC,  10)
VMCS_UNUSED(                        0x0016, _S16, _TC,  11)
VMCS_UNUSED(                        0x0018, _S16, _TC,  12)
VMCS_UNUSED(                        0x001A, _S16, _TC,  13)
VMCS_UNUSED(                        0x001C, _S16, _TC,  14)
VMCS_UNUSED(                        0x001E, _S16, _TC,  15)
VMCS_UNUSED(                        0x0020, _S16, _TC,  16)
VMCS_UNUSED(                        0x0022, _S16, _TC,  17)
VMCS_UNUSED(                        0x0024, _S16, _TC,  18)
VMCS_UNUSED(                        0x0026, _S16, _TC,  19)
VMCS_UNUSED(                        0x0028, _S16, _TC,  20)
VMCS_UNUSED(                        0x002A, _S16, _TC,  21)
VMCS_UNUSED(                        0x002C, _S16, _TC,  22)
VMCS_UNUSED(                        0x002E, _S16, _TC,  23)
VMCS_UNUSED(                        0x0030, _S16, _TC,  24)
VMCS_UNUSED(                        0x0032, _S16, _TC,  25)
VMCS_UNUSED(                        0x0034, _S16, _TC,  26)
VMCS_UNUSED(                        0x0036, _S16, _TC,  27)
VMCS_UNUSED(                        0x0038, _S16, _TC,  28)
VMCS_UNUSED(                        0x003A, _S16, _TC,  29)
VMCS_UNUSED(                        0x003C, _S16, _TC,  30)
VMCS_UNUSED(                        0x003E, _S16, _TC,  31)
VMCS_GROUP_END(16, CTL)

/* 16-bit read-only data fields. */
VMCS_GROUP_START(16, DATA)
VMCS_UNUSED(                        0x0400, _S16, _TD,  0)
VMCS_UNUSED(                        0x0402, _S16, _TD,  1)
VMCS_UNUSED(                        0x0404, _S16, _TD,  2)
VMCS_UNUSED(                        0x0406, _S16, _TD,  3)
VMCS_UNUSED(                        0x0408, _S16, _TD,  4)
VMCS_UNUSED(                        0x040A, _S16, _TD,  5)
VMCS_UNUSED(                        0x040C, _S16, _TD,  6)
VMCS_UNUSED(                        0x040E, _S16, _TD,  7)
VMCS_UNUSED(                        0x0410, _S16, _TD,  8)
VMCS_UNUSED(                        0x0412, _S16, _TD,  9)
VMCS_UNUSED(                        0x0414, _S16, _TD,  10)
VMCS_UNUSED(                        0x0416, _S16, _TD,  11)
VMCS_UNUSED(                        0x0418, _S16, _TD,  12)
VMCS_UNUSED(                        0x041A, _S16, _TD,  13)
VMCS_UNUSED(                        0x041C, _S16, _TD,  14)
VMCS_UNUSED(                        0x041E, _S16, _TD,  15)
VMCS_UNUSED(                        0x0420, _S16, _TD,  16)
VMCS_UNUSED(                        0x0422, _S16, _TD,  17)
VMCS_UNUSED(                        0x0424, _S16, _TD,  18)
VMCS_UNUSED(                        0x0426, _S16, _TD,  19)
VMCS_UNUSED(                        0x0428, _S16, _TD,  20)
VMCS_UNUSED(                        0x042A, _S16, _TD,  21)
VMCS_UNUSED(                        0x042C, _S16, _TD,  22)
VMCS_UNUSED(                        0x042E, _S16, _TD,  23)
VMCS_UNUSED(                        0x0430, _S16, _TD,  24)
VMCS_UNUSED(                        0x0432, _S16, _TD,  25)
VMCS_UNUSED(                        0x0434, _S16, _TD,  26)
VMCS_UNUSED(                        0x0436, _S16, _TD,  27)
VMCS_UNUSED(                        0x0438, _S16, _TD,  28)
VMCS_UNUSED(                        0x043A, _S16, _TD,  29)
VMCS_UNUSED(                        0x043C, _S16, _TD,  30)
VMCS_UNUSED(                        0x043E, _S16, _TD,  31)
VMCS_GROUP_END(16, DATA)

/* 16-bit guest state. */
VMCS_GROUP_START(16, GUEST)
VMCS_FIELD(ES,                      0x0800, _S16, _TG,  0, _NC,  _V, _RW, _NS)
VMCS_FIELD(CS,                      0x0802, _S16, _TG,  1, _NC,  _V, _RW, _NS)
VMCS_FIELD(SS,                      0x0804, _S16, _TG,  2, _NC,  _V, _RW, _NS)
VMCS_FIELD(DS,                      0x0806, _S16, _TG,  3, _NC,  _V, _RW, _NS)
VMCS_FIELD(FS,                      0x0808, _S16, _TG,  4, _NC,  _V, _RW, _NS)
VMCS_FIELD(GS,                      0x080A, _S16, _TG,  5, _NC,  _V, _RW, _NS)
VMCS_FIELD(LDTR,                    0x080C, _S16, _TG,  6, _NC,  _V, _RW, _NS)
VMCS_FIELD(TR,                      0x080E, _S16, _TG,  7, _NC,  _V, _RW, _NS)
VMCS_FIELD(INTR_STATUS,             0x0810, _S16, _TG,  8, _NC, _NV, _NA, _NS)
VMCS_FIELD(PML_INDEX,               0x0812, _S16, _TG,  9, _NC,  _V, _NA, _NS)
VMCS_UNUSED(                        0x0814, _S16, _TG,  10)
VMCS_UNUSED(                        0x0816, _S16, _TG,  11)
VMCS_UNUSED(                        0x0818, _S16, _TG,  12)
VMCS_UNUSED(                        0x081A, _S16, _TG,  13)
VMCS_UNUSED(                        0x081C, _S16, _TG,  14)
VMCS_UNUSED(                        0x081E, _S16, _TG,  15)
VMCS_UNUSED(                        0x0820, _S16, _TG,  16)
VMCS_UNUSED(                        0x0822, _S16, _TG,  17)
VMCS_UNUSED(                        0x0824, _S16, _TG,  18)
VMCS_UNUSED(                        0x0826, _S16, _TG,  19)
VMCS_UNUSED(                        0x0828, _S16, _TG,  20)
VMCS_UNUSED(                        0x082A, _S16, _TG,  21)
VMCS_UNUSED(                        0x082C, _S16, _TG,  22)
VMCS_UNUSED(                        0x082E, _S16, _TG,  23)
VMCS_UNUSED(                        0x0830, _S16, _TG,  24)
VMCS_UNUSED(                        0x0832, _S16, _TG,  25)
VMCS_UNUSED(                        0x0834, _S16, _TG,  26)
VMCS_UNUSED(                        0x0836, _S16, _TG,  27)
VMCS_UNUSED(                        0x0838, _S16, _TG,  28)
VMCS_UNUSED(                        0x083A, _S16, _TG,  29)
VMCS_UNUSED(                        0x083C, _S16, _TG,  30)
VMCS_UNUSED(                        0x083E, _S16, _TG,  31)
VMCS_GROUP_END(16, GUEST)

/* 16-bit host state. */
VMCS_GROUP_START(16, HOST)
VMCS_FIELD(HOST_ES,                 0x0C00, _S16, _TH,  0, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_CS,                 0x0C02, _S16, _TH,  1, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_SS,                 0x0C04, _S16, _TH,  2, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_DS,                 0x0C06, _S16, _TH,  3, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_FS,                 0x0C08, _S16, _TH,  4, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_GS,                 0x0C0A, _S16, _TH,  5, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_TR,                 0x0C0C, _S16, _TH,  6, _NC,  _V, _NA,  _S)
VMCS_UNUSED(                        0x0C0E, _S16, _TH,  7)
VMCS_UNUSED(                        0x0C10, _S16, _TH,  8)
VMCS_UNUSED(                        0x0C12, _S16, _TH,  9)
VMCS_UNUSED(                        0x0C14, _S16, _TH,  10)
VMCS_UNUSED(                        0x0C16, _S16, _TH,  11)
VMCS_UNUSED(                        0x0C18, _S16, _TH,  12)
VMCS_UNUSED(                        0x0C1A, _S16, _TH,  13)
VMCS_UNUSED(                        0x0C1C, _S16, _TH,  14)
VMCS_UNUSED(                        0x0C1E, _S16, _TH,  15)
VMCS_UNUSED(                        0x0C20, _S16, _TH,  16)
VMCS_UNUSED(                        0x0C22, _S16, _TH,  17)
VMCS_UNUSED(                        0x0C24, _S16, _TH,  18)
VMCS_UNUSED(                        0x0C26, _S16, _TH,  19)
VMCS_UNUSED(                        0x0C28, _S16, _TH,  20)
VMCS_UNUSED(                        0x0C2A, _S16, _TH,  21)
VMCS_UNUSED(                        0x0C2C, _S16, _TH,  22)
VMCS_UNUSED(                        0x0C2E, _S16, _TH,  23)
VMCS_UNUSED(                        0x0C30, _S16, _TH,  24)
VMCS_UNUSED(                        0x0C32, _S16, _TH,  25)
VMCS_UNUSED(                        0x0C34, _S16, _TH,  26)
VMCS_UNUSED(                        0x0C36, _S16, _TH,  27)
VMCS_UNUSED(                        0x0C38, _S16, _TH,  28)
VMCS_UNUSED(                        0x0C3A, _S16, _TH,  29)
VMCS_UNUSED(                        0x0C3C, _S16, _TH,  30)
VMCS_UNUSED(                        0x0C3E, _S16, _TH,  31)
VMCS_GROUP_END(16, HOST)
VMCS_SET_END(16)


VMCS_SET_START(64)
/* 64-bit control fields. */
VMCS_GROUP_START(64, CTL)
VMCS_FIELD(IOBITMAPA,               0x2000, _S64, _TC,  0, _NC,  _V, _NA,  _S)
VMCS_FIELD(IOBITMAPB,               0x2002, _S64, _TC,  1, _NC,  _V, _NA,  _S)
VMCS_FIELD(MSRBITMAP,               0x2004, _S64, _TC,  2, _NC,  _V, _NA,  _S)
VMCS_FIELD(VMEXIT_MSR_STORE_ADDR,   0x2006, _S64, _TC,  3, _NC,  _V, _NA,  _S)
VMCS_FIELD(VMEXIT_MSR_LOAD_ADDR,    0x2008, _S64, _TC,  4, _NC,  _V, _NA,  _S)
VMCS_FIELD(VMENTRY_MSR_LOAD_ADDR,   0x200A, _S64, _TC,  5, _NC,  _V, _NA,  _S)
VMCS_FIELD(EXECUTIVE_VMCS_PTR,      0x200C, _S64, _TC,  6, _NC,  _V, _NA, _NS)
VMCS_FIELD(PML_ADDR,                0x200E, _S64, _TC,  7, _NC,  _V, _NA,  _S)
VMCS_FIELD(TSC_OFF,                 0x2010, _S64, _TC,  8,  _C,  _V, _NA,  _S)
VMCS_FIELD(VIRT_APIC_ADDR,          0x2012, _S64, _TC,  9, _NC,  _V, _NA,  _S)
VMCS_FIELD(APIC_ACCESS_ADDR,        0x2014, _S64, _TC, 10, _NC,  _V, _NA,  _S)
VMCS_FIELD(PI_DESC_ADDR,            0x2016, _S64, _TC, 11, _NC, _NV, _NA,  _S)
VMCS_FIELD(VMFUNC_CTLS,             0x2018, _S64, _TC, 12, _NC,  _V, _NA,  _S)
VMCS_FIELD(EPTP,                    0x201A, _S64, _TC, 13, _NC,  _V, _NA, _NS)
VMCS_FIELD(EOI_EXIT0,               0x201C, _S64, _TC, 14, _NC, _NV, _NA,  _S)
VMCS_FIELD(EOI_EXIT1,               0x201E, _S64, _TC, 15, _NC, _NV, _NA,  _S)
VMCS_FIELD(EOI_EXIT2,               0x2020, _S64, _TC, 16, _NC, _NV, _NA,  _S)
VMCS_FIELD(EOI_EXIT3,               0x2022, _S64, _TC, 17, _NC, _NV, _NA,  _S)
VMCS_FIELD(EPTP_LIST_ADDR,          0x2024, _S64, _TC, 18, _NC,  _V, _NA,  _S)
VMCS_FIELD(VMREAD_BITMAP,           0x2026, _S64, _TC, 19, _NC, _NV, _NA,  _S)
VMCS_FIELD(VMWRITE_BITMAP,          0x2028, _S64, _TC, 20, _NC, _NV, _NA,  _S)
VMCS_FIELD(VE_INFO_ADDR,            0x202A, _S64, _TC, 21, _NC,  _V, _NA,  _S)
VMCS_FIELD(XSS_EXITING_BITMAP,      0x202C, _S64, _TC, 22, _NC,  _V, _NA,  _S)
VMCS_FIELD(ENCLS_EXITING_BITMAP,    0x202E, _S64, _TC, 23, _NC,  _V, _NA,  _S)
VMCS_UNUSED(                        0x2030, _S64, _TC, 24)
VMCS_FIELD(TSC_MULTIPLIER,          0x2032, _S64, _TC, 25, _NC, _NV, _NA,  _S)
VMCS_UNUSED(                        0x2034, _S64, _TC, 26)
VMCS_FIELD(ENCLV_EXITING_BITMAP,    0x2036, _S64, _TC, 27, _NC,  _V, _NA,  _S)
VMCS_UNUSED(                        0x2038, _S64, _TC, 28)
VMCS_UNUSED(                        0x203A, _S64, _TC, 29)
VMCS_UNUSED(                        0x203C, _S64, _TC, 30)
VMCS_UNUSED(                        0x203E, _S64, _TC, 31)
VMCS_GROUP_END(64, CTL)

/* 64-bit read-only data field. */
VMCS_GROUP_START(64, DATA)
VMCS_FIELD(PHYSADDR,                0x2400, _S64, _TD,  0, _NC,  _V, _RO, _NS)
VMCS_UNUSED(                        0x2402, _S64, _TD,  1)
VMCS_UNUSED(                        0x2404, _S64, _TD,  2)
VMCS_UNUSED(                        0x2406, _S64, _TD,  3)
VMCS_UNUSED(                        0x2408, _S64, _TD,  4)
VMCS_UNUSED(                        0x240A, _S64, _TD,  5)
VMCS_UNUSED(                        0x240C, _S64, _TD,  6)
VMCS_UNUSED(                        0x240E, _S64, _TD,  7)
VMCS_UNUSED(                        0x2410, _S64, _TD,  8)
VMCS_UNUSED(                        0x2412, _S64, _TD,  9)
VMCS_UNUSED(                        0x2414, _S64, _TD,  10)
VMCS_UNUSED(                        0x2416, _S64, _TD,  11)
VMCS_UNUSED(                        0x2418, _S64, _TD,  12)
VMCS_UNUSED(                        0x241A, _S64, _TD,  13)
VMCS_UNUSED(                        0x241C, _S64, _TD,  14)
VMCS_UNUSED(                        0x241E, _S64, _TD,  15)
VMCS_UNUSED(                        0x2420, _S64, _TD,  16)
VMCS_UNUSED(                        0x2422, _S64, _TD,  17)
VMCS_UNUSED(                        0x2424, _S64, _TD,  18)
VMCS_UNUSED(                        0x2426, _S64, _TD,  19)
VMCS_UNUSED(                        0x2428, _S64, _TD,  20)
VMCS_UNUSED(                        0x242A, _S64, _TD,  21)
VMCS_UNUSED(                        0x242C, _S64, _TD,  22)
VMCS_UNUSED(                        0x242E, _S64, _TD,  23)
VMCS_UNUSED(                        0x2430, _S64, _TD,  24)
VMCS_UNUSED(                        0x2432, _S64, _TD,  25)
VMCS_UNUSED(                        0x2434, _S64, _TD,  26)
VMCS_UNUSED(                        0x2436, _S64, _TD,  27)
VMCS_UNUSED(                        0x2438, _S64, _TD,  28)
VMCS_UNUSED(                        0x243A, _S64, _TD,  29)
VMCS_UNUSED(                        0x243C, _S64, _TD,  30)
VMCS_UNUSED(                        0x243E, _S64, _TD,  31)
VMCS_GROUP_END(64, DATA)

/* 64-bit guest state. */
VMCS_GROUP_START(64, GUEST)
VMCS_FIELD(LINK_PTR,                0x2800, _S64, _TG,  0, _NC,  _V,  _NA, _NS)
VMCS_FIELD(DEBUGCTL,                0x2802, _S64, _TG,  1, _NC,  _V,  _NA, _NS)
VMCS_FIELD(PAT,                     0x2804, _S64, _TG,  2, _NC,  _V,  _NA, _NS)
VMCS_FIELD(EFER,                    0x2806, _S64, _TG,  3, _NC,  _V, _URW, _NS)
VMCS_FIELD(PGC,                     0x2808, _S64, _TG,  4, _NC,  _V,  _NA, _NS)
VMCS_FIELD(PDPTE0,                  0x280A, _S64, _TG,  5, _NC,  _V,  _RW, _NS)
VMCS_FIELD(PDPTE1,                  0x280C, _S64, _TG,  6, _NC,  _V,  _RW, _NS)
VMCS_FIELD(PDPTE2,                  0x280E, _S64, _TG,  7, _NC,  _V,  _RW, _NS)
VMCS_FIELD(PDPTE3,                  0x2810, _S64, _TG,  8, _NC,  _V,  _RW, _NS)
VMCS_FIELD(BNDCFGS,                 0x2812, _S64, _TG,  9, _NC,  _V,  _NA, _NS)
VMCS_UNUSED(                        0x2814, _S64, _TG,  10)
VMCS_UNUSED(                        0x2816, _S64, _TG,  11)
VMCS_FIELD(PKRS,                    0x2818, _S64, _TG,  12,_NC,  _V,  _NA, _NS)
VMCS_UNUSED(                        0x281A, _S64, _TG,  13)
VMCS_UNUSED(                        0x281C, _S64, _TG,  14)
VMCS_UNUSED(                        0x281E, _S64, _TG,  15)
VMCS_UNUSED(                        0x2820, _S64, _TG,  16)
VMCS_UNUSED(                        0x2822, _S64, _TG,  17)
VMCS_UNUSED(                        0x2824, _S64, _TG,  18)
VMCS_UNUSED(                        0x2826, _S64, _TG,  19)
VMCS_UNUSED(                        0x2828, _S64, _TG,  20)
VMCS_UNUSED(                        0x282A, _S64, _TG,  21)
VMCS_UNUSED(                        0x282C, _S64, _TG,  22)
VMCS_UNUSED(                        0x282E, _S64, _TG,  23)
VMCS_UNUSED(                        0x2830, _S64, _TG,  24)
VMCS_UNUSED(                        0x2832, _S64, _TG,  25)
VMCS_UNUSED(                        0x2834, _S64, _TG,  26)
VMCS_UNUSED(                        0x2836, _S64, _TG,  27)
VMCS_UNUSED(                        0x2838, _S64, _TG,  28)
VMCS_UNUSED(                        0x283A, _S64, _TG,  29)
VMCS_UNUSED(                        0x283C, _S64, _TG,  30)
VMCS_UNUSED(                        0x283E, _S64, _TG,  31)
VMCS_GROUP_END(64, GUEST)

/* 64-bit host state. */
VMCS_GROUP_START(64, HOST)
VMCS_FIELD(HOST_PAT,                0x2C00, _S64, _TH,  0, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_EFER,               0x2C02, _S64, _TH,  1, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_PGC,                0x2C04, _S64, _TH,  2, _NC,  _V, _NA,  _S)
VMCS_FIELD(HOST_PKRS,               0x2C06, _S64, _TH,  3, _NC,  _V, _NA,  _S)
VMCS_UNUSED(                        0x2C08, _S64, _TH,  4)
VMCS_UNUSED(                        0x2C0A, _S64, _TH,  5)
VMCS_UNUSED(                        0x2C0C, _S64, _TH,  6)
VMCS_UNUSED(                        0x2C0E, _S64, _TH,  7)
VMCS_UNUSED(                        0x2C10, _S64, _TH,  8)
VMCS_UNUSED(                        0x2C12, _S64, _TH,  9)
VMCS_UNUSED(                        0x2C14, _S64, _TH,  10)
VMCS_UNUSED(                        0x2C16, _S64, _TH,  11)
VMCS_UNUSED(                        0x2C18, _S64, _TH,  12)
VMCS_UNUSED(                        0x2C1A, _S64, _TH,  13)
VMCS_UNUSED(                        0x2C1C, _S64, _TH,  14)
VMCS_UNUSED(                        0x2C1E, _S64, _TH,  15)
VMCS_UNUSED(                        0x2C20, _S64, _TH,  16)
VMCS_UNUSED(                        0x2C22, _S64, _TH,  17)
VMCS_UNUSED(                        0x2C24, _S64, _TH,  18)
VMCS_UNUSED(                        0x2C26, _S64, _TH,  19)
VMCS_UNUSED(                        0x2C28, _S64, _TH,  20)
VMCS_UNUSED(                        0x2C2A, _S64, _TH,  21)
VMCS_UNUSED(                        0x2C2C, _S64, _TH,  22)
VMCS_UNUSED(                        0x2C2E, _S64, _TH,  23)
VMCS_UNUSED(                        0x2C30, _S64, _TH,  24)
VMCS_UNUSED(                        0x2C32, _S64, _TH,  25)
VMCS_UNUSED(                        0x2C34, _S64, _TH,  26)
VMCS_UNUSED(                        0x2C36, _S64, _TH,  27)
VMCS_UNUSED(                        0x2C38, _S64, _TH,  28)
VMCS_UNUSED(                        0x2C3A, _S64, _TH,  29)
VMCS_UNUSED(                        0x2C3C, _S64, _TH,  30)
VMCS_UNUSED(                        0x2C3E, _S64, _TH,  31)
VMCS_GROUP_END(64, HOST)
VMCS_SET_END(64)


VMCS_SET_START(32)
/* 32-bit control fields. */
VMCS_GROUP_START(32, CTL)
VMCS_FIELD(PIN_VMEXEC_CTL,          0x4000, _S32, _TC,  0,  _C,  _V, _URW,  _S)
VMCS_FIELD(CPU_VMEXEC_CTL,          0x4002, _S32, _TC,  1,  _C,  _V, _URW,  _S)
VMCS_FIELD(XCP_BITMAP,              0x4004, _S32, _TC,  2, _NC,  _V, _URW,  _S)
VMCS_FIELD(PF_ERR_MASK,             0x4006, _S32, _TC,  3, _NC,  _V,  _NA,  _S)
VMCS_FIELD(PF_ERR_MATCH,            0x4008, _S32, _TC,  4, _NC,  _V,  _NA,  _S)
VMCS_FIELD(CR3_TARG_COUNT,          0x400A, _S32, _TC,  5, _NC,  _V,  _NA,  _S)
VMCS_FIELD(VMEXIT_CTL,              0x400C, _S32, _TC,  6, _NC,  _V, _URW,  _S)
VMCS_FIELD(VMEXIT_MSR_STORE_COUNT,  0x400E, _S32, _TC,  7, _NC,  _V,  _NA,  _S)
VMCS_FIELD(VMEXIT_MSR_LOAD_COUNT,   0x4010, _S32, _TC,  8, _NC,  _V,  _NA,  _S)
VMCS_FIELD(VMENTRY_CTL,             0x4012, _S32, _TC,  9, _NC,  _V, _URW,  _S)
VMCS_FIELD(VMENTRY_MSR_LOAD_COUNT,  0x4014, _S32, _TC, 10, _NC,  _V,  _NA,  _S)
VMCS_FIELD(VMENTRY_INTR_INFO,       0x4016, _S32, _TC, 11, _NC,  _V, _URW, _NS)
VMCS_FIELD(VMENTRY_XCP_ERR,         0x4018, _S32, _TC, 12, _NC,  _V, _URW,  _S)
VMCS_FIELD(VMENTRY_INSTR_LEN,       0x401A, _S32, _TC, 13, _NC,  _V, _URW,  _S)
VMCS_FIELD(TPR_THRESHOLD,           0x401C, _S32, _TC, 14,  _C,  _V,  _NA,  _S)
VMCS_FIELD(2ND_VMEXEC_CTL,          0x401E, _S32, _TC, 15,  _C,  _V, _URW,  _S)
VMCS_FIELD(PAUSE_LOOP_GAP,          0x4020, _S32, _TC, 16, _NC,  _V,  _NA,  _S)
VMCS_FIELD(PAUSE_LOOP_WINDOW,       0x4022, _S32, _TC, 17, _NC,  _V,  _NA,  _S)
VMCS_UNUSED(                        0x4024, _S32, _TC, 18)
VMCS_UNUSED(                        0x4026, _S32, _TC, 19)
VMCS_UNUSED(                        0x4028, _S32, _TC, 20)
VMCS_UNUSED(                        0x402A, _S32, _TC, 21)
VMCS_UNUSED(                        0x402C, _S32, _TC, 22)
VMCS_UNUSED(                        0x402E, _S32, _TC, 23)
VMCS_UNUSED(                        0x4030, _S32, _TC, 24)
VMCS_UNUSED(                        0x4032, _S32, _TC, 25)
VMCS_UNUSED(                        0x4034, _S32, _TC, 26)
VMCS_UNUSED(                        0x4036, _S32, _TC, 27)
VMCS_UNUSED(                        0x4038, _S32, _TC, 28)
VMCS_UNUSED(                        0x403A, _S32, _TC, 29)
VMCS_UNUSED(                        0x403C, _S32, _TC, 30)
VMCS_UNUSED(                        0x403E, _S32, _TC, 31)
VMCS_GROUP_END(32, CTL)

/* 32-bit read-only data fields. */
VMCS_GROUP_START(32, DATA)
VMCS_FIELD(VMINSTR_ERR,             0x4400, _S32, _TD,  0, _NC,  _V, _NA, _NS)
VMCS_FIELD(EXIT_REASON,             0x4402, _S32, _TD,  1, _NC,  _V, _RO, _NS)
VMCS_FIELD(EXIT_INTR_INFO,          0x4404, _S32, _TD,  2,  _C,  _V, _RO, _NS)
VMCS_FIELD(EXIT_INTR_ERR,           0x4406, _S32, _TD,  3, _NC,  _V, _RO, _NS)
VMCS_FIELD(IDTVEC_INFO,             0x4408, _S32, _TD,  4, _NC,  _V, _RO, _NS)
VMCS_FIELD(IDTVEC_ERR,              0x440A, _S32, _TD,  5, _NC,  _V, _RO, _NS)
VMCS_FIELD(INSTRLEN,                0x440C, _S32, _TD,  6, _NC,  _V, _RO, _NS)
VMCS_FIELD(INSTR_INFO,              0x440E, _S32, _TD,  7, _NC,  _V, _RO, _NS)
VMCS_UNUSED(                        0x4410, _S32, _TD,  8)
VMCS_UNUSED(                        0x4412, _S32, _TD,  9)
VMCS_UNUSED(                        0x4414, _S32, _TD,  10)
VMCS_UNUSED(                        0x4416, _S32, _TD,  11)
VMCS_UNUSED(                        0x4418, _S32, _TD,  12)
VMCS_UNUSED(                        0x441A, _S32, _TD,  13)
VMCS_UNUSED(                        0x441C, _S32, _TD,  14)
VMCS_UNUSED(                        0x441E, _S32, _TD,  15)
VMCS_UNUSED(                        0x4420, _S32, _TD,  16)
VMCS_UNUSED(                        0x4422, _S32, _TD,  17)
VMCS_UNUSED(                        0x4424, _S32, _TD,  18)
VMCS_UNUSED(                        0x4426, _S32, _TD,  19)
VMCS_UNUSED(                        0x4428, _S32, _TD,  20)
VMCS_UNUSED(                        0x442A, _S32, _TD,  21)
VMCS_UNUSED(                        0x442C, _S32, _TD,  22)
VMCS_UNUSED(                        0x442E, _S32, _TD,  23)
VMCS_UNUSED(                        0x4430, _S32, _TD,  24)
VMCS_UNUSED(                        0x4432, _S32, _TD,  25)
VMCS_UNUSED(                        0x4434, _S32, _TD,  26)
VMCS_UNUSED(                        0x4436, _S32, _TD,  27)
VMCS_UNUSED(                        0x4438, _S32, _TD,  28)
VMCS_UNUSED(                        0x443A, _S32, _TD,  29)
VMCS_UNUSED(                        0x443C, _S32, _TD,  30)
VMCS_UNUSED(                        0x443E, _S32, _TD,  31)
VMCS_GROUP_END(32, DATA)

/* 32-bit guest state. */
VMCS_GROUP_START(32, GUEST)
VMCS_FIELD(ES_LIMIT,                0x4800, _S32, _TG,  0, _NC,  _V, _RW, _NS)
VMCS_FIELD(CS_LIMIT,                0x4802, _S32, _TG,  1, _NC,  _V, _RW, _NS)
VMCS_FIELD(SS_LIMIT,                0x4804, _S32, _TG,  2, _NC,  _V, _RW, _NS)
VMCS_FIELD(DS_LIMIT,                0x4806, _S32, _TG,  3, _NC,  _V, _RW, _NS)
VMCS_FIELD(FS_LIMIT,                0x4808, _S32, _TG,  4, _NC,  _V, _RW, _NS)
VMCS_FIELD(GS_LIMIT,                0x480A, _S32, _TG,  5, _NC,  _V, _RW, _NS)
VMCS_FIELD(LDTR_LIMIT,              0x480C, _S32, _TG,  6, _NC,  _V, _RW, _NS)
VMCS_FIELD(TR_LIMIT,                0x480E, _S32, _TG,  7, _NC,  _V, _RW, _NS)
VMCS_FIELD(GDTR_LIMIT,              0x4810, _S32, _TG,  8, _NC,  _V, _RW, _NS)
VMCS_FIELD(IDTR_LIMIT,              0x4812, _S32, _TG,  9, _NC,  _V, _RW, _NS)
VMCS_FIELD(ES_AR,                   0x4814, _S32, _TG, 10, _NC,  _V, _RW, _NS)
VMCS_FIELD(CS_AR,                   0x4816, _S32, _TG, 11, _NC,  _V, _RW, _NS)
VMCS_FIELD(SS_AR,                   0x4818, _S32, _TG, 12, _NC,  _V, _RW, _NS)
VMCS_FIELD(DS_AR,                   0x481A, _S32, _TG, 13, _NC,  _V, _RW, _NS)
VMCS_FIELD(FS_AR,                   0x481C, _S32, _TG, 14, _NC,  _V, _RW, _NS)
VMCS_FIELD(GS_AR,                   0x481E, _S32, _TG, 15, _NC,  _V, _RW, _NS)
VMCS_FIELD(LDTR_AR,                 0x4820, _S32, _TG, 16, _NC,  _V, _RW, _NS)
VMCS_FIELD(TR_AR,                   0x4822, _S32, _TG, 17, _NC,  _V, _RW, _NS)
VMCS_FIELD(HOLDOFF,                 0x4824, _S32, _TG, 18,  _C,  _V, _RW, _NS)
VMCS_FIELD(ACTSTATE,                0x4826, _S32, _TG, 19, _NC,  _V, _NA, _NS)
VMCS_FIELD(SMBASE,                  0x4828, _S32, _TG, 20, _NC,  _V, _NA, _NS)
VMCS_FIELD(SYSENTER_CS,             0x482A, _S32, _TG, 21, _NC,  _V, _RW, _NS)
VMCS_UNUSED(                        0x482C, _S32, _TG, 22)
VMCS_FIELD(TIMER,                   0x482E, _S32, _TG, 23, _NC, _NV, _NA, _NS)
VMCS_UNUSED(                        0x4830, _S32, _TG, 24)
VMCS_UNUSED(                        0x4832, _S32, _TG, 25)
VMCS_UNUSED(                        0x4834, _S32, _TG, 26)
VMCS_UNUSED(                        0x4836, _S32, _TG, 27)
VMCS_UNUSED(                        0x4838, _S32, _TG, 28)
VMCS_UNUSED(                        0x483A, _S32, _TG, 29)
VMCS_UNUSED(                        0x483C, _S32, _TG, 30)
VMCS_UNUSED(                        0x483E, _S32, _TG, 31)

VMCS_GROUP_END(32, GUEST)

/* 32-bit host state. */
VMCS_GROUP_START(32, HOST)
VMCS_FIELD(HOST_SYSENTER_CS,        0x4C00, _S32, _TH,  0, _NC,  _V, _NA,  _S)
VMCS_UNUSED(                        0x4C02, _S32, _TH,  1)
VMCS_UNUSED(                        0x4C04, _S32, _TH,  2)
VMCS_UNUSED(                        0x4C06, _S32, _TH,  3)
VMCS_UNUSED(                        0x4C08, _S32, _TH,  4)
VMCS_UNUSED(                        0x4C0A, _S32, _TH,  5)
VMCS_UNUSED(                        0x4C0C, _S32, _TH,  6)
VMCS_UNUSED(                        0x4C0E, _S32, _TH,  7)
VMCS_UNUSED(                        0x4C10, _S32, _TH,  8)
VMCS_UNUSED(                        0x4C12, _S32, _TH,  9)
VMCS_UNUSED(                        0x4C14, _S32, _TH,  10)
VMCS_UNUSED(                        0x4C16, _S32, _TH,  11)
VMCS_UNUSED(                        0x4C18, _S32, _TH,  12)
VMCS_UNUSED(                        0x4C1A, _S32, _TH,  13)
VMCS_UNUSED(                        0x4C1C, _S32, _TH,  14)
VMCS_UNUSED(                        0x4C1E, _S32, _TH,  15)
VMCS_UNUSED(                        0x4C20, _S32, _TH,  16)
VMCS_UNUSED(                        0x4C22, _S32, _TH,  17)
VMCS_UNUSED(                        0x4C24, _S32, _TH,  18)
VMCS_UNUSED(                        0x4C26, _S32, _TH,  19)
VMCS_UNUSED(                        0x4C28, _S32, _TH,  20)
VMCS_UNUSED(                        0x4C2A, _S32, _TH,  21)
VMCS_UNUSED(                        0x4C2C, _S32, _TH,  22)
VMCS_UNUSED(                        0x4C2E, _S32, _TH,  23)
VMCS_UNUSED(                        0x4C30, _S32, _TH,  24)
VMCS_UNUSED(                        0x4C32, _S32, _TH,  25)
VMCS_UNUSED(                        0x4C34, _S32, _TH,  26)
VMCS_UNUSED(                        0x4C36, _S32, _TH,  27)
VMCS_UNUSED(                        0x4C38, _S32, _TH,  28)
VMCS_UNUSED(                        0x4C3A, _S32, _TH,  29)
VMCS_UNUSED(                        0x4C3C, _S32, _TH,  30)
VMCS_UNUSED(                        0x4C3E, _S32, _TH,  31)
VMCS_GROUP_END(32, HOST)
VMCS_SET_END(32)


VMCS_SET_START(NAT)
/* natural-width control fields. */
VMCS_GROUP_START(NAT, CTL)
VMCS_FIELD(CR0_GHMASK,              0x6000,  _SN, _TC,  0,  _C,  _V, _URW,  _S)
VMCS_FIELD(CR4_GHMASK,              0x6002,  _SN, _TC,  1,  _C,  _V, _URW,  _S)
VMCS_FIELD(CR0_SHADOW,              0x6004,  _SN, _TC,  2, _NC,  _V,  _NA,  _S)
VMCS_FIELD(CR4_SHADOW,              0x6006,  _SN, _TC,  3, _NC,  _V,  _NA,  _S)
VMCS_FIELD(CR3_TARGVAL0,            0x6008,  _SN, _TC,  4, _NC,  _V,  _NA,  _S)
VMCS_FIELD(CR3_TARGVAL1,            0x600A,  _SN, _TC,  5, _NC,  _V,  _NA,  _S)
VMCS_FIELD(CR3_TARGVAL2,            0x600C,  _SN, _TC,  6, _NC,  _V,  _NA,  _S)
VMCS_FIELD(CR3_TARGVAL3,            0x600E,  _SN, _TC,  7, _NC,  _V,  _NA,  _S)
VMCS_UNUSED(                        0x6010,  _SN, _TC,  8)
VMCS_UNUSED(                        0x6012,  _SN, _TC,  9)
VMCS_UNUSED(                        0x6014,  _SN, _TC,  10)
VMCS_UNUSED(                        0x6016,  _SN, _TC,  11)
VMCS_UNUSED(                        0x6018,  _SN, _TC,  12)
VMCS_UNUSED(                        0x601A,  _SN, _TC,  13)
VMCS_UNUSED(                        0x601C,  _SN, _TC,  14)
VMCS_UNUSED(                        0x601E,  _SN, _TC,  15)
VMCS_UNUSED(                        0x6020,  _SN, _TC,  16)
VMCS_UNUSED(                        0x6022,  _SN, _TC,  17)
VMCS_UNUSED(                        0x6024,  _SN, _TC,  18)
VMCS_UNUSED(                        0x6026,  _SN, _TC,  19)
VMCS_UNUSED(                        0x6028,  _SN, _TC,  20)
VMCS_UNUSED(                        0x602A,  _SN, _TC,  21)
VMCS_UNUSED(                        0x602C,  _SN, _TC,  22)
VMCS_UNUSED(                        0x602E,  _SN, _TC,  23)
VMCS_UNUSED(                        0x6030,  _SN, _TC,  24)
VMCS_UNUSED(                        0x6032,  _SN, _TC,  25)
VMCS_UNUSED(                        0x6034,  _SN, _TC,  26)
VMCS_UNUSED(                        0x6036,  _SN, _TC,  27)
VMCS_UNUSED(                        0x6038,  _SN, _TC,  28)
VMCS_UNUSED(                        0x603A,  _SN, _TC,  29)
VMCS_UNUSED(                        0x603C,  _SN, _TC,  30)
VMCS_UNUSED(                        0x603E,  _SN, _TC,  31)
VMCS_GROUP_END(NAT, CTL)

VMCS_GROUP_START(NAT, DATA)
VMCS_FIELD(EXIT_QUAL,               0x6400,  _SN, _TD,  0, _NC,  _V, _RO, _NS)
VMCS_FIELD(IO_RCX,                  0x6402,  _SN, _TD,  1, _NC,  _V, _RO, _NS)
VMCS_FIELD(IO_RSI,                  0x6404,  _SN, _TD,  2, _NC,  _V, _RO, _NS)
VMCS_FIELD(IO_RDI,                  0x6406,  _SN, _TD,  3, _NC,  _V, _RO, _NS)
VMCS_FIELD(IO_RIP,                  0x6408,  _SN, _TD,  4, _NC,  _V, _RO, _NS)
VMCS_FIELD(LINEAR_ADDR,             0x640A,  _SN, _TD,  5, _NC,  _V, _RO, _NS)
VMCS_UNUSED(                        0x640C,  _SN, _TD,  6)
VMCS_UNUSED(                        0x640E,  _SN, _TD,  7)
VMCS_UNUSED(                        0x6410,  _SN, _TD,  8)
VMCS_UNUSED(                        0x6412,  _SN, _TD,  9)
VMCS_UNUSED(                        0x6414,  _SN, _TD,  10)
VMCS_UNUSED(                        0x6416,  _SN, _TD,  11)
VMCS_UNUSED(                        0x6418,  _SN, _TD,  12)
VMCS_UNUSED(                        0x641A,  _SN, _TD,  13)
VMCS_UNUSED(                        0x641C,  _SN, _TD,  14)
VMCS_UNUSED(                        0x641E,  _SN, _TD,  15)
VMCS_UNUSED(                        0x6420,  _SN, _TD,  16)
VMCS_UNUSED(                        0x6422,  _SN, _TD,  17)
VMCS_UNUSED(                        0x6424,  _SN, _TD,  18)
VMCS_UNUSED(                        0x6426,  _SN, _TD,  19)
VMCS_UNUSED(                        0x6428,  _SN, _TD,  20)
VMCS_UNUSED(                        0x642A,  _SN, _TD,  21)
VMCS_UNUSED(                        0x642C,  _SN, _TD,  22)
VMCS_UNUSED(                        0x642E,  _SN, _TD,  23)
VMCS_UNUSED(                        0x6430,  _SN, _TD,  24)
VMCS_UNUSED(                        0x6432,  _SN, _TD,  25)
VMCS_UNUSED(                        0x6434,  _SN, _TD,  26)
VMCS_UNUSED(                        0x6436,  _SN, _TD,  27)
VMCS_UNUSED(                        0x6438,  _SN, _TD,  28)
VMCS_UNUSED(                        0x643A,  _SN, _TD,  29)
VMCS_UNUSED(                        0x643C,  _SN, _TD,  30)
VMCS_UNUSED(                        0x643E,  _SN, _TD,  31)
VMCS_GROUP_END(NAT, DATA)

/* natural-width guest state. */
VMCS_GROUP_START(NAT, GUEST)
VMCS_FIELD(CR0,                     0x6800,  _SN, _TG,  0, _NC,  _V, _URW, _NS)
VMCS_FIELD(CR3,                     0x6802,  _SN, _TG,  1, _NC,  _V,  _RW, _NS)
VMCS_FIELD(CR4,                     0x6804,  _SN, _TG,  2, _NC,  _V,  _RW, _NS)
VMCS_FIELD(ES_BASE,                 0x6806,  _SN, _TG,  3, _NC,  _V,  _RW, _NS)
VMCS_FIELD(CS_BASE,                 0x6808,  _SN, _TG,  4, _NC,  _V,  _RW, _NS)
VMCS_FIELD(SS_BASE,                 0x680A,  _SN, _TG,  5, _NC,  _V,  _RW, _NS)
VMCS_FIELD(DS_BASE,                 0x680C,  _SN, _TG,  6, _NC,  _V,  _RW, _NS)
VMCS_FIELD(FS_BASE,                 0x680E,  _SN, _TG,  7, _NC,  _V,  _RW, _NS)
VMCS_FIELD(GS_BASE,                 0x6810,  _SN, _TG,  8, _NC,  _V,  _RW, _NS)
VMCS_FIELD(LDTR_BASE,               0x6812,  _SN, _TG,  9, _NC,  _V,  _RW, _NS)
VMCS_FIELD(TR_BASE,                 0x6814,  _SN, _TG, 10, _NC,  _V,  _RW, _NS)
VMCS_FIELD(GDTR_BASE,               0x6816,  _SN, _TG, 11, _NC,  _V,  _RW, _NS)
VMCS_FIELD(IDTR_BASE,               0x6818,  _SN, _TG, 12, _NC,  _V,  _RW, _NS)
VMCS_FIELD(DR7,                     0x681A,  _SN, _TG, 13, _NC,  _V, _URW, _NS)
VMCS_FIELD(RSP,                     0x681C,  _SN, _TG, 14, _NC,  _V,  _RW, _NS)
VMCS_FIELD(RIP,                     0x681E,  _SN, _TG, 15, _NC,  _V,  _RW, _NS)
VMCS_FIELD(RFLAGS,                  0x6820,  _SN, _TG, 16, _NC,  _V,  _RW, _NS)
VMCS_FIELD(PENDDBG,                 0x6822,  _SN, _TG, 17, _NC,  _V,  _RW, _NS)
VMCS_FIELD(SYSENTER_ESP,            0x6824,  _SN, _TG, 18, _NC,  _V,  _RW, _NS)
VMCS_FIELD(SYSENTER_EIP,            0x6826,  _SN, _TG, 19, _NC,  _V,  _RW, _NS)
VMCS_FIELD(S_CET,                   0x6828,  _SN, _TG, 20, _NC,  _V,  _NA, _NS)
VMCS_FIELD(SSP,                     0x682A,  _SN, _TG, 21, _NC,  _V,  _NA, _NS)
VMCS_FIELD(ISST,                    0x682C,  _SN, _TG, 22, _NC,  _V,  _NA, _NS)
VMCS_UNUSED(                        0x682E,  _SN, _TG, 23)
VMCS_UNUSED(                        0x6830,  _SN, _TG, 24)
VMCS_UNUSED(                        0x6832,  _SN, _TG, 25)
VMCS_UNUSED(                        0x6834,  _SN, _TG, 26)
VMCS_UNUSED(                        0x6836,  _SN, _TG, 27)
VMCS_UNUSED(                        0x6838,  _SN, _TG, 28)
VMCS_UNUSED(                        0x683A,  _SN, _TG, 29)
VMCS_UNUSED(                        0x683C,  _SN, _TG, 30)
VMCS_UNUSED(                        0x683E,  _SN, _TG, 31)
VMCS_GROUP_END(NAT, GUEST)

/* natural-width host state. */
VMCS_GROUP_START(NAT, HOST)
VMCS_FIELD(HOST_CR0,                0x6C00,  _SN, _TH,  0, _NC,  _V, _URW,  _S)
VMCS_FIELD(HOST_CR3,                0x6C02,  _SN, _TH,  1, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_CR4,                0x6C04,  _SN, _TH,  2, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_FSBASE,             0x6C06,  _SN, _TH,  3, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_GSBASE,             0x6C08,  _SN, _TH,  4, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_TRBASE,             0x6C0A,  _SN, _TH,  5, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_GDTRBASE,           0x6C0C,  _SN, _TH,  6, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_IDTRBASE,           0x6C0E,  _SN, _TH,  7, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_SYSENTER_ESP,       0x6C10,  _SN, _TH,  8, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_SYSENTER_EIP,       0x6C12,  _SN, _TH,  9, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_RSP,                0x6C14,  _SN, _TH, 10, _NC,  _V, _URW,  _S)
VMCS_FIELD(HOST_RIP,                0x6C16,  _SN, _TH, 11, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_S_CET,              0x6C18,  _SN, _TH, 12, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_SSP,                0x6C1A,  _SN, _TH, 13, _NC,  _V,  _NA,  _S)
VMCS_FIELD(HOST_ISST,               0x6C1C,  _SN, _TH, 14, _NC,  _V,  _NA,  _S)
VMCS_UNUSED(                        0x6C1E,  _SN, _TH, 15)
VMCS_UNUSED(                        0x6C20,  _SN, _TH, 16)
VMCS_UNUSED(                        0x6C22,  _SN, _TH, 17)
VMCS_UNUSED(                        0x6C24,  _SN, _TH, 18)
VMCS_UNUSED(                        0x6C26,  _SN, _TH, 19)
VMCS_UNUSED(                        0x6C28,  _SN, _TH, 20)
VMCS_UNUSED(                        0x6C2A,  _SN, _TH, 21)
VMCS_UNUSED(                        0x6C2C,  _SN, _TH, 22)
VMCS_UNUSED(                        0x6C2E,  _SN, _TH, 23)
VMCS_UNUSED(                        0x6C30,  _SN, _TH, 24)
VMCS_UNUSED(                        0x6C32,  _SN, _TH, 25)
VMCS_UNUSED(                        0x6C34,  _SN, _TH, 26)
VMCS_UNUSED(                        0x6C36,  _SN, _TH, 27)
VMCS_UNUSED(                        0x6C38,  _SN, _TH, 28)
VMCS_UNUSED(                        0x6C3A,  _SN, _TH, 29)
VMCS_UNUSED(                        0x6C3C,  _SN, _TH, 30)
VMCS_UNUSED(                        0x6C3E,  _SN, _TH, 31)
VMCS_GROUP_END(NAT, HOST)
VMCS_SET_END(NAT)


#ifdef LOCAL_GROUP_START
#undef VMCS_GROUP_START
#undef LOCAL_GROUP_START
#endif

#ifdef LOCAL_GROUP_END
#undef VMCS_GROUP_END
#undef LOCAL_GROUP_END
#endif

#ifdef LOCAL_SET_START
#undef VMCS_SET_START
#undef LOCAL_SET_START
#endif

#ifdef LOCAL_SET_END
#undef VMCS_SET_END
#undef LOCAL_SET_END
#endif

#ifdef LOCAL_UNUSED
#undef VMCS_UNUSED
#undef LOCAL_UNUSED
#endif
