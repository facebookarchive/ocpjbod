/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

#include "jbod_interface.h"
#include "scsi_buffer.h"
#include "ses.h"
#include "led.h"
#include "json.h"
#include "drive_control.h"

#include "knox.c"
#include "triton.c"

#define SCSI_BUFFER_MAX_SIZE 64

struct jbod_profile jbod_library[] = {
  {"facebook", "Knox2U          ", "0e20", "", &knox, "Knox"},
  {"wiwynn  ", "HB2U            ", "0408", "", &honeybadger, "HoneyBadger"},
  {"wiwynn  ", "TN4U            ", "", "", &triton, "BryceCanyon"},
  {"wiwynn  ", "BC4U            ", "", "", &triton, "BryceCanyon"},
};

int library_size = sizeof(jbod_library) / sizeof(struct jbod_profile);

static int inquiry_would_block(int sg_fd, const char *devname) {
  int accessibility_res = -1;
  int command = SG_SCSI_RESET_NOTHING;
  int count = 0;

  /* bsg device does not support SG_SCSI_RESET,
     so we only do SG_SCSI_RESET_NOTHING check for sg device */
  if (strstr(devname, "/dev/sg") == NULL)
    return 0;

  for (count = 0; count < 10; ++count) {
    accessibility_res = ioctl(sg_fd, SG_SCSI_RESET, &command);
    if (accessibility_res == 0) {
#ifdef DEBUG
      if (count > 0)
        perr("Retried SG_SCSI_RESET %d times.\n", count);
#endif
      return 0;
    }
    sleep(1);
  }
#ifdef DEBUG
  perr("Device %s is inaccessible\n", devname);
#endif
  return 1;
}

struct jbod_profile *extract_profile(const char *devname)
{
  struct jbod_profile *profile;
  char buff[INQUIRY_RESP_INITIAL_LEN];
  int sg_fd;

  profile = NULL;

  sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
  if (sg_fd < 0) {
#ifdef DEBUG
    perr("Cannot open %s.\n", devname);
#endif
    return NULL;
  }

  if (inquiry_would_block(sg_fd, devname)) {
    goto done;
  }

  if (sg_ll_inquiry(sg_fd, 0, 0, 0, buff, sizeof(buff), 0, 0)) {
#ifdef DEBUG
    perr("Cannot inquiry %s.\n", devname);
#endif
    goto done;
  }

  profile = (struct jbod_profile *)calloc(1, sizeof(struct jbod_profile));
  if (profile == NULL) {
    goto done;
  }

  memcpy(profile->vendor, &buff[8], INQUIRY_VENDOR_LEN);
  memcpy(profile->product, &buff[16], INQUIRY_PRODUCT_LEN);
  memcpy(profile->revision, &buff[32], INQUIRY_REVISION_LEN);
  memcpy(profile->specific, &buff[36], INQUIRY_SPECIFIC_LEN);

  if (profile->specific[0] < 0x20) {
    profile->specific[0] = '\0';
  }

done:
  sg_cmds_close_device(sg_fd);
  return profile;
}

struct jbod_interface *detect_dev(const char *devname)
{
  struct jbod_profile *profile;
  int i;

  profile = extract_profile(devname);
  if (profile == NULL) {
    return NULL;
  }

  for (i = 0; i < library_size; i ++) {
    if (strcmp(profile->vendor, jbod_library[i].vendor) == 0 &&
        strcmp(profile->product, jbod_library[i].product) == 0)
    {
      free(profile);
      return jbod_library[i].interface;
    }
  }

  free(profile);
  return NULL;
}

static int jbod_interface_to_index(struct jbod_interface *interface)
{
  int i;
  for (i = 0; i < library_size; i ++) {
    if (jbod_library[i].interface == interface)
      return i;
  }
  return -1;
}

