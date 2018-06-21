/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "array_device_slot.h"
#include "common.h"
#include "json.h"
#include "ses.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char* status_str(struct array_device_slot slot) {
  if ((slot.common_status & 0xf) == 0x5) {
    return "Not Installed";
  } else if (slot.device_off) {
    return "Power Off";
  } else {
    return "Power On";
  }
}

const char *fault_led_status_str(int fault_code) {
  const char *fault_led_status[] = {"None", "Requested", "Sensed", "Sensed"};
  return fault_led_status[fault_code];
}

void print_array_device_slot(struct array_device_slot *slot)
{
  const char *device_status_str = status_str(*slot);

  IF_PRINT_NONE_JSON {
    printf("%s Slot:\t%d\tPhy: %d\t%s\tSAS Addr: 0x%s\t"
           "Fault: %s",
           slot->name, slot->slot, slot->phy, device_status_str,
           slot->sas_addr_str, fault_led_status_str(slot->fault));
    if (slot->dev_name) {
      printf("\t%s", slot->dev_name);
      printf("\t%s", slot->by_slot_name);
    }
    printf("\n");
  }

  PRINT_JSON_GROUP_HEADER(slot->name);
  PRINT_JSON_ITEM("name", "%s", slot->name);
  PRINT_JSON_ITEM("slot", "%d", slot->slot);
  PRINT_JSON_ITEM("phy", "%d", slot->phy);
  PRINT_JSON_ITEM("status", "%s", device_status_str);
  PRINT_JSON_ITEM("sas_addr", "0x%s", slot->sas_addr_str);
  if (slot->dev_name) {
    PRINT_JSON_ITEM("fault", "%s", fault_led_status_str(slot->fault));
    PRINT_JSON_ITEM("devname", "%s", slot->dev_name);
    PRINT_JSON_LAST_ITEM("by_slot_name", "%s", slot->by_slot_name);
  } else
    PRINT_JSON_LAST_ITEM("fault", "%s", fault_led_status_str(slot->fault));

  PRINT_JSON_GROUP_ENDING;
}

int extract_array_device_slot_info(
  unsigned char *array_device_slot_element,
  unsigned char *additional_slot_element,
  unsigned char *slot_description,
  int page_two_offset,
  struct array_device_slot *slot)
{
  verify_additional_element_eip_sas(additional_slot_element);

  slot->common_status = array_device_slot_element[0];
  slot->slot = (int) additional_slot_element[3];
  slot->phy = -1;  /* update phy later in the expander info */
  slot->fault = (array_device_slot_element[3] & 0x60) >> 5;
  slot->ident = array_device_slot_element[2] & 0x02 ? 1 : 0;
  slot->active = 0;
  slot->device_off = array_device_slot_element[3] & 0x10 ? 1 : 0;

  memcpy(slot->sas_addr, additional_slot_element + 20, 8);
  print_sas_addr_a(slot->sas_addr, slot->sas_addr_str);

  slot->name = copy_description(slot_description);

  slot->page_two_offset = page_two_offset;
  slot->dev_name = NULL;
  slot->by_slot_name = NULL;
  return 0;
}

int control_hdd_power(
  unsigned char *page_two,
  struct array_device_slot *slot,
  int op /* 0 for power off; 1 for power on */)
{
  slot->common_control = slot->common_status & 0xf0;
  slot->common_control |= 0x80;

  page_two[slot->page_two_offset] = slot->common_control;
  if (op) {
    page_two[slot->page_two_offset + 3] &= 0xef;
  } else {
    page_two[slot->page_two_offset + 3] |= 0x10;
  }
  return 0;
}

/* return whether the HDD is powered on */
int check_hdd_power(
  unsigned char *page_two,
  struct array_device_slot *slot)
{
  /* this bit is DEVICE_OFF, so we need to reverse it */
  return !(page_two[slot->page_two_offset + 3] & 0x10);
}

int control_hdd_led_fault(
  unsigned char *page_two,
  struct array_device_slot *slot,
  int op  /* 0 for clear fault request; 1 for request fault */)
{
  slot->common_control = slot->common_status & 0xf0;
  slot->common_control |= 0x80;

  page_two[slot->page_two_offset] = slot->common_control;
  if (op) {
    page_two[slot->page_two_offset + 3] |= 0x20;
  } else {
    page_two[slot->page_two_offset + 3] &= 0xdf;
  }
  return 0;
}

int find_dev_name(
  struct array_device_slot *slot,
  char *expander_addr)
{
  char link_name[PATH_MAX];
  char real_name[PATH_MAX];
  DIR *dir;
  struct dirent *ent;
  char *sys_block = "/sys/block";
  char sas_address_filename[PATH_MAX];
  int sas_address_fd;
  char sas_address_file_content[4096];
  struct stat st;
  int read_len;

  snprintf(link_name, PATH_MAX, "%s/enclosure-0x%s-slot%d",
           DEV_DISK_BY_SLOT, expander_addr, slot->slot);

  unlink(link_name);

  if (sas_addr_invalid(slot->sas_addr))
    return 0;

  dir = opendir(sys_block);
  if (dir == NULL)
    return 0;
  while ((ent = readdir(dir)) != NULL) {
    if (strncmp(ent->d_name, "sd", 2))  /* only check sd devices */
      continue;

    snprintf(sas_address_filename, PATH_MAX, "%s/%s/device/sas_address",
             sys_block, ent->d_name);
    if (access(sas_address_filename, R_OK) != 0)
      continue;
    if ((sas_address_fd = open(sas_address_filename, O_RDONLY)) < 0)
      continue;

    read_len = read(sas_address_fd, sas_address_file_content, 4096);
    close(sas_address_fd);

    if (read_len < SAS_ADDR_LENGTH * 2 + 2)
      continue;
    if (strncmp(sas_address_file_content + 2,
                slot->sas_addr_str, SAS_ADDR_LENGTH * 2) == 0) {
      snprintf(real_name, PATH_MAX, "/dev/%s", ent->d_name);
      if ((stat(real_name, &st) == 0) &&
          ((st.st_mode & S_IFMT) == S_IFBLK)) {
        slot->dev_name = strndup(real_name, PATH_MAX);
        if (symlink(real_name, link_name) == 0)
          slot->by_slot_name = strndup(link_name, PATH_MAX);
      }
      break;
    }
  }
  closedir(dir);

  return 0;
}
