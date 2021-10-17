/*********************************************************
 * Copyright (C) 1998-2018,2020 VMware, Inc. All rights reserved.
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
 * memtrack.c --
 *
 *    Utility module for tracking pinned memory, which allows later
 *    lookup by VPN.
 *
 * --
 *
 * Track memory using a 3-level directory, to keep allocations to one
 * page in size. The first level is inlined with the MemTrack struct
 * and a new page is allocated for each directory in the second level,
 * as needed. The third level packs in as many MemTrackEntry structs
 * on a single page as possible.
 *
 * Optionally use a 2-level directory on systems that prefer larger
 * contiguous allocations. In this case we allow the first level
 * allocation to be larger than 1 page (its size will depend on the
 * maximum number of tracked pages, currently set to 4GB).
 *
 *   MemTrack      MemTrackDir1        MemTrackDir2      MemTrackDir3
 *   (Handle)                           (Optional)
 *
 *                                ---->/----------\
 *                                |    | Dir[0]   |----->/----------\
 *                                |    | Dir[1]   |      | Entry[0] |
 *                                |    | ...      |      | Entry[1] |
 *                                |    | Dir[N]   |      | ...      |
 * /----------\  MEMTRACK_DIR2_ENTRIES \----------/      | Entry[N] |
 * | ...      |                   |                      \----------/
 * | dir1     |----/----------\   |  ->/----------\
 * | ...      |\   | Dir[0]   |----  | | Dir[N+1] |
 * |          | \  | Dir[1]   |------- | Dir[N+2] |
 * \----------/  \ | ...      |        | ...      |
 *                \| Dir[N]   |-----   | Dir[N+N] |           .
 *                 \----------/    |   \----------/           .
 *   MEMTRACK_DIR1_ENTRIES         |                          .
 *                                 --->/----------\
 *                                     | ...      |
 *                                     |          |
 *                                     |          |
 *                                     | Dir[M]   |----->/----------\
 *                                     \----------/      | ...      |
 *                                                       |          |
 *                                                       |          |
 *                                                       | Entry[M] |
 *                                                       \----------/
 *
 * We also keep a simple chaining hash table of entries hashed on
 * the VPN, for quick lookup. A separate hash table hashed on the MPN
 * exists as well, but this is only used in debug builds.
 *
 * This tracker does not allow pages to be removed. If, in the future,
 * we have a use case for removing MPNs from the tracker, a simple
 * MemTrackEntry recycle scheme can be implemented at the cost of an
 * additional pointer per MemTrackEntry instance.
 */

#if defined(__linux__)
/* Must come before any kernel header file. */
#   include "driver-config.h"

#   include <linux/string.h> /* memset() in the kernel */
#elif defined(WINNT_DDK)
#   undef PAGE_SIZE          /* Redefined in ntddk.h, and we use that defn. */
#   undef PAGE_SHIFT
#   include <ntddk.h>
#else
#   include <string.h>
#endif

#include "hostif.h"
#include "vmx86.h"

#include "memtrack.h"

/*
 * Modify this value to increase the maximum number of tracked pages
 * per MemTrack instance.
 */
#define MEMTRACK_MAX_TRACKED        GBYTES_2_PAGES(119)

/*
 * Linux uses a 3-level directory, because we want to keep allocations
 * to a single page.
 */
#if defined(__linux__)
#define MEMTRACK_3LEVEL             (1)
#endif

#define MEMTRACK_DIR3_ENTRIES       (PAGE_SIZE / sizeof (MemTrackEntry))
#if defined(MEMTRACK_3LEVEL)
#define MEMTRACK_DIR2_ENTRIES       (PAGE_SIZE / sizeof (void *))
#else
#define MEMTRACK_DIR2_ENTRIES       (1)
#endif
#define MEMTRACK_DIR1_ENTRIES       CEILING(MEMTRACK_MAX_TRACKED,        \
                                            (MEMTRACK_DIR2_ENTRIES *     \
                                             MEMTRACK_DIR3_ENTRIES))

#define MEMTRACK_HT_SIZE            (16384)
#define MEMTRACK_HT_ENTRIES         (PAGE_SIZE / sizeof (void *))
#define MEMTRACK_HT_PAGES           (MEMTRACK_HT_SIZE / MEMTRACK_HT_ENTRIES)

typedef struct MemTrackDir3 {
   MemTrackEntry     entries[MEMTRACK_DIR3_ENTRIES];
} MemTrackDir3;

