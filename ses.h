/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef SES_H
#define SES_H

#include "common.h"
#include "array_device_slot.h"
#include "enclosure_info.h"
#include "expander.h"
#include "cooling.h"
#include "sensors.h"

/*
 * SES part of the management tool, defines interface
 */

#ifndef SG_SES_H   /* START: COPIED FROM SG3_UTIL */

/* Element Type codes */
#define UNSPECIFIED_ETC 0x0
#define DEVICE_ETC 0x1
#define POWER_SUPPLY_ETC 0x2
#define COOLING_ETC 0x3
#define TEMPERATURE_ETC 0x4
#define DOOR_LOCK_ETC 0x5
#define AUD_ALARM_ETC 0x6
#define ESC_ELECTRONICS_ETC 0x7
#define SCC_CELECTR_ETC 0x8
#define NV_CACHE_ETC 0x9
#define INV_OP_REASON_ETC 0xa
#define UI_POWER_SUPPLY_ETC 0xb
#define DISPLAY_ETC 0xc
#define KEY_PAD_ETC 0xd
#define ENCLOSURE_ETC 0xe
#define SCSI_PORT_TRAN_ETC 0xf
#define LANGUAGE_ETC 0x10
#define COMM_PORT_ETC 0x11
#define VOLT_SENSOR_ETC 0x12
#define CURR_SENSOR_ETC 0x13
#define SCSI_TPORT_ETC 0x14
#define SCSI_IPORT_ETC 0x15
#define SIMPLE_SUBENC_ETC 0x16
#define ARRAY_DEV_ETC 0x17
#define SAS_EXPANDER_ETC 0x18
#define SAS_CONNECTOR_ETC 0x19

#endif   /* END: COPIED FROM SG3_UTIL */

#define MAX_ELEMENT_TYPE_COUNT 32
#define MAX_COUNT_PER_ELEMENT 64
#define MAX_SES_PAGE_SIZE 8192
#define MAX_SES_PAGE_ID 16

#define ELEMENT_STATUS_UNSUPPORTED       0x0
#define ELEMENT_STATUS_OK                0x1
#define ELEMENT_STATUS_CRITICAL          0x2
#define ELEMENT_STATUS_NONCRITICAL       0x3
#define ELEMENT_STATUS_UNRECOVERABLE     0x4
#define ELEMENT_STATUS_NOT_INSTALLED     0x5
#define ELEMENT_STATUS_UNKNOWN           0x6
#define ELEMENT_STATUS_NOT_AVAILABLE     0x7
#define ELEMENT_STATUS_NO_ACCESS_ALLOWED 0x8
#define ELEMENT_STATUS_MASK              0xf

#define STATUS_CODE(x)  ((x) & ELEMENT_STATUS_MASK)


struct element_list {
  int element_type;
  int count;
};

/* all ses pages */
struct ses_pages {
  unsigned char page_zero[MAX_SES_PAGE_SIZE];
  unsigned char page_one[MAX_SES_PAGE_SIZE];
  unsigned char page_two[MAX_SES_PAGE_SIZE];
  unsigned char page_five[MAX_SES_PAGE_SIZE];
  unsigned char page_seven[MAX_SES_PAGE_SIZE];
  unsigned char page_a[MAX_SES_PAGE_SIZE];
};

/* all ses information */
struct ses_status_info {
  struct array_device_slot slots[MAX_COUNT_PER_ELEMENT];
  int slot_count;
  struct temperature_sensor temp_sensors[MAX_COUNT_PER_ELEMENT];
  int temp_count;
  struct voltage_sensor vol_sensors[MAX_COUNT_PER_ELEMENT];
  int vol_count;
  struct current_sensor curr_sensors[MAX_COUNT_PER_ELEMENT];
  int curr_count;
  struct cooling_fan fans[MAX_COUNT_PER_ELEMENT];
  int fan_count;
  struct sas_expander expander;
  struct enclosure_descriptor enclosure;
  struct enclosure_control enclosure_control;
};

extern int interpret_ses_pages(
  struct ses_pages *pages,
  struct ses_status_info *ses_info);

extern int read_ses_pages(int sg_fd, struct ses_pages *pages,
                           int *page_two_size);

/* read ses page; return errno and provide number of bytes read in count */
extern int sg_read_ses_page(int sg_fd, int page_code, unsigned char *buf,
                            int buf_size, int *count);

/* send a ses page */
extern int sg_send_ses_page(int sg_fd, unsigned char *buf, int size);

/* validate additional element with eip == 1 and protocol == SAS */
extern void verify_additional_element_eip_sas (
  unsigned char *additional_element);

/* copy element description from buffer to a string */
extern char *copy_description(unsigned char *description);

#endif
