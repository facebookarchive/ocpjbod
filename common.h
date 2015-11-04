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
#include <assert.h>

#define OCP_SLOT_PER_ENCLOSURE 15

#define SAS_ADDR_LENGTH         8
#define SAS_ADDR_STR_LENGTH     (SAS_ADDR_LENGTH * 2 + 1)
#define MAX_PN_LENGTH           16
#define MAX_SN_LENGTH           16
#define MAX_TAG_LENGTH          16
#define MAX_FRU_COUNT           8
#define MAX_JBOD_PER_HOST       128
#define DEV_DISK_BY_SLOT        "/dev/disk/by-slot"


#define SAS_ADDR(name) unsigned char name[SAS_ADDR_LENGTH]
#define SAS_ADDR_STR(name)  char name[SAS_ADDR_STR_LENGTH];

#define PRODUCT_NUMBER(name) char name[MAX_PN_LENGTH]
#define SERIAL_NUMBER(name) char name[MAX_SN_LENGTH]
#define ASSET_TAG(name) char name[MAX_TAG_LENGTH]

enum {SAS_HDD, SATA_HDD} hdd_type;

void print_sas_addr_a(unsigned char *sas_addr, char *sas_addr_str);

int sas_addr_invalid(unsigned char *addr);

#ifndef NAME
#define NAME "ocpjbod"
#endif

#define perr(fmt, args...) fprintf(stderr, "%s: "fmt, NAME, ##args);

#endif