#if defined(MEMTRACK_3LEVEL)
typedef struct MemTrackDir2 {
   MemTrackDir3     *dir[MEMTRACK_DIR2_ENTRIES];
} MemTrackDir2;
#else
typedef struct MemTrackDir3 MemTrackDir2;
#endif

typedef struct MemTrackDir1 {
   MemTrackDir2     *dir[MEMTRACK_DIR1_ENTRIES];
} MemTrackDir1;

typedef struct MemTrackHTPage {
   MemTrackEntry    *entries[MEMTRACK_HT_ENTRIES];
} MemTrackHTPage;

typedef struct MemTrackHT {
   MemTrackHTPage   *pages[MEMTRACK_HT_PAGES];
} MemTrackHT;

typedef uint64 MemTrackHTKey;

typedef struct MemTrack {
   VMDriver         *vm;            /* The VM instance. */
   PageCnt           numPages;      /* Number of pages tracked. */
   MemTrackDir1      dir1;          /* First level directory. */
   MemTrackHT        vpnHashTable;  /* VPN to entry hashtable. */
   MemTrackHT       *mpnHashTable;  /* MPN to entry hashtable. */
} MemTrack;

/*
 * The following functions and macros help allocate and access the
 * directory structure. This is convenient because the second level
 * directory is optional.
 */

#define MEMTRACK_IDX2DIR(_idx, _p1, _p2, _p3)                          \
   do {                                                                \
      _p1   = _idx / (MEMTRACK_DIR2_ENTRIES * MEMTRACK_DIR3_ENTRIES);  \
      _p2   = (_idx / MEMTRACK_DIR3_ENTRIES) % MEMTRACK_DIR2_ENTRIES;  \
      _p3   = _idx % MEMTRACK_DIR3_ENTRIES;                            \
   } while (0)

#define MEMTRACK_GETDIR2(_dir1, _p1)     (_dir1->dir[_p1])
#define MEMTRACK_ALLOCDIR2(_dir1, _p1)   MemTrackAllocDir2(_dir1, _p1)
#define MEMTRACK_FREEDIR2(_dir1)         HostIF_FreePage(_dir1)

#if defined(MEMTRACK_3LEVEL)
#define MEMTRACK_GETENTRY(_dir1, _p1, _p2, _p3) \
                                         (&((_dir1->dir[_p1])->dir[_p2])->entries[_p3])
#define MEMTRACK_GETDIR3(_dir2, _p2)     (_dir2->dir[_p2])
#define MEMTRACK_ALLOCDIR3(_dir2, _p2)   MemTrackAllocDir3(_dir2, _p2)
#define MEMTRACK_FREEDIR3(_dir2)         HostIF_FreePage(_dir2)
#else
#define MEMTRACK_GETENTRY(_dir1, _p1, _p2, _p3) \
                                         (&(_dir1->dir[_p1])->entries[_p3])
#define MEMTRACK_GETDIR3(_dir2, _p2)     (_dir2)
#define MEMTRACK_ALLOCDIR3(_dir2, _p2)   (_dir2)
#define MEMTRACK_FREEDIR3(_dir2)
#endif

static INLINE void *
MemTrackAllocPage(void)
{
   void *ptr = HostIF_AllocPage();
   if (ptr != NULL) {
      memset(ptr, 0, PAGE_SIZE);
   }
   return ptr;
}

#define MEMTRACK_ALLOCDFN(_name, _itype, _otype)   \
   static INLINE _otype *                          \
   _name(_itype *arg, uint64 pos)                  \
   {                                               \
      if (arg->dir[pos] == NULL) {                 \
         arg->dir[pos] = MemTrackAllocPage();      \
      }                                            \
      return arg->dir[pos];                        \
   }

#if defined(MEMTRACK_3LEVEL)
MEMTRACK_ALLOCDFN(MemTrackAllocDir3, MemTrackDir2, MemTrackDir3)
#endif
MEMTRACK_ALLOCDFN(MemTrackAllocDir2, MemTrackDir1, MemTrackDir2)


/*
 *----------------------------------------------------------------------
 *
 * MemTrackHTLookup --
 * MemTrackHTInsert --
 *
 *      Helper functions to insert or lookup entries in the VPN or
 *      MPN hash tables. Hash tables are always allocated in page
 *      size chunks.
 *
 *----------------------------------------------------------------------
 */

