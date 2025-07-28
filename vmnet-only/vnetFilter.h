/*********************************************************
 * Copyright (C) 2006-2014 VMware, Inc. All rights reserved.
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
 * vnetFilter.h --
 *
 *      This file defines the external interface provided
 *      by the vmnet driver for host packet filter 
 *      functionality.  This functionality is likely to
 *      be eventually moved to a separate driver.
 *
 */

#ifndef _VNETFILTER_H_
#define _VNETFILTER_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_types.h"

/*
 * Call:
 *      Windows vmnet driver using IOCTL_VNET_FILTERHOST2.
 *      Linux vmnet driver using SIOCSFILTERRULES.
 */


/* list of subcommands for the host filter ioctl() call */
#define VNET_FILTER_CMD_MIN                 0x1000 /* equal to smallest sub-command */
#define VNET_FILTER_CMD_CREATE_RULE_SET     0x1000
#define VNET_FILTER_CMD_DELETE_RULE_SET     0x1001
#define VNET_FILTER_CMD_ADD_IPV4_RULE       0x1002
#define VNET_FILTER_CMD_ADD_IPV6_RULE       0x1003 /* not implemented */
#define VNET_FILTER_CMD_CHANGE_RULE_SET     0x1004
#define VNET_FILTER_CMD_SET_LOG_LEVEL       0x1005
#define VNET_FILTER_CMD_MAX                 0x1005 /* equal to largest sub-command */

/* action for a rule or rule set */
/* VNet_CreateRuleSet.defaultAction */
/* VNet_AddIPv4Rule.action */
/* VNet_ChangeRuleSet.defaultAction */
#define VNET_FILTER_RULE_NO_CHANGE  0x2000
#define VNET_FILTER_RULE_BLOCK      0x2001
#define VNET_FILTER_RULE_ALLOW      0x2002

/* direction that should apply to a rule */
/* VNet_AddIPv4Rule.direction */
#define VNET_FILTER_DIRECTION_IN   0x3001
#define VNET_FILTER_DIRECTION_OUT  0x3002
#define VNET_FILTER_DIRECTION_BOTH 0x3003

/* used to change which rule set is used for host filtering */
/* VNet_ChangeRuleSet.activate */
#define VNET_FILTER_STATE_NO_CHANGE 0x4000
#define VNET_FILTER_STATE_ENABLE    0x4001
#define VNET_FILTER_STATE_DISABLE   0x4002

/* log Levels, cut and paste from bora/lib/public/policy.h */
#define VNET_FILTER_LOGLEVEL_NONE    (0)
#define VNET_FILTER_LOGLEVEL_TERSE   (1)
#define VNET_FILTER_LOGLEVEL_NORMAL  (2)
#define VNET_FILTER_LOGLEVEL_VERBOSE (3)
#define VNET_FILTER_LOGLEVEL_MAXIMUM (4)

