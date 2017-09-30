/*********************************************************
 * Copyright (C) 2006,2016 VMware, Inc. All rights reserved.
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
 * comport.c --
 *
 *      Simple COM1 port routines for debugging code that can't do any real
 *      host IO, such as the worldswitch and related.
 *
 *      They all wait for the last character to complete transmission so if the
 *      system crashes immediately on return, the last character will be seen
 *      by the remote end.
 *
 *      These routines do not have any external dependencies so can be called
 *      by any code that has privilege to access IO ports.
 *
 *      Under Windows, they can be made to forward output to DbgPrint for
 *      printing via the debugger.  Just have USE_DBGPRINT set to 1.  If you
 *      let USE_DBGPRINT be 0 with Windows, make sure the comport hardware is
 *      powered-up by leaving Hyperterm running with the comport open, else
 *      Windows will power the chip down.
 */

#include "comport.h"
#include "vm_basic_asm.h"  // for INB, OUTB

#if 000 // defined(_MSC_VER)
#define USE_DBGPRINT 1
#define USE_MACPORT80 0
#else
#define USE_DBGPRINT 0
#ifdef __APPLE__
#define USE_MACPORT80 1
#else
#define USE_MACPORT80 0
#endif
#endif

#if USE_DBGPRINT
void DbgPrint(char const *format, ...);
#elif !USE_MACPORT80
#define IOBASE 0x3F8  // COM1 base IO port number
#define BAUD 115200   // baud rate
#define THR 0         // transmitter holding register
#define LSR 5         // line status register
#define LSR_TE 0x20   // - transmit fifo completely empty
#define LSR_TI 0x40   // - transmitter idle
#endif


void
CP_Init(void)
{
#if !USE_DBGPRINT && !USE_MACPORT80
   OUTB(IOBASE+3, 0x83);               // LCR=select DLL/DLH, wordlen=8 bits
   OUTB(IOBASE+0, (115200/BAUD)&255);  // DLL=lo order baud rate
   OUTB(IOBASE+1, (115200/BAUD)/256);  // DLH=hi order baud rate
   OUTB(IOBASE+3, 0x03);               // LCR=select RBR/THR/IER
   OUTB(IOBASE+4, 0x07);               // MCR=dtr, rts, port-enable
   OUTB(IOBASE+2, 0x07);               // FCR=reset rcv fifo, reset xmt fifo
   OUTB(IOBASE+1, 0);                  // IER=disable all interrupts
#endif
}


void
CP_PutChr(uint8 ch) // IN
{
#if USE_DBGPRINT
   DbgPrint("%c", ch);
#elif USE_MACPORT80
   int bit;

   OUTB(0x80, (ch & 1) | 0x10);
   for (bit = 1; bit < 64; bit ++) {
      OUTB(0x80, (ch >> (bit & 7)) & 1);
   }
#else
   if (ch == '\n') CP_PutChr('\r');
   while ((INB(IOBASE+LSR) & LSR_TE) == 0) { }
   OUTB(IOBASE+THR, ch);
   while ((INB(IOBASE+LSR) & LSR_TI) == 0) { }
#endif
}


void
CP_PutDec(uint32 value) // IN
{
#if USE_DBGPRINT
   DbgPrint("%u", value);
#else
   char s[12];
   int i;

   i = 0;
   do {
      s[i++] = (value % 10) + '0';
      value /= 10;
   } while (value > 0);
   while (--i >= 0) CP_PutChr(s[i]);
#endif
}


void
CP_PutHexPtr(void *value) // IN
{
   if (sizeof value == 8) {
      CP_PutHex64((uint64)(VA)value);
   }
   if (sizeof value == 4) {
      CP_PutHex32((uint32)(VA)value);
   }
}


void
CP_PutHex64(uint64 value) // IN
{
   CP_PutHex32((uint32)(value >> 32));
   CP_PutHex32((uint32)value);
}


void
CP_PutHex32(uint32 value) // IN
{
#if USE_DBGPRINT
   DbgPrint("%8.8X", value);
#else
   CP_PutHex16((uint16)(value >> 16));
   CP_PutHex16((uint16)value);
#endif
}


void
CP_PutHex16(uint16 value) // IN
{
#if USE_DBGPRINT
   DbgPrint("%4.4X", value);
#else
   CP_PutHex8((uint8)(value >> 8));
   CP_PutHex8((uint8)value);
#endif
}


void
CP_PutHex8(uint8 value) // IN
{
#if USE_DBGPRINT
   DbgPrint("%2.2X", value);
#else
   CP_PutChr("0123456789ABCDEF"[(value>>4)&15]);
   CP_PutChr("0123456789ABCDEF"[value&15]);
#endif
}


void
CP_PutSp(void)
{
   CP_PutChr(' ');
}


void
CP_PutCrLf(void)
{
   CP_PutChr('\n');
}


void
CP_PutStr(char const *s) // IN
{
#if USE_DBGPRINT
   DbgPrint("%s", s);
#else
   char c;

   while ((c = *(s ++)) != 0) {
      CP_PutChr(c);
   }
#endif
}