#define MEMTRACK_HASHKEY(_key, _hash, _page, _pos)  \
   do {                                             \
      _hash = _key % MEMTRACK_HT_SIZE;              \
      _page = _hash / MEMTRACK_HT_ENTRIES;          \
      _pos  = _hash % MEMTRACK_HT_ENTRIES;          \
   } while(0)

static INLINE MemTrackEntry **
MemTrackHTLookup(MemTrackHT *ht,       // IN
                 MemTrackHTKey key)    // IN
{
   uint64 hash, page, pos;

   MEMTRACK_HASHKEY(key, hash, page, pos);

   return &ht->pages[page]->entries[pos];
}

static INLINE void
MemTrackHTInsert(MemTrackHT *ht,          // IN
                 MemTrackEntry *ent,      // IN
                 MemTrackEntry **chain,   // OUT
                 MemTrackHTKey key)       // IN
{
   MemTrackEntry **head = MemTrackHTLookup(ht, key);
   *chain = *head;
   *head = ent;
}


/*
 *----------------------------------------------------------------------
 *
 * MemTrackCleanup --
 *
 *      Deallocate all memory associated with the specified tracker.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Memory deallocation.
 *
 *----------------------------------------------------------------------
 */

static void
MemTrackCleanup(MemTrack *mt)    // IN
{
   PageCnt idx;
   uint64 p1;
   MemTrackDir1 *dir1;
   Bool freeBackMap = mt != NULL && mt->mpnHashTable != NULL;

   if (mt == NULL) {
      return;
   }
   dir1 = &mt->dir1;

   for (p1 = 0; p1 < MEMTRACK_DIR1_ENTRIES; p1++) {
      uint64 p2;
      MemTrackDir2 *dir2 = MEMTRACK_GETDIR2(dir1, p1);

      if (dir2 == NULL) {
         break;
      }
      for (p2 = 0; p2 < MEMTRACK_DIR2_ENTRIES; p2++) {
         MemTrackDir3 *dir3 = MEMTRACK_GETDIR3(dir2, p2);

         if (dir3 == NULL) {
            break;
         }
         MEMTRACK_FREEDIR3(dir3);
      }
      MEMTRACK_FREEDIR2(dir2);
   }

   for (idx = 0; idx < MEMTRACK_HT_PAGES; idx++) {
      if (mt->vpnHashTable.pages[idx] != NULL) {
         HostIF_FreePage(mt->vpnHashTable.pages[idx]);
      }
      if (freeBackMap) {
         if (mt->mpnHashTable->pages[idx] != NULL) {
            HostIF_FreePage(mt->mpnHashTable->pages[idx]);
         }
      }
   }
   if (freeBackMap) {
      HostIF_FreeKernelMem(mt->mpnHashTable);
   }

   HostIF_FreeKernelMem(mt);
}


/*
 *----------------------------------------------------------------------
 *
 * MemTrack_Init --
 *
 *      Allocate and initialize the tracker.
 *
 * Results:
 *      Handle used to access the tracker.
 *
 * Side effects:
 *      Memory allocation.
 *
 *----------------------------------------------------------------------
 */

