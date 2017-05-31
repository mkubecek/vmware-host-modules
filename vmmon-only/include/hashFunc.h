/*********************************************************
 * Copyright (C) 2001,2016 VMware, Inc. All rights reserved.
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

#ifndef _HASH_FUNC_H
#define _HASH_FUNC_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL

#include "includeCheck.h"

/*
 * hashFunc.h --
 *
 *	Hash functions defined as simple wrappers around code placed
 *	in the public domain by Bob Jenkins.  His original code can
 *	be used freely for any purpose, including commercial (see the
 *	original comments in the enclosed code).
 */

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"

/*
 * operations
 */

extern uint64 HashFunc_HashBytes(const uint8 *key,  uint32 nBytes);
extern uint64 HashFunc_HashMoreBytes(const uint8 *key, uint32 nBytes, 
                                     uint64 initialValue);
extern uint64 HashFunc_HashBytesSlow(const uint8 *key, uint32 nBytes);
extern uint64 HashFunc_HashQuads(const uint64 *key, uint32 nQuads);
extern uint64 HashFunc_HashPage(const void *addr);

/*
--------------------------------------------------------------------
Hashes 32-bit key and returns a value that can fit into numBits bits.
--------------------------------------------------------------------
*/

static INLINE uint32
HashFunc_TruncHash32(uint32 key, unsigned numBits)
{
   const uint32 hashConst = 0x9e3779b9; /* the golden ratio */
   ASSERT(numBits > 0 && numBits <= 32);
   return (key * hashConst) >> (32 - numBits);
}

/*
--------------------------------------------------------------------
Hashes 64-bit key and returns a value that can fit into numBits bits.
--------------------------------------------------------------------
*/

static INLINE uint32
HashFunc_TruncHash64(uint64 key, unsigned numBits)
{
   return HashFunc_TruncHash32((uint32)(key ^ (key >> 32)), numBits);
}


/*
 * Original Bob Jenkins public-domain code with minor modifications
 * (static functions, 32-bit length args, disabled self-test code).
 */


typedef uint64 ub8;   /* unsigned 8-byte quantities */
typedef uint32 ub4;   /* unsigned 4-byte quantities */
typedef uint8  ub1;

#define hashsize(n) ((ub8)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
--------------------------------------------------------------------
mix -- mix 3 64-bit values reversibly.
mix() takes 48 machine instructions, but only 24 cycles on a superscalar
  machine (like Intel's new MMX architecture).  It requires 4 64-bit
  registers for 4::2 parallelism.
All 1-bit deltas, all 2-bit deltas, all deltas composed of top bits of
  (a,b,c), and all deltas of bottom bits were tested.  All deltas were
  tested both on random keys and on keys that were nearly all zero.
  These deltas all cause every bit of c to change between 1/3 and 2/3
  of the time (well, only 113/400 to 287/400 of the time for some
  2-bit delta).  These deltas all cause at least 80 bits to change
  among (a,b,c) when the mix is run either forward or backward (yes it
  is reversible).
This implies that a hash using mix64 has no funnels.  There may be
  characteristics with 3-bit deltas or bigger, I didn't test for
  those.
--------------------------------------------------------------------
*/
#define mix64(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>43); \
  b -= c; b -= a; b ^= (a<<9); \
  c -= a; c -= b; c ^= (b>>8); \
  a -= b; a -= c; a ^= (c>>38); \
  b -= c; b -= a; b ^= (a<<23); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>35); \
  b -= c; b -= a; b ^= (a<<49); \
  c -= a; c -= b; c ^= (b>>11); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<18); \
  c -= a; c -= b; c ^= (b>>22); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 64-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 8-byte value
Returns a 64-bit value.  Every bit of the key affects every bit of
the return value.  No funnels.  Every 1-bit and 2-bit delta achieves
avalanche.  About 41+5len instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 64 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, Jan 4 1997.  bob_jenkins@burtleburtle.net.  You may
use this code any way you wish, private, educational, or commercial,
as long as this whole comment accompanies it.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^64
is acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