static int find_bsg_device(const char *sg_d_name, char *bsg_path)
{
  DIR *dir;
  struct dirent *ent;
  char tmp_path[PATH_MAX];
  char *bsg_folders[] = {"/dev/bsg", "/dev"};
  const int bsg_folder_count = 2;
  int i;

  if (strncmp("sg", sg_d_name, 2) != 0) {
    /* already the bsg device, just return the full path */
    snprintf(bsg_path, PATH_MAX, "/dev/bsg/%s", sg_d_name);
    return 1;
  }

  snprintf(tmp_path, PATH_MAX,
           "/sys/class/scsi_generic/%s/device/bsg", sg_d_name);

  if ((dir = opendir(tmp_path)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_name[0] == '.')
        continue;

      for (i = 0 ; i < bsg_folder_count; i ++) {
        snprintf(tmp_path, PATH_MAX, "%s/%s", bsg_folders[i], ent->d_name);
        if (access(tmp_path, F_OK) == 0) {
          memcpy(bsg_path, tmp_path, strlen(tmp_path) + 1);
          closedir(dir);
          return 1;
        }
      }
    }
    closedir(dir);
  }
  return 0;
}

void print_list_of_jbod(struct jbod_device jbod_devices[MAX_JBOD_PER_HOST], int count, int show_detail) {
  int i;
  struct jbod_device d;

  char *header_string = show_detail ?
    "sg_device\tbsg_device   \tname\t"
    "Node_SN\tFB_Asset_Node\tFB_Asset_Chassis \n"
    :
    "sg_device\tbsg_device   \tname\n";

  IF_PRINT_NONE_JSON printf("%s", header_string);

  for (i = 0; i < count; i++) {
    d = jbod_devices[i];

    IF_PRINT_NONE_JSON {
      if (show_detail) {
        printf(
          "%s\t%s\t%s\t%s\t%s\t%s\n",
          d.sg_device,
          d.bsg_device,
          d.profile_name,
          d.short_profile.node_sn,
          d.short_profile.fb_asset_node,
          d.short_profile.fb_asset_chassis);
      } else {
        printf(
          "%s\t%s\t%s\n",
          d.sg_device,
          d.bsg_device,
          d.profile_name);
      }
    }
    if (i != 0)
      PRINT_JSON_MORE_GROUP;
    PRINT_JSON_GROUP_HEADER(d.sg_device);
    PRINT_JSON_ITEM("sg_device", "%s", d.sg_device);
    PRINT_JSON_ITEM(
      "bsg_device", "%s", d.bsg_device);

    if (show_detail) {
      PRINT_JSON_ITEM("name", "%s", d.profile_name);
      PRINT_JSON_ITEM("Node_SN", "%s", d.short_profile.node_sn);
      PRINT_JSON_ITEM("FB_Asset_Node", "%s", d.short_profile.fb_asset_node);
      PRINT_JSON_LAST_ITEM("FB_Asset_Chassis", "%s",
                           d.short_profile.fb_asset_chassis);
    } else {
      PRINT_JSON_LAST_ITEM("name", "%s", d.profile_name);
    }

    PRINT_JSON_GROUP_ENDING;
  }
}

int lib_list_jbod(struct jbod_device out[MAX_JBOD_PER_HOST])
{
  DIR *dir;
  struct dirent *ent;
  int index;
  struct jbod_interface *interface;
  char sg_path[PATH_MAX];
  char bsg_path[PATH_MAX];
  int jbod_count = 0;
  struct jbod_short_profile short_p;
#define DEVICE_PATH_COUNT 2
  /* first search /dev/sgXX, then /dev/bsg/XX */
  char *path_prefix[DEVICE_PATH_COUNT][2] =
    {{"/dev/", "sg"}, {"/dev/bsg/", ""}};
  int p;

  for (p = 0; p < DEVICE_PATH_COUNT; ++p) {
    if ((dir = opendir(path_prefix[p][0])) != NULL) {
      while ((ent = readdir (dir)) != NULL) {
        if (strncmp(path_prefix[p][1], ent->d_name, strlen(path_prefix[p][1]))
            != 0)
          continue;
        snprintf(sg_path, PATH_MAX, "%s%s", path_prefix[p][0], ent->d_name);

        interface = detect_dev(sg_path);
        if (interface) {

          index = jbod_interface_to_index(interface);
          short_p = interface->get_short_profile(sg_path);

          snprintf(out[jbod_count].sg_device, PATH_MAX, "%s", sg_path);
          snprintf(out[jbod_count].bsg_device, PATH_MAX, "%s",
            find_bsg_device(basename(sg_path), bsg_path) ? bsg_path : "");
          snprintf(
            out[jbod_count].profile_name, TYPE_NAME_MAX, "%s", jbod_library[index].name);
          memcpy(
            &out[jbod_count].short_profile,
            &short_p,
            sizeof(struct jbod_short_profile));

          jbod_count ++;
        }
      }
      closedir (dir);
      if (jbod_count > 0)  /* found /dev/sgXXX, skip search in /dev/bsgXXX */
        break;
    }
  }
  return jbod_count;
}

