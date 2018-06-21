/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include <sys/stat.h>

#include "ses.h"
#include "cooling.h"
#include "sensors.h"
#include "expander.h"
#include "array_device_slot.h"


static int check_page_zero(unsigned char *page_zero)
{
  char supported_pages[MAX_SES_PAGE_ID] = {0};
  const unsigned char required_pages[] = {0, 1, 2, 5, 7, 0xa, 0xe};
  int i;

  for (i = 0; i < page_zero[3]; i++) {
    if (page_zero[4 + i] < MAX_SES_PAGE_ID) {
      supported_pages[page_zero[4 + i]] = 1;
    }
  }

  for (i = 0; i < sizeof(required_pages); i++) {
    if (supported_pages[required_pages[i]] == 0) {
      perr(
          "Error: the system does not support required page %d\n",
          (int)required_pages[i]);
      return 1;
    }
  }
  return 0;
}

static int
validate_ses_page(int page_code, unsigned char* page_buf) {
  if (page_code != page_buf[0]) {
    perr(
        "Error reading page 0x%x: expected 0x%x in byte 0, got 0x%x\n",
        page_code,
        page_code,
        page_buf[0]);
      return 1;
  }
  if (page_code == 0x0) {
    return check_page_zero(page_buf);
  }

  return 0;
}

int sg_read_ses_page(int sg_fd, int page_code, unsigned char *buf,
                     int buf_size, int *count)
{
  int ret;
  ret = sg_ll_receive_diag(sg_fd, 1 /* pcv */, page_code, buf,
                           buf_size, 1, 0);

  if (0 == ret) {
    *count = (buf[2] << 8) + buf[3] + 4;
    ret = validate_ses_page(page_code, buf);
  } else {
    perr(
        "Error reading page 0x%x, sg_ll_receive_diag returned: %d\n",
        page_code,
        ret);
    *count = 0;
    ret = EINVAL;
  }

  return ret;
}

int sg_send_ses_page(int sg_fd, unsigned char *buf, int size)
{
  return sg_ll_send_diag(sg_fd, 0 /* sf_code */, 1, 0 /* sf_bit */,
                         0 /* devofl_bit */, 0 /* unitofl_bit */,
                         0 /* long_duration */, buf, size,
                         1 /* noisy */, 0 /* verbose */);
}

/*
 * return list of elements in ses page 0x02
 */
int extract_element_list(
  unsigned char *ses_page_one,
  struct element_list *elem_list,
  int *element_type_count)
{
  int count = ses_page_one[10];
  int i;
  int page_one_index;

  page_one_index = 12 + ses_page_one[11];

  *element_type_count = count;

  for (i = 0; i < count; i++) {
    elem_list[i].element_type = ses_page_one[page_one_index + 4 * i];
    elem_list[i].count = ses_page_one[page_one_index + 4 * i + 1];
  }

#ifdef DEBUG
  for (i = 0; i < element_type_count; i ++) {
    printf("0x%x, %d\n", elem_list[i].element_type, elem_list[i].count);
  }
#endif

  return 0;
}

int read_ses_pages(int sg_fd, struct ses_pages *pages,
                    int *page_two_size)
{
  int size = 0;
  int rc = 0;
  assert(sg_fd > 0);

  rc = sg_read_ses_page(sg_fd, 0x0, pages->page_zero, MAX_SES_PAGE_SIZE, &size);
  if (0 != rc) {
    return rc;
  }

  rc = sg_read_ses_page(sg_fd, 0x1, pages->page_one, MAX_SES_PAGE_SIZE, &size);
  if (0 != rc) {
    return rc;
  }

  rc = sg_read_ses_page(sg_fd, 0x2, pages->page_two, MAX_SES_PAGE_SIZE, &size);
  if (0 != rc) {
    return rc;
  }
  if (page_two_size)
    *page_two_size = size;

  rc = sg_read_ses_page(sg_fd, 0x5, pages->page_five, MAX_SES_PAGE_SIZE, &size);
  if (0 != rc) {
    return rc;
  }

  rc =
      sg_read_ses_page(sg_fd, 0x7, pages->page_seven, MAX_SES_PAGE_SIZE, &size);
  if (0 != rc) {
    return rc;
  }

  rc = sg_read_ses_page(sg_fd, 0xa, pages->page_a, MAX_SES_PAGE_SIZE, &size);
  if (0 != rc) {
    return rc;
  }

  return rc;
}

int interpret_ses_pages(
  struct ses_pages *pages,
  struct ses_status_info *ses_info)
{
  int page_two_index = 8;
  int page_five_index = 8;
  int page_seven_index = 8;
  int page_a_index = 8;
  int i, j;
  struct stat file_stat;

  struct element_list elem_list[MAX_ELEMENT_TYPE_COUNT];
  int element_type_count = 0;

  extract_element_list(pages->page_one, elem_list, &element_type_count);