static INLINE ub8 hash(register const ub1 *k, /* the key */
                       register ub4 length,   /* the length of the key */
                       register ub8 level)    /* the previous hash, or an arbitrary value */
{
  register ub8 a,b,c;
  ub4 len;

  /* Set up the internal state */
  len = length;
  a = b = level;                         /* the previous hash value */
  c = CONST64U(0x9e3779b97f4a7c13); /* the golden ratio; an arbitrary value */

  /*---------------------------------------- handle most of the key */
  while (len >= 24)
  {
    a += (k[0]        +((ub8)k[ 1]<< 8)+((ub8)k[ 2]<<16)+((ub8)k[ 3]<<24)
     +((ub8)k[4 ]<<32)+((ub8)k[ 5]<<40)+((ub8)k[ 6]<<48)+((ub8)k[ 7]<<56));
    b += (k[8]        +((ub8)k[ 9]<< 8)+((ub8)k[10]<<16)+((ub8)k[11]<<24)
     +((ub8)k[12]<<32)+((ub8)k[13]<<40)+((ub8)k[14]<<48)+((ub8)k[15]<<56));
    c += (k[16]       +((ub8)k[17]<< 8)+((ub8)k[18]<<16)+((ub8)k[19]<<24)
     +((ub8)k[20]<<32)+((ub8)k[21]<<40)+((ub8)k[22]<<48)+((ub8)k[23]<<56));
    mix64(a,b,c);
    k += 24; len -= 24;
  }

  /*------------------------------------- handle the last 23 bytes */
  c += length;
  switch(len)              /* all the case statements fall through */
  {
  case 23: c+=((ub8)k[22]<<56);
  case 22: c+=((ub8)k[21]<<48);
  case 21: c+=((ub8)k[20]<<40);
  case 20: c+=((ub8)k[19]<<32);
  case 19: c+=((ub8)k[18]<<24);
  case 18: c+=((ub8)k[17]<<16);
  case 17: c+=((ub8)k[16]<<8);
    /* the first byte of c is reserved for the length */
  case 16: b+=((ub8)k[15]<<56);
  case 15: b+=((ub8)k[14]<<48);
  case 14: b+=((ub8)k[13]<<40);
  case 13: b+=((ub8)k[12]<<32);
  case 12: b+=((ub8)k[11]<<24);
  case 11: b+=((ub8)k[10]<<16);
  case 10: b+=((ub8)k[ 9]<<8);
  case  9: b+=((ub8)k[ 8]);
  case  8: a+=((ub8)k[ 7]<<56);
  case  7: a+=((ub8)k[ 6]<<48);
  case  6: a+=((ub8)k[ 5]<<40);
  case  5: a+=((ub8)k[ 4]<<32);
  case  4: a+=((ub8)k[ 3]<<24);
  case  3: a+=((ub8)k[ 2]<<16);
  case  2: a+=((ub8)k[ 1]<<8);
  case  1: a+=((ub8)k[ 0]);
    /* case 0: nothing left to add */
  }
  mix64(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}

/*
--------------------------------------------------------------------
 This works on all machines, is identical to hash() on little-endian 
 machines, and it is much faster than hash(), but it requires
 -- that the key be an array of ub8's, and
 -- that all your machines have the same endianness, and
 -- that the length be the number of ub8's in the key
--------------------------------------------------------------------
*/
static INLINE ub8 hash2(register const ub8 *k, /* the key */
                        register ub4 length,   /* the length of the key */
                        register ub8 level)    /* the previous hash, or an arbitrary value */
{
  register ub8 a,b,c;
  ub4 len;

  /* Set up the internal state */
  len = length;
  a = b = level;                         /* the previous hash value */
  c = CONST64U(0x9e3779b97f4a7c13); /* the golden ratio; an arbitrary value */

  /*---------------------------------------- handle most of the key */
  while (len >= 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix64(a,b,c);
    k += 3; len -= 3;
  }

  /*-------------------------------------- handle the last 2 ub8's */
  c += length;
  switch(len)              /* all the case statements fall through */
  {
    /* c is reserved for the length */
  case  2: b+=k[1];
  case  1: a+=k[0];
    /* case 0: nothing left to add */
  }
  mix64(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}

/*
--------------------------------------------------------------------
 This is identical to hash() on little-endian machines, and it is much
 faster than hash(), but a little slower than hash2(), and it requires
 -- that all your machines be little-endian, for example all Intel x86
    chips or all VAXen.  It gives wrong results on big-endian machines.
--------------------------------------------------------------------
*/

static INLINE ub8 hash3(register const ub1 *k, /* the key */
                        register ub4 length,   /* the length of the key */
                        register ub8 level)    /* the previous hash, or an arbitrary value */
{
  register ub8 a,b,c;
  ub4 len;

  /* Set up the internal state */
  len = length;
  a = b = level;                         /* the previous hash value */
  c = CONST64U(0x9e3779b97f4a7c13); /* the golden ratio; an arbitrary value */

  /*---------------------------------------- handle most of the key */
  if (((size_t)k)&7)
  {
    while (len >= 24)
    {
      a += (k[0]        +((ub8)k[ 1]<< 8)+((ub8)k[ 2]<<16)+((ub8)k[ 3]<<24)
       +((ub8)k[4 ]<<32)+((ub8)k[ 5]<<40)+((ub8)k[ 6]<<48)+((ub8)k[ 7]<<56));
      b += (k[8]        +((ub8)k[ 9]<< 8)+((ub8)k[10]<<16)+((ub8)k[11]<<24)
       +((ub8)k[12]<<32)+((ub8)k[13]<<40)+((ub8)k[14]<<48)+((ub8)k[15]<<56));
      c += (k[16]       +((ub8)k[17]<< 8)+((ub8)k[18]<<16)+((ub8)k[19]<<24)
       +((ub8)k[20]<<32)+((ub8)k[21]<<40)+((ub8)k[22]<<48)+((ub8)k[23]<<56));
      mix64(a,b,c);
      k += 24; len -= 24;
    }
  }
  else
  {
    while (len >= 24)    /* aligned */
    {
      a += *(ub8 *)(k+0);
      b += *(ub8 *)(k+8);
      c += *(ub8 *)(k+16);
      mix64(a,b,c);
      k += 24; len -= 24;
    }
  }

  /*------------------------------------- handle the last 23 bytes */
  c += length;
  switch(len)              /* all the case statements fall through */
  {
  case 23: c+=((ub8)k[22]<<56);
  case 22: c+=((ub8)k[21]<<48);
  case 21: c+=((ub8)k[20]<<40);
  case 20: c+=((ub8)k[19]<<32);
  case 19: c+=((ub8)k[18]<<24);
  case 18: c+=((ub8)k[17]<<16);
  case 17: c+=((ub8)k[16]<<8);
    /* the first byte of c is reserved for the length */
  case 16: b+=((ub8)k[15]<<56);
  case 15: b+=((ub8)k[14]<<48);
  case 14: b+=((ub8)k[13]<<40);
  case 13: b+=((ub8)k[12]<<32);
  case 12: b+=((ub8)k[11]<<24);
  case 11: b+=((ub8)k[10]<<16);
  case 10: b+=((ub8)k[ 9]<<8);
  case  9: b+=((ub8)k[ 8]);
  case  8: a+=((ub8)k[ 7]<<56);
  case  7: a+=((ub8)k[ 6]<<48);
  case  6: a+=((ub8)k[ 5]<<40);
  case  5: a+=((ub8)k[ 4]<<32);
  case  4: a+=((ub8)k[ 3]<<24);
  case  3: a+=((ub8)k[ 2]<<16);
  case  2: a+=((ub8)k[ 1]<<8);
  case  1: a+=((ub8)k[ 0]);
    /* case 0: nothing left to add */
  }
  mix64(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}

#endif //_HASH_FUNC_H