int fetch_ses_status(int sg_fd, struct ses_status_info *ses_info)
{
  int rc = 0;
  struct ses_pages *pages = NULL;
  assert(ses_info != NULL);
  memset(ses_info, 0, sizeof(*ses_info));

  pages = (struct ses_pages *) calloc(1, sizeof(struct ses_pages));
  assert(pages != NULL);

  rc = read_ses_pages(sg_fd, pages, NULL);
  if (0 == rc) {
    interpret_ses_pages(pages, ses_info);
  }

  if (pages) {
    free(pages);
    pages = NULL;
  }

  return rc;
}

void jbod_print_temperature_reading(int sg_fd, int print_thresholds)
{
  int i, rc;
  struct ses_status_info ses_info = {};

  rc = fetch_ses_status(sg_fd, &ses_info);
  if (0 == rc) {
    for (i = 0; i < ses_info.temp_count; i++) {
      PRINT_JSON_GROUP_SEPARATE;
      print_temperature_sensor(ses_info.temp_sensors + i, print_thresholds);
    }
  }
}

void jbod_print_voltage_reading(int sg_fd, int print_thresholds) {
  int i, rc;
  struct ses_status_info ses_info = {};

  rc = fetch_ses_status(sg_fd, &ses_info);
  if (0 == rc) {
    for (i = 0; i < ses_info.vol_count; i++) {
      PRINT_JSON_MORE_GROUP;
      print_volatage_sensor(ses_info.vol_sensors + i, print_thresholds);
    }
  }
}

void jbod_print_current_reading(int sg_fd, int print_thresholds)
{
  int i, rc;
  struct ses_status_info ses_info = {};

  rc = fetch_ses_status(sg_fd, &ses_info);
  if (0 == rc) {
    for (i = 0; i < ses_info.curr_count; i++) {
      PRINT_JSON_MORE_GROUP;
      print_current_sensor(ses_info.curr_sensors + i, print_thresholds);
    }
  }
}

void jbod_print_all_sensor_reading(int sg_fd, int print_thresholds)
{
  jbod_print_temperature_reading(sg_fd, print_thresholds);
  jbod_print_voltage_reading(sg_fd, print_thresholds);
  jbod_print_current_reading(sg_fd, print_thresholds);
}

void jbod_print_fan_info(int sg_fd)
{
  struct ses_status_info ses_info = {};
  int i, rc;

  rc = fetch_ses_status(sg_fd, &ses_info);
  if (0 == rc) {
    for (i = 0; i < ses_info.fan_count; i++) {
      PRINT_JSON_GROUP_SEPARATE;
      print_cooling_fan(ses_info.fans + i);
    }
  }
}

int fan_pwm_buffer_id = -1;
int fan_pwm_buffer_offset = -1;
void jbod_control_fan_pwm(int sg_fd, int pwm)
{
  unsigned char buf[SCSI_BUFFER_MAX_SIZE];
  int write_len;
  int i;

  if (fan_pwm_buffer_id == -1 || fan_pwm_buffer_offset == -1) {
    perr("Fan pwm control is not supported.\n");
    return;
  }
  assert(pwm >= 0);
  assert(pwm <= 100);
  scsi_read_buffer(sg_fd, fan_pwm_buffer_id, fan_pwm_buffer_offset,
                   buf, SCSI_BUFFER_MAX_SIZE);
  write_len = (int) buf[0] + 1;
  for (i = 0; i < write_len; i ++) {
    buf[i + 1] = (char) pwm;
  }
  scsi_write_buffer(sg_fd, fan_pwm_buffer_id, fan_pwm_buffer_offset,
                    buf, write_len);
}

void jbod_print_hdd_info (int sg_fd)
{
  struct ses_status_info ses_info = {};
  int i, rc;

  rc = fetch_ses_status(sg_fd, &ses_info);
  if (0 == rc) {
    for (i = 0; i < ses_info.slot_count; i++) {
      PRINT_JSON_GROUP_SEPARATE;
      print_array_device_slot(ses_info.slots + i);
    }
  }
}