/* header that's common for all command structs */
#pragma pack(push, 1)
typedef struct VNet_RuleHeader {
   uint32 type;   /* type of struct */
   uint32 ver;    /* version of struct */
   uint32 len;    /* length of struct */
} VNet_RuleHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VNet_CreateRuleSet {
   VNet_RuleHeader header; /* type = VNET_FILTER_CMD_CREATE_RULE_SET, ver = 1,
                              len = sizeof(VNet_CreateRuleSet) */

   uint32 ruleSetId;       /* id of rule to delete (must be non-0) */
   uint32 defaultAction;   /* VNET_FILTER_RULE_DROP or VNET_FILTER_RULE_PERMIT */
} VNet_CreateRuleSet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VNet_DeleteRuleSet {
   VNet_RuleHeader header; /* type = VNET_FILTER_CMD_DELETE_RULE_SET, ver = 1,
                              len = sizeof(VNet_DeleteRuleSet) */

   uint32 ruleSetId;       /* rule set to delete (from VNet_CreateRuleSet.ruleSetId) */
} VNet_DeleteRuleSet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VNet_AddIPv4Rule {
   VNet_RuleHeader header; /* type = VNET_FILTER_CMD_ADD_IPV4_RULE, ver = 1,
                              len = sizeof(VNet_AddIPv4Rule) +
                              addrListLen  * sizeof(VNet_IPv4Address) +
                              protoListLen * sizeof(VNet_IPv4Protocol) */

   uint32 ruleSetId;       /* rule set (from VNet_CreateRuleSet.ruleSetId) */
   uint32 action;          /* VNET_FILTER_RULE_DROP or VNET_FILTER_RULE_PERMIT */
   uint32 direction;       /* VNET_FILTER_DIRECTION_IN, VNET_FILTER_DIRECTION_OUT, or
                              VNET_FILTER_DIRECTION_BOTH */

   uint32 addressListLen;  /* Number of VNet_IPv4Address's that follow.
                              Must be at least one.  Must equal 1 if addr==mask==0.
                              expected but not required: (addr & ~mask) == 0 */

   uint32 proto;           /* ~0 is don't care, otherwise protocol in IP header*/

   uint32 portListLen;     /* Number of VNet_IPv4Port's that follow the
                              VNet_IPv4Address's.  Ports currently only apply for
                              TCP and UDP.  Must be at least one, even if non-TCP or
                              non-UDP protocol is specified in 'proto' (use 0 or ~0 for
                              all elements in VNet_IPv4Port).  Must equal 1 if all
                              elements in a VNet_IPv4Port are ~0. */
   /* add flags for tracking in which direction the connection is established? */
} VNet_AddIPv4Rule;
#pragma pack(pop)

/*
 * VNet_AddIPv4Rule is immediately followed by 1 or more VNet_IPv4Address.
 * The last VNet_IPv4Address is immediately followed by 1 or more VNet_IPv4Port.
 */

#pragma pack(push, 1)
typedef struct VNet_IPv4Address {
   /* currently no fields for local address/mask (add them?) */

   /* can specify don't care on IP address via addr==mask==0,
      but only for a list with 1 item */
   uint32 ipv4RemoteAddr; /* remote entity's address (dst on outbound, src on inbound) */
   uint32 ipv4RemoteMask; /* remote entity's mask    (dst on outbound, src on inbound) */
} VNet_IPv4Address;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VNet_IPv4Port {
   /* can specify ~0 for all 4 only if one item in the list */

   uint32 localPortLow;    /* ~0 is don't care, otherwise low  local range (inclusive) */
   uint32 localPortHigh;   /* ~0 is don't care, otherwise high local range (inclusive) */
   uint32 remotePortLow;   /* ~0 is don't care, otherwise low  remote range (inclusive) */
   uint32 remotePortHigh;  /* ~0 is don't care, otherwise high remote range (inclusive) */
} VNet_IPv4Port;
#pragma pack(pop)

// typedef struct VNet_IPv4Port VNet_IPv6Port;

#pragma pack(push, 1)
typedef struct VNet_ChangeRuleSet {
   VNet_RuleHeader header; /* type = VNET_FILTER_CMD_CHANGE_RULE_SET, ver = 1,
                              len = sizeof(VNet_ChangeRuleSet) */

   uint32 ruleSetId;       /* rule set (from VNet_CreateRuleSet.ruleSetId) */
   uint32 defaultAction;   /* usually VNET_FILTER_RULE_NO_CHANGE, but can change default
                              rule via VNET_FILTER_RULE_DROP or VNET_FILTER_RULE_PERMIT */
   uint32 activate;        /* specify rule to use for filtering via
                              VNET_FILTER_STATE_ENABLE or VNET_FILTER_STATE_DISABLE.
                              Can use VNET_FILTER_STATE_NO_CHANGE to change only the
                              default rule of the rule set */
} VNet_ChangeRuleSet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VNet_SetLogLevel {
   VNet_RuleHeader header; /* type = VNET_FILTER_CMD_SET_LOG_LEVEL, */
                           /* ver = 1,                              */
                           /* len = sizeof(VNet_SetLogLevel)        */
   uint32 logLevel;        /* the log level to set                  */
} VNet_SetLogLevel;
#pragma pack(pop)

#endif // ifndef _VNETFILTER_H_