MemTrack *
MemTrack_Init(VMDriver *vm) // IN:
{
   MemTrack *mt;
   PageCnt idx;

#if defined(MEMTRACK_3LEVEL)
   ASSERT_ON_COMPILE(sizeof *mt <= PAGE_SIZE);
   ASSERT_ON_COMPILE(sizeof (MemTrackDir2) == PAGE_SIZE);
#endif
   ASSERT_ON_COMPILE(sizeof (MemTrackDir3) <= PAGE_SIZE);

   mt = HostIF_AllocKernelMem(sizeof *mt, FALSE);
   if (mt == NULL) {
      Warning("MemTrack failed to allocate handle.\n");
      goto error;
   }
   memset(mt, 0, sizeof *mt);
   mt->vm = vm;

   for (idx = 0; idx < MEMTRACK_HT_PAGES; idx++) {
      MemTrackHTPage *htPage = MemTrackAllocPage();

      if (htPage == NULL) {
         Warning("MemTrack failed to allocate VPN hash table (%"FMT64"d).\n",
                 idx);
         goto error;
      }
      mt->vpnHashTable.pages[idx] = htPage;
   }

   mt->mpnHashTable = HostIF_AllocKernelMem(sizeof *mt->mpnHashTable, FALSE);
   if (mt->mpnHashTable == NULL) {
      Warning("MemTrack failed to allocate MPN hash table.\n");
      goto error;
   }
   memset(mt->mpnHashTable, 0, sizeof *mt->mpnHashTable);
   for (idx = 0; idx < MEMTRACK_HT_PAGES; idx++) {
      MemTrackHTPage *htPage = MemTrackAllocPage();
      if (htPage == NULL) {
         Warning("MemTrack failed to allocate MPN hash table (%"FMT64"d).\n",
                 idx);
         goto error;
      }
      mt->mpnHashTable->pages[idx] = htPage;
   }

   return mt;

error:
   MemTrackCleanup(mt);
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * MemTrack_Add --
 *
 *      Add the specified VPN:MPN pair to the memory tracker.
 *
 * Results:
 *      A pointer to the element, or NULL on error.
 *
 * Side effects:
 *      Memory allocation.
 *
 *----------------------------------------------------------------------
 */

MemTrackEntry *
MemTrack_Add(MemTrack *mt,    // IN
             VPN64 vpn,       // IN
             MPN mpn)         // IN
{
   PageCnt idx = mt->numPages;
   uint64 p1, p2, p3;
   MemTrackEntry *ent;
   MemTrackDir1 *dir1 = &mt->dir1;
   MemTrackDir2 *dir2;
   MemTrackDir3 *dir3;
   MEMTRACK_IDX2DIR(idx, p1, p2, p3);


   ASSERT(HostIF_VMLockIsHeld(mt->vm));

   if (p1 >= MEMTRACK_DIR1_ENTRIES ||
       p2 >= MEMTRACK_DIR2_ENTRIES ||
       p3 >= MEMTRACK_DIR3_ENTRIES) {
      return NULL;
   }

   dir2 = MEMTRACK_ALLOCDIR2(dir1, p1);
   if (dir2 == NULL) {
      return NULL;
   }

   dir3 = MEMTRACK_ALLOCDIR3(dir2, p2);
   if (dir3 == NULL) {
      return NULL;
   }

   ent = MEMTRACK_GETENTRY(dir1, p1, p2, p3);
   ent->vpn = vpn;
   ent->mpn = mpn;

   MemTrackHTInsert(&mt->vpnHashTable, ent, &ent->vpnChain, ent->vpn);
   MemTrackHTInsert(mt->mpnHashTable, ent, &ent->mpnChain, ent->mpn);

   mt->numPages++;

   return ent;
}


/*
 *----------------------------------------------------------------------
 *
 * MemTrack_LookupVPN --
 *
 *      Lookup the specified VPN address in the memory tracker.
 *
 * Results:
 *      A pointer to the element, or NULL if not there.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MemTrackEntry *
MemTrack_LookupVPN(MemTrack *mt, // IN
                   VPN64 vpn)    // IN
{
   MemTrackEntry *next = *MemTrackHTLookup(&mt->vpnHashTable, vpn);
   ASSERT(HostIF_VMLockIsHeld(mt->vm));

   while (next != NULL) {
      if (next->vpn == vpn) {
         return next;
      }
      next = next->vpnChain;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * MemTrack_LookupMPN --
 *
 *      Lookup the specified MPN address in the memory tracker.
 *
 * Results:
 *      A pointer to the element, or NULL if not there.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
MemTrackEntry *
MemTrack_LookupMPN(MemTrack *mt, // IN
                   MPN mpn)      // IN
{
   MemTrackEntry *next;
   ASSERT(HostIF_VMLockIsHeld(mt->vm));
   next = *MemTrackHTLookup(mt->mpnHashTable, mpn);

   while (next != NULL) {
      if (next->mpn == mpn) {
         return next;
      }
      next = next->mpnChain;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * MemTrack_Cleanup --
 *
 *      Cleanup all resources allocated for the tracker. For
 *      all pages in the tracker call the user provided clean up
 *      function.
 *
 * Results:
 *      Number of pages in the tracker.
 *
 * Side effects:
 *      Memory deallocation.
 *
 *----------------------------------------------------------------------
 */

PageCnt
MemTrack_Cleanup(MemTrack *mt,            // IN
                 MemTrackCleanupCb *cb,   // IN
                 void *cData)             // IN
{
   PageCnt idx;
   PageCnt count = 0;

   for (idx = 0; idx < mt->numPages; idx++) {
      uint64 p1, p2, p3;
      MemTrackEntry *ent;
      MemTrackDir1 *dir1 = &mt->dir1;
      MEMTRACK_IDX2DIR(idx, p1, p2, p3);

      ent = MEMTRACK_GETENTRY(dir1, p1, p2, p3);
      cb(cData, ent);

      count++;
   }

   MemTrackCleanup(mt);

   return count;
}