void jbod_print_enclosure_info (int sg_fd)
{
  int rc;
  struct ses_status_info ses_info = {};

  rc = fetch_ses_status(sg_fd, &ses_info);
  if (0 == rc) {
    IF_PRINT_NONE_JSON
    printf("Expander SAS Addr\t0x%s\n", ses_info.expander.sas_addr_str);

    PRINT_JSON_ITEM(
        "Expander SAS Addr", "0x%s", ses_info.expander.sas_addr_str);
  }
}

/*
 * if timeout < 0, does not do graceful shutdown
 * if timeout == 0, does not wait for HDD disappear
 * if timeout > 0, wait up to timeout seconds for HDD to disappear
 */
int jbod_hdd_power_off_with_timeout(int sg_fd, int slot_id, int timeout,
                                    int cold_storage)
{
  struct ses_pages pages;
  struct ses_status_info ses_status;
  int page_two_size;
  int rc;

  rc = read_ses_pages(sg_fd, &pages, &page_two_size);
  if (0 != rc) {
    perr("Couldn't read ses pages: %d\n", rc);
    return rc;
  }
  interpret_ses_pages(&pages, &ses_status);
  if (slot_id < 0 || slot_id >= ses_status.slot_count) {
    perr("Slot_id %d is invalid\n", slot_id);
    perr("Valid slot_ids [%d, %d]\n", 0,
            ses_status.slot_count);
    return EINVAL;
  }

  /* clear link in /dev/disk/by-slot */
  if (ses_status.slots[slot_id].by_slot_name)
    unlink(ses_status.slots[slot_id].by_slot_name);


  /* gracefully shutdown the HDD */
  if (timeout >= 0)
    remove_hdd(ses_status.slots[slot_id].dev_name,
               ses_status.slots[slot_id].sas_addr_str);

  /* pull HDD power in hardware */
  control_hdd_power(pages.page_two, ses_status.slots + slot_id, 0);
  rc = sg_send_ses_page(sg_fd, pages.page_two, page_two_size);
  if (timeout <= 0 || 0 != rc) {
    return rc;
  }
  int i = 0;
  for (i = 0; i < timeout; ++i) {
    rc = fetch_ses_status(sg_fd, &ses_status);
    if ((0 == rc) && ses_status.slots[slot_id].dev_name == NULL) {
      return rc;
    }
    sleep(1);
  }
  if (ses_status.slots[slot_id].dev_name) {
    perr(
        "the device %s is still there, errno: %d\n",
        ses_status.slots[slot_id].dev_name,
        rc);
    return 1;
  }
  return rc;
}

/*
 * Power up a drive and wait for the powerup to succeed
 *
 * @returns 0 for success, EINVAL (22) for invalid slot id, -1 for timeout
 * -2 for failures in power on HDD (hit X HDDs per expander limits in cold
 * storage)
 */
