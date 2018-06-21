/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

#define OCP_SLOT_PER_ENCLOSURE 15
#define TRITON_SLOT_PER_ENCLOSURE 36

#define SAS_ADDR_LENGTH         8
#define SAS_ADDR_STR_LENGTH     (SAS_ADDR_LENGTH * 2)
#define MAX_PN_LENGTH           16
#define MAX_SN_LENGTH           16
#define MAX_TAG_LENGTH          16
#define MAX_FRU_COUNT           8
#define MAX_JBOD_PER_HOST       128
/**
 * TODO: This needs to be bumped if we support multiple JBOFs
 *
 * That will require being able to detect which BMC USB networking
 * interface maps to a PCI expander.
 *
 * Until then, this is 1 so that if an old version of this tool
 * is run on a host with multiple expanders it will warn
 */
#define MAX_JBOF_PER_HOST       1
#define DEV_DISK_BY_SLOT        "/dev/disk/by-slot"


#define SAS_ADDR(name) unsigned char name[SAS_ADDR_LENGTH]
#define SAS_ADDR_STR(name)  char name[SAS_ADDR_STR_LENGTH + 1];

#define PRODUCT_NUMBER(name) char name[MAX_PN_LENGTH]
#define SERIAL_NUMBER(name) char name[MAX_SN_LENGTH]
#define ASSET_TAG(name) char name[MAX_TAG_LENGTH]

extern void print_sas_addr_a(unsigned char *sas_addr, char *sas_addr_str);

extern int sas_addr_invalid(unsigned char *addr);
extern void fix_none_ascii(char *buf, int len);

#ifndef NAME
#define NAME "ocpjbod"
#endif

#define perr(fmt, ...) do {                                     \
  fprintf(stderr, "%s: %s: %d: " fmt, NAME, __func__, __LINE__, \
    ##__VA_ARGS__); \
} while(0)

#endif
