/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef ARRAY_DEVICE_SLOT_H
#define ARRAY_DEVICE_SLOT_H

#include "common.h"

/*
 * full information pulled for array_device_slot,
 * including info from SES page 0x02, 0x07, 0x0a
 */
struct array_device_slot {
  unsigned char common_status;          /* for status only */
  unsigned char common_control;         /* for control only */
  int slot;
  int phy;
  int fault;            /* bit 0 for fault requested, bit 1 for fault sensed */
  int ident;
  int active;
  int device_off;
  SAS_ADDR(sas_addr);
  SAS_ADDR_STR(sas_addr_str);
  char *name;             /* name in SES page */
  char *dev_name;         /* OS dev name */
  char *by_slot_name;     /* OS dev name in /dev/disk/by-slot */
  int page_two_offset;     /* for control */
};

/* human-readable form of array_device_slot.common_status */
extern const char* fault_led_status_str(int);

/* human-readable status */
extern const char* status_str(struct array_device_slot);

extern void print_array_device_slot(struct array_device_slot *slot);

extern int extract_array_device_slot_info(
  unsigned char *array_device_slot_element,
  unsigned char *additional_slot_element,
  unsigned char *slot_description,
  int page_two_offset,
  struct array_device_slot *slotp);

extern int control_hdd_power(
  unsigned char *page_two,
  struct array_device_slot *slot,
  int op /* 0 for power off; 1 for power on */);

extern int control_hdd_led_fault(
  unsigned char *page_two,
  struct array_device_slot *slot,
  int op /* 0 for clear fault request; 1 for request fault */);

extern int check_hdd_power(
  unsigned char *page_two,
  struct array_device_slot *slot);

/* find OS dev name */
extern int find_dev_name(
  struct array_device_slot *slot,
  char *expander_addr);

#endif