  for (i = 0; i < element_type_count; i ++) {
      if (elem_list[i].element_type == ARRAY_DEV_ETC) {
        ses_info->slot_count = elem_list[i].count;
      } else if (elem_list[i].element_type == SAS_CONNECTOR_ETC) {

      } else if (elem_list[i].element_type == COOLING_ETC) {
        ses_info->fan_count = elem_list[i].count;
      } else if (elem_list[i].element_type == TEMPERATURE_ETC){
        ses_info->temp_count = elem_list[i].count;
      } else if (elem_list[i].element_type == VOLT_SENSOR_ETC){
        ses_info->vol_count = elem_list[i].count;
      } else if (elem_list[i].element_type == CURR_SENSOR_ETC){
        ses_info->curr_count = elem_list[i].count;
      } else if (elem_list[i].element_type == SAS_EXPANDER_ETC){

      }
  }

  for (i = 0; i < element_type_count; i ++) {
    /* skip overall status */
    page_two_index += 4;
    page_five_index += 4;
    page_seven_index += pages->page_seven[page_seven_index + 3] + 4;

    for (j = 0; j < elem_list[i].count; j ++) {
      switch (elem_list[i].element_type) {
        case ARRAY_DEV_ETC:
          assert(elem_list[i].count == OCP_SLOT_PER_ENCLOSURE ||
                 elem_list[i].count == TRITON_SLOT_PER_ENCLOSURE);
          assert((pages->page_a[page_a_index] & 0x10) == 0x10);
          extract_array_device_slot_info(
            pages->page_two + page_two_index,
            pages->page_a + page_a_index,
            pages->page_seven + page_seven_index,
            page_two_index,
            ses_info->slots + j);
          page_a_index += pages->page_a[page_a_index + 1] + 2;
          break;
        case SAS_CONNECTOR_ETC:
          break;
        case COOLING_ETC:
          extract_cooling_fan_info(
            pages->page_two + page_two_index,
            pages->page_seven + page_seven_index,
            ses_info->fans + j,
            page_two_index);
          break;
        case TEMPERATURE_ETC:
          extract_temperature_sensor_info(
            pages->page_two + page_two_index,
            pages->page_seven + page_seven_index,
            pages->page_five + page_five_index,
            ses_info->temp_sensors + j);
          break;
        case VOLT_SENSOR_ETC:
          extract_voltage_sensor_info(
            pages->page_two + page_two_index,
            pages->page_seven + page_seven_index,
            pages->page_five + page_five_index,
            ses_info->vol_sensors + j);
          break;
        case CURR_SENSOR_ETC:
          extract_current_sensor_info(
            pages->page_two + page_two_index,
            pages->page_seven + page_seven_index,
            pages->page_five + page_five_index,
            ses_info->curr_sensors + j);
          break;
        case SAS_EXPANDER_ETC:
          extract_expander_info(
            pages->page_two,
            pages->page_a + page_a_index,
            pages->page_seven + page_seven_index,
            &(ses_info->expander),
            ses_info->slots);
          page_a_index += pages->page_a[page_a_index + 1] + 2;
          break;
        case ENCLOSURE_ETC:
          extract_enclosure_control(
            pages->page_two, page_two_index, &(ses_info->enclosure_control));
          break;
        case ESC_ELECTRONICS_ETC:
          page_a_index += pages->page_a[page_a_index + 1] + 2;
          break;
        default:
          break;
      }

      /* move to next element */
      page_two_index += 4;
      page_five_index += 4;
      page_seven_index += pages->page_seven[page_seven_index + 3] + 4;
    }
  }

  if (stat(DEV_DISK_BY_SLOT, &file_stat) != 0)
    mkdir(DEV_DISK_BY_SLOT, 0755);
  for (i = 0; i < ses_info->slot_count; i ++)
    find_dev_name(ses_info->slots + i, ses_info->expander.sas_addr_str);

#ifdef DEBUG
  for (i = 0; i < ses_info->slot_count; i ++)
    print_array_device_slot(ses_info->slots + i);
  for (i = 0; i < ses_info->fan_count; i ++)
    print_cooling_fan(ses_info->fans + i);
  for (i = 0; i < ses_info->temp_count; i ++)
    print_temperature_sensor(ses_info->temp_sensors + i);
  for (i = 0; i < ses_info->vol_count; i ++)
    print_volatage_sensor(ses_info->vol_sensors + i);
  for (i = 0; i < ses_info->curr_count; i ++)
    print_current_sensor(ses_info->curr_sensors + i);
  print_expander_info(&(ses_info->expander));
#endif  /* DEBUG */

  return 0;
}

void verify_additional_element_eip_sas (
  unsigned char *additional_element)
{
  // assert EIP == 1
  assert((additional_element[0] & 0x10) == 0x10);

  // assert protocol identifier says SAS
  assert((additional_element[0] & 0x0f) == 0x6);
}

char *copy_description(unsigned char *description)
{
  int len = description[3];
  char *name = (char *) malloc(len + 1);
  memcpy(name, description + 4, len);
  name[len] = '\0';
  fix_none_ascii(name, len);
  return name;
}
