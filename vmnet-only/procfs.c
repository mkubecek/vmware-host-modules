/*********************************************************
 * Copyright (C) 1998-2013, 2020 VMware, Inc. All rights reserved.
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

#include "driver-config.h"

#define EXPORT_SYMTAB

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include "compat_skbuff.h"
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include "compat_sock.h"

#define __KERNEL_SYSCALLS__
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/file.h>

#include "vnetInt.h"


#if defined(CONFIG_PROC_FS)

static int VNetProcMakeEntryInt(VNetProcEntry *parent, char *name, int mode,
                                void *data, VNetProcReadFn *fn,
                                VNetProcEntry **ret);
static void VNetProcRemoveEntryInt(VNetProcEntry *node, VNetProcEntry *parent);

static VNetProcEntry *base = NULL;


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_Init --
 *
 *      Initialize the vnets procfs entries.
 *
 * Results: 
 *      errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetProc_Init(void)
{
   return VNetProcMakeEntryInt(NULL, "vmnet", S_IFDIR, NULL, NULL, &base);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_Cleanup --
 *
 *      Cleanup the vnets proc filesystem entries.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VNetProc_Cleanup(void)
{
   VNetProcRemoveEntryInt(base, NULL);
   base = NULL;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
/*
 *----------------------------------------------------------------------
 *
 * VNetProcShow --
 *
 *      Show the contents of this procfs node.  We bounce through this
 *      into the read function callback that was given to us when the
 *      entry was created.
 *
 * Results:
 *      errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VNetProcShow(struct seq_file *p, // IN:
             void *v)            // IN:
{
   char *buf = (char *)__get_free_page(GFP_KERNEL);
   if (buf != NULL) {
      VNetProcEntry *ent = p->private;
      char *start;
      int eof;
      buf[ent->fn(buf, &start, 0, PAGE_SIZE, &eof, ent->data)] = '\0';
      seq_printf(p, buf);
      free_page((unsigned long)buf);
      return 0;
   }
   return -ENOMEM;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
/*
 *----------------------------------------------------------------------
 *
 * VNetProcOpen --
 *
 *      Open a procfs node.
 *
 * Results:
 *      errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VNetProcOpen(struct inode *inode,   // IN:
             struct file *file)     // IN:
{
   return single_open(file, VNetProcShow, PDE_DATA(inode));
}

/* Our procfs callbacks.  We only need to specialize open. */
static struct file_operations fops = {
   .open    = VNetProcOpen,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release,
};
#endif
#endif


/*
 *----------------------------------------------------------------------
 *
 * VNetProcMakeEntryInt --
 *
 *      Make an entry in the vnets proc file system.
 *
 * Results:
 *      errno. If errno is 0 and then ret is filled in with the
 *      resulting proc entry.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetProcMakeEntryInt(VNetProcEntry   *parent,   // IN:
                     char            *name,     // IN:
                     int              mode,     // IN:
                     void            *data,     // IN:
                     VNetProcReadFn  *fn,       // IN:
                     VNetProcEntry  **ret)      // OUT:
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
   VNetProcEntry *ent = kmalloc(sizeof *ent, GFP_KERNEL);
   if (ent != NULL) {
      if (mode & S_IFDIR) {
         ent->pde    = proc_mkdir(name, NULL);
      } else {
         ent->data   = data;
         ent->fn     = fn;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
         ent->pde    = proc_create_single_data(name, mode, parent->pde,
                                               VNetProcShow, ent);
#else
         ent->pde    = proc_create_data(name, mode, parent->pde, &fops, ent);
#endif
      }
      if (ent->pde != NULL) {
         *ret = ent;
         return 0;
      }
      kfree(ent);
   }
   return -ENOMEM;
#else
   VNetProcEntry *ent = create_proc_entry(name, mode, parent);
   if (ent != NULL) {
      ent->data      = data;
      ent->read_proc = fn;
      *ret           = ent;
      return 0;
   }
   return -ENOMEM;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProcRemoveEntryInt --
 *
 *      Remove a previously installed proc entry.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VNetProcRemoveEntryInt(VNetProcEntry *node,     // IN:
                       VNetProcEntry *parent)   // IN:
{
   if (node != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
      proc_remove(node->pde);
      kfree(node);
#else
      remove_proc_entry(node->name, parent);
#endif
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_MakeEntry --
 *
 *      Make an entry in the vnets proc file system.
 *
 * Results: 
 *      errno. If errno is 0 and ret is non NULL then ret is filled
 *      in with the resulting proc entry.
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetProc_MakeEntry(char              *name,  // IN:
                   int                mode,  // IN:
                   void              *data,  // IN:
                   VNetProcReadFn    *fn,    // IN:
                   VNetProcEntry    **ret)   // OUT:
{
   return VNetProcMakeEntryInt(base, name, mode, data, fn, ret);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_RemoveEntry --
 *
 *      Remove a previously installed proc entry.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VNetProc_RemoveEntry(VNetProcEntry *node)
{
   VNetProcRemoveEntryInt(node, base);
}


#else /* CONFIG_PROC_FS */


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_Init --
 *
 *      Initialize the vnets procfs entries.
 *
 * Results: 
 *      errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetProc_Init(void)
{
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_Cleanup --
 *
 *      Cleanup the vnets proc filesystem entries.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VNetProc_Cleanup(void)
{
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_MakeEntry --
 *
 *      Make an entry in the vnets proc file system.
 *
 * Results: 
 *      errno. If errno is 0 and ret is non NULL then ret is filled
 *      in with the resulting proc entry.
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetProc_MakeEntry(char              *name,
                   int                mode,
                   void              *data,
                   VNetProcReadFn    *fn,
                   VNetProcEntry    **ret)
{
   return -ENXIO;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetProc_RemoveEntry --
 *
 *      Remove a previously installed proc entry.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VNetProc_RemoveEntry(VNetProcEntry *parent)
{
}

#endif /* CONFIG_PROC_FS */