int jbod_hdd_power_on_with_timeout(int sg_fd, int slot_id,
                                   int timeout, int cold_storage)
{
  struct ses_pages pages;
  struct ses_status_info ses_status;
  int page_two_size, rc;
  const int max_power_on_cycle_time_s = 60;
  const int dev_check_period = 1;

  rc = read_ses_pages(sg_fd, &pages, &page_two_size);
  if (0 != rc) {
    perr("Couldn't read ses pages: %d\n", rc);
    return rc;
  }
  interpret_ses_pages(&pages, &ses_status);
  if (slot_id < 0 || slot_id >= ses_status.slot_count) {
    perr("Slot_id %d is invalid\n", slot_id);
    perr("Valid slot_ids [%d, %d]\n", 0,
            ses_status.slot_count);
    return EINVAL;
  }

  int start_time = time(NULL);
  int end_time = start_time + timeout;
  do {
    memset(&ses_status, 0, sizeof(ses_status));
    memset(&pages, 0, sizeof(pages));
    page_two_size = 0;
    rc = read_ses_pages(sg_fd, &pages, &page_two_size);
    if (0 != rc) {
      perr("Couldn't read ses pages: %d\n", rc);
      return rc;
    }
    interpret_ses_pages(&pages, &ses_status);

    if (ses_status.slots[slot_id].dev_name && /* device on AND show dev_name */
        (ses_status.slots[slot_id].device_off == 0)) {
      perr("slot %d is already powered and has a device, %s\n", slot_id,
        ses_status.slots[slot_id].dev_name);
      return 0;
    }
    if (cold_storage) {
      int slot_to_power_off = 0;
      for (; slot_to_power_off < ses_status.slot_count; ++slot_to_power_off) {
        jbod_hdd_power_off_with_timeout(sg_fd, slot_to_power_off, 5, cold_storage);
      }
    }
    control_hdd_power(pages.page_two, ses_status.slots + slot_id, 1);
    // TODO(xuanji): we ignore the return value of this
    sg_send_ses_page(sg_fd, pages.page_two, page_two_size);
    memset(&pages, 0, sizeof(pages));
    page_two_size = 0;
    rc = read_ses_pages(sg_fd, &pages, &page_two_size);
    if (0 != rc) {
      perr("Couldn't read ses pages: %d\n", rc);
      return rc;
    }
    if (!check_hdd_power(pages.page_two, ses_status.slots + slot_id))
      return -2;
    if (timeout < 0) {
      timeout = 0;
    }
    if (timeout == 0) {
      return 0;
    }
    int i = 0;
    for (i = 0; i < max_power_on_cycle_time_s; ++i) {
      if (i % dev_check_period == 0) {
        rc = fetch_ses_status(sg_fd, &ses_status);
        if (0 != rc) {
          perr("Couldn't fetch ses status: %d", rc);
          return rc;
        }
        if (ses_status.slots[slot_id].dev_name == NULL) {
          if (end_time < time(NULL)) {
            perr("the device didn't show up before timeout\n");

            return -1;
          }
        } else {
          return 0;
        }
      }
      sleep(1);
    }
  } while (1);

  return 0;
}

int jbod_hdd_power_control (int sg_fd, int slot_id, int op,
                            int timeout, int cold_storage)
{
  if (op == 1) {
    int power_on_res = jbod_hdd_power_on_with_timeout(sg_fd, slot_id,
                                                      timeout, cold_storage);
    if (power_on_res == -1)
      // Timeout expired before we could confirm that the disk powered on, so
      // the disk is in an unknown state. Exit with error.
      exit(1);
    else if (power_on_res == -2)
      exit(2);
  } else {
    /* TODO: (ngie) T28635337 use `power_off_res`
    int power_off_res =
    */ jbod_hdd_power_off_with_timeout(sg_fd, slot_id,
                                                        timeout, cold_storage);
  }

  return 0;  /* TODO: maybe fix this */
}

int jbod_hdd_led_control (int sg_fd, int slot_id, int op)
{
  struct ses_pages pages;
  struct ses_status_info ses_status;
  int page_two_size;

  read_ses_pages(sg_fd, &pages, &page_two_size);
  interpret_ses_pages(&pages, &ses_status);
  if (slot_id < 0 || slot_id >= ses_status.slot_count) {
    perr("Slot_id %d is invalid5\n", slot_id);
    perr("Valid slot_ids [%d, %d]\n", 0,
            ses_status.slot_count);
    return EINVAL;
  }
  control_hdd_led_fault(pages.page_two, ses_status.slots + slot_id, op);
  return sg_send_ses_page(sg_fd, pages.page_two, page_two_size);
}

void jbod_power_cycle_enclosure(int sg_fd)
{
  struct ses_pages pages;
  struct ses_status_info ses_status;
  int page_two_size;
  int ret;

  read_ses_pages(sg_fd, &pages, &page_two_size);
  interpret_ses_pages(&pages, &ses_status);
  power_cycle_enclosure(
    pages.page_two, &(ses_status.enclosure_control));
  ret = sg_send_ses_page(sg_fd, pages.page_two, page_two_size);
  printf("sg_send returns: %d\n", ret);
}

int gpio_buffer_id = -1;
int gpio_buffer_length = -1;
char *(*gpio_descriptions)[3];

