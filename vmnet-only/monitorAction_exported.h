/*********************************************************
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
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

#ifndef _MONITORACTION_EXPORTED_H_
#define _MONITORACTION_EXPORTED_H_

#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_assert.h"
#include "vm_atomic.h"
#include "vm_basic_types.h"

/*
 * Please bump the version number if your change will break the
 * compatability to the drivers.
 */
#define ACTION_EXPORTED_VERSION   2

#define ACTION_WORD_SIZE   (sizeof(uint64) * 8)
#define ACTION_NUM_WORDS   (2)
#define ACTION_NUM_IDS     (ACTION_NUM_WORDS * ACTION_WORD_SIZE)

#define MONACTION_INVALID  MAX_UINT32

typedef uint32 MonitorIdemAction;

/*
 * Representation of a set of actions.
 */
typedef struct MonitorActionSet {
   volatile uint64 word[ACTION_NUM_WORDS];
} MonitorActionSet;

#ifndef __cplusplus
typedef enum MonitorActionSetName MonitorActionSetName;
#endif

/*
 * Summary of action and interrupt states.
 */
typedef struct MonitorActionIntr {
   MonitorActionSet  pendingSet;
   volatile Bool     action;
   Bool              intr;
   Bool              nmi;
   Bool              db;
   uint32            _pad;
} MonitorActionIntr;

/*
 *------------------------------------------------------------------------
 * MonitorActionSet_AtomicInclude --
 *
 *    This function atomically adds an action to an action set.
 *
 * Results:
 *    TRUE if the action being added did not exist in the action set.
 *    FALSE otherwise.
 *
 * Side effects:
 *    The given action set will be updated.
 *------------------------------------------------------------------------
 */
static INLINE Bool
MonitorActionSet_AtomicInclude(MonitorActionSet *set, const uint32 actionID)
{
   Atomic_uint64 *atomicSet =
      Atomic_VolatileToAtomic64(&set->word[actionID / ACTION_WORD_SIZE]);
   uint64 mask = (uint64)1 << (actionID % ACTION_WORD_SIZE);
   uint64 oldWord;
   uint64 newWord;

   ASSERT_ON_COMPILE((ACTION_WORD_SIZE & (ACTION_WORD_SIZE - 1)) == 0);
#ifdef VMX86_DEBUG
   /* If ASSERT is not desirable, do explicit check. Please see PR 567811. */
#ifdef MODULE
   if (UNLIKELY(actionID / ACTION_WORD_SIZE >= ACTION_NUM_WORDS)) {
      return FALSE;
   }
#else
   ASSERT(actionID / ACTION_WORD_SIZE < ACTION_NUM_WORDS);
#endif // MODULE
#endif // VMX86_DEBUG
   do {
      oldWord = Atomic_Read64(atomicSet);
      newWord = oldWord | mask;
   } while (!Atomic_CMPXCHG64(atomicSet, &oldWord, &newWord));
   return (oldWord & mask) == 0;
}


/*
 *----------------------------------------------------------------------------
 * MonitorAction_SetBits --
 *
 *      The core logic for posting an action. Update the set of pending
 *      actions of the target VCPU in the shared area to mark the action
 *      as present. Make sure the bit is set in the pendingSet first to
 *      avoid a race with the drain loop.
 *
 *      It's the responsibility of the callers to ensure that the change
 *      to actionIntr->action is globally visible before any IPI is sent
 *      (the change to pendingSet is pushed out by the cmpxchg in
 *      MonitorActionSet_AtomicInclude).
 *
 * Results:
 *      TRUE if the action being posted was not pending before.
 *      FALSE otherwise (other threads could have posted the same action).
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */
static INLINE Bool
MonitorAction_SetBits(MonitorActionIntr *actionIntr, MonitorIdemAction actionID)
{
   /* Careful if optimizing this: see PR70016. */
   Bool newAction =
      MonitorActionSet_AtomicInclude(&actionIntr->pendingSet, actionID);
   actionIntr->action = TRUE;
   return newAction;
}

/*
 * C1 states entered by monitor while waiting for an action
 */
typedef enum {
   VMM_C1_STATE_INVALID = 0,
   VMM_C1_STATE_HLT,
   VMM_C1_STATE_MWAIT,
   VMM_C1_STATE_PAUSE
} vmmC1StateType;

#endif // _MONITORACTION_EXPORTED_H_