void jbod_print_gpio(int sg_fd)
{
  unsigned char buf[SCSI_BUFFER_MAX_SIZE];
  int i;

  if (gpio_buffer_id == -1 ||
      gpio_buffer_length == -1 ||
      gpio_descriptions == NULL) {
    perr("GPIO reading is not supported.\n");
    return;
  }

  scsi_read_buffer(sg_fd, gpio_buffer_id, 0,
                   buf, gpio_buffer_length);

  for (i = 0; i < gpio_buffer_length; i ++) {

    if (gpio_descriptions[i][0])
      if (buf[i] == 0 || buf[i] == 1) {

        IF_PRINT_NONE_JSON
          printf("%50s:\t%s (%d)\n", gpio_descriptions[i][0],
                 gpio_descriptions[i][buf[i] + 1], buf[i]);

        PRINT_JSON_GROUP_SEPARATE;
        PRINT_JSON_GROUP_HEADER(gpio_descriptions[i][0]);
        PRINT_JSON_ITEM("name", "%s", gpio_descriptions[i][0]);
        PRINT_JSON_ITEM("status", "%s", gpio_descriptions[i][buf[i] + 1]);
        PRINT_JSON_LAST_ITEM("val", "%d", buf[i]);
        PRINT_JSON_GROUP_ENDING;
      }
  }
}

int event_status_buffer_id = -1;
int event_status_buffer_length = -1;
char *(*event_status_descriptions)[3];

void jbod_print_event_status(int sg_fd)
{
  unsigned char *buf;
  int i;

  if (event_status_buffer_id == -1 ||
      event_status_buffer_length == -1 ||
      event_status_descriptions == NULL) {
    perr("Event status reading is not supported.\n");
    return;
  }
  buf = (unsigned char *) malloc(event_status_buffer_length);
  scsi_read_buffer(sg_fd, event_status_buffer_id, 0,
                   buf, event_status_buffer_length);
  for (i = 0; i < event_status_buffer_length; i ++) {
    if (event_status_descriptions[i][0])
      if (buf[i] == 0 || buf[i] == 1) {
        IF_PRINT_NONE_JSON
          printf("%s:\t%s (%d)\n", event_status_descriptions[i][0],
                 event_status_descriptions[i][buf[i] + 1], buf[i]);

        PRINT_JSON_GROUP_SEPARATE;

        PRINT_JSON_GROUP_HEADER(event_status_descriptions[i][0]);
        PRINT_JSON_ITEM("name", "%s", event_status_descriptions[i][0]);
        PRINT_JSON_ITEM("status", "%s",
                        event_status_descriptions[i][buf[i] + 1]);
        PRINT_JSON_LAST_ITEM("value", "%d", buf[i]);
        PRINT_JSON_GROUP_ENDING;
      }
  }
  free(buf);
}


int asset_tag_count = 0;
struct scsi_buffer_parameter ** asset_tag_list = NULL;

void jbod_print_asset_tag(int sg_fd)
{
  int i;
  IF_PRINT_NONE_JSON printf("ID\tName\tValue\n");
  for (i = 0; i < asset_tag_count; i++) {
    IF_PRINT_NONE_JSON printf("%d\t", i);
    PRINT_JSON_GROUP_SEPARATE;
    print_read_value(sg_fd, asset_tag_list[i]);
  }
}

void jbod_set_asset_tag(int sg_fd, int tag_id, char *tag)
{
  int len;

  if (asset_tag_list == NULL || asset_tag_count == -1) {
    perr("Asset set is not supported.\n");
  }
  if (tag_id >= asset_tag_count) {
    perr("Invalid tag ID: %d\n", tag_id);
    return;
  }

  len = asset_tag_list[tag_id]->len;

  if (len > strlen(tag))
    len = strlen(tag);

  scsi_write_buffer(sg_fd, asset_tag_list[tag_id]->buf_id,
                    asset_tag_list[tag_id]->buf_offset,
                    (unsigned char*) tag, len);
  perr("Updated tag ID: %d\n", tag_id);
}

void jbod_set_asset_tag_by_name(int sg_fd, char *tag_name, char *tag)
{
  int i;

  if (asset_tag_list == NULL || asset_tag_count == -1) {
    perr("Asset set is not supported.\n");
    return;
  }

  for (i = 0; i < asset_tag_count; i++) {
    if (strcmp(tag_name, asset_tag_list[i]->name) == 0) {
      jbod_set_asset_tag(sg_fd, i, tag);
      return;
    }
  }
  perr("Invalid tag: %s\n", tag_name);
}

struct led_info *jbod_leds = NULL;
int jbod_led_buffer_length = -1;
int jbod_led_buffer_id = -1;

void jbod_print_sys_led(int sg_fd)
{
  unsigned char *buf;
  int i;
  if (jbod_led_buffer_id == -1 || jbod_led_buffer_length == -1) {
    perr("LED reading is not supported.\n");
    return;
  }
  buf = (unsigned char *) malloc(jbod_led_buffer_length);
  scsi_read_buffer(sg_fd, jbod_led_buffer_id, 0, buf, jbod_led_buffer_length);

  IF_PRINT_NONE_JSON {
    printf("%s\t%s\t%s\n", "ID", "Name", "Status");
  }

  for (i = 0; i < jbod_led_buffer_length; i ++) {
    jbod_leds[i].status = buf[i];
    print_led(i, jbod_leds + i);
  }
  free(buf);
}

void jbod_control_sys_led(int sg_fd, int led_id, int value)
{
  perr("Sorry, led control is not supported.\n");
}

struct config_item all_configs[3] = {
  {-1, -1, "Power Window", NULL, NULL},
  {-1, -1, "HDD Temperature Interval", NULL, NULL},
  {-1, -1, "Fan Profile", NULL, NULL}};

void jbod_show_config(int sg_fd)
{
  int i;
  unsigned char buf[1];

  for (i = 0; i < 3; i ++) {
    if (all_configs[i].show_func)
      all_configs[i].show_func(sg_fd, all_configs + i);
    else if (all_configs[i].buffer_id != -1 &&
        all_configs[i].buffer_offset != -1) {
      scsi_read_buffer(sg_fd, all_configs[i].buffer_id,
                       0, buf, all_configs[i].buffer_offset + 1);
      IF_PRINT_NONE_JSON printf("%s:\t %d\n", all_configs[i].name,
                                buf[all_configs[i].buffer_offset]);

      PRINT_JSON_GROUP_SEPARATE;
      PRINT_JSON_GROUP_HEADER(all_configs[i].name);
      PRINT_JSON_ITEM("name", "%s", all_configs[i].name);
      PRINT_JSON_LAST_ITEM("value", "%d", buf[all_configs[i].buffer_offset]);
      PRINT_JSON_GROUP_ENDING;

    }
  }
}

void change_config(int sg_fd, int val, struct config_item *config)
{
  unsigned char buf[1] = {((unsigned char)(val & 0xff))};
  if (config->change_func)
    config->change_func(sg_fd, val, config);
  else if (config->buffer_id != -1 &&
      config->buffer_offset != -1) {
    scsi_write_buffer(sg_fd, config->buffer_id, config->buffer_offset, buf, 1);
  } else {
    printf("Sorry, this command is not supported.\n");
  }
}

void jbod_config_power_window(int sg_fd, int val)
{
  change_config(sg_fd, val, all_configs);
}

void jbod_config_hdd_temp_interval(int sg_fd, int val)
{
  unsigned char buf[4] = {0, 0, 0, ((unsigned char)(val & 0xff))};
  if (all_configs[1].buffer_id != -1 &&
      all_configs[1].buffer_offset != -1) {
    scsi_write_buffer(sg_fd, all_configs[1].buffer_id, 0, buf, 4);
  } else {
    printf("Sorry, this command is not supported.\n");
  }
}

void jbod_config_fan_profile(int sg_fd, int val)
{
  change_config(sg_fd, val, all_configs + 2);
}

void jbod_identify_enclosure(int sg_fd, int val)
{
  printf("Sorry, identify is not supported.\n");
}

void jbod_print_phyerr(int sg_fd)
{
  printf("Sorry, print phyerr is not supported.\n");
}

void jbod_reset_phyerr(int sg_fd)
{
  printf("Sorry, reset phyerr is not supported.\n");
}

void jbod_print_profile(struct jbod_profile *profile)
{
  IF_PRINT_NONE_JSON {
    printf("vendor     \t%s\n", profile->vendor);
    printf("product    \t%s\n", profile->product);
    printf("revision   \t%s\n", profile->revision);
    /* printf("fw version \t%s\n", profile->specific); */
  }
  PRINT_JSON_ITEM("vendor", "%s", profile->vendor);
  PRINT_JSON_ITEM("product", "%s", profile->product);
  PRINT_JSON_ITEM("revision", "%s", profile->revision);
  PRINT_JSON_ITEM("fw version", "%s", profile->specific);
}

struct jbod_short_profile jbod_get_short_profile (char *devname)
{
  struct jbod_short_profile p = {"N/A", "N/A", "N/A"};
  return p;
}
