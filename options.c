/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <getopt.h>

#include "options.h"
#include "jbod_interface.h"
#include "jbof_interface.h"
#include "json.h"

#ifdef UTIL_VERSION
#define VERSION_STRING UTIL_VERSION
#else
#define VERSION_STRING "00.rc"
#endif

struct option long_options[] = {
  {"help",           no_argument,         0,    'h' },
  {"hdd-on",         required_argument,   0,    'O' },
  {"hdd-off",        required_argument,   0,    'o' },
  {"hdd-remove",     required_argument,   0,    'R' },
  {"fault-on",       required_argument,   0,    'F' },
  {"fault-off",      required_argument,   0,    'f' },
  {"thresholds",     no_argument,         0,    't' },
  {"all",            no_argument,         0,    'a' },
  {"auto",           no_argument,         0,    'A' },
  {"pwm",            required_argument,   0,    'p' },
  {"precool",        required_argument,   0,    'C' },
  {"tag",            required_argument,   0,    'T' },
  {"id",             required_argument,   0,    'i' },
  {"log",            no_argument,         0,    'l' },
  {"status",         no_argument,         0,    's' },
  {"power-win",      required_argument,   0,    'w' },
  {"hdd-temp-int",   required_argument,   0,    'H' },
  {"fan-profile",    required_argument,   0,    'P' },
  {"off",            no_argument,         0,    'D' },
  {"json",           no_argument,         0,    'j' },
  {"clear",          no_argument,         0,    'c' },
  {"detail",         no_argument,         0,    'd' },
  {"timeout",        required_argument,   0,    'm' },
  {"cold-storage",   no_argument,         0,    'z' },
  {"dirty",          no_argument,         0,    'y' },
  {0,                0,                   0,    0   },
};

static const char short_options[] = "O:o:R:F:f:taAp:C:T:i:lsw:H:P:Djcdm:zy";

static int option_index = 0;

static char jbof_targets_[MAX_JBOF_PER_HOST][PATH_MAX];
static size_t jbof_target_count_ = 0;

static void jbof_unsupported() {
  perr("this command is not suported by jbof target\n");
  exit(1);
}

/* extract device name from argc/argv */
static char* get_devname(int argc, char* argv[]) {
  optind = 1;
  while (getopt_long(argc, argv, short_options, long_options, NULL) != -1) {
  }

  if (optind < argc) {
    return argv[optind];
  }

  if (jbof_target_count_) {
    return jbof_targets_[0];
  }

  return NULL;
}

/* list all JBODs */
int execute_list(int argc, char *argv[])
{
  char c;
  int show_detail = 0;
  int jbod_count;
  struct jbod_device jbod_devices[MAX_JBOD_PER_HOST];

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'd':
        show_detail = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  jbod_count = lib_list_jbod(jbod_devices);
  print_list_of_jbod(jbod_devices, jbod_count, show_detail);

  return 0;
}

int jbof_execute_list(int argc, char *argv[])
{
  char c;
  int show_detail = 0;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'd':
        show_detail = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  list_jbof(NULL, show_detail, 0);
  return 0;
}

/* show sensor readings */
int execute_sensor(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int print_thresholds = 0;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 't':
        print_thresholds = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_all_sensor_reading(sg_fd, print_thresholds);
      PRINT_JSON_MORE_ITEM;
      jbod->print_power_reading(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

int execute_pwm(int argc, char *argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbod_interface_t* jbod = detect_dev(devname);
  if (!jbod) {
    perr("%s is not a jbod device\n", devname);
    return EXIT_FAILURE;
  }
  if (!jbod->print_pwm) {
    perr("command is not supported by jbod device, name='%s'\n", devname);
    return EXIT_FAILURE;
  }
  int sg_fd = sg_cmds_open_device(devname, 0, 0);
  if (sg_fd <= 0) {
    perr("unable to open sg device, name='%s'\n", devname);
    return EXIT_FAILURE;
  }
  jbod->print_pwm(sg_fd);
  sg_cmds_close_device(sg_fd);
  return EXIT_SUCCESS;
}

int execute_cfm(int argc, char *argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbod_interface_t* jbod = detect_dev(devname);
  if (!jbod) {
    perr("%s is not a jbod device\n", devname);
    return EXIT_FAILURE;
  }
  if (!jbod->print_pwm) {
    perr("command is not supported by jbod device, name='%s'\n", devname);
    return EXIT_FAILURE;
  }
  int sg_fd = sg_cmds_open_device(devname, 0, 0);
  if (sg_fd <= 0) {
    perr("unable to open sg device, name='%s'\n", devname);
    return EXIT_FAILURE;
  }
  jbod->print_cfm(sg_fd);
  sg_cmds_close_device(sg_fd);
  return EXIT_SUCCESS;
}

static int jbof_execute_sensor(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  profile->interface->print_sensor_info(handle);
  profile->interface->close(handle);
  return EXIT_SUCCESS;
}

int jbof_drive_power(char *jbof_id, int slot, int power) {
  struct jbof_profile *profile;
  struct jbof_handle *handle;
  profile = lookup_jbof_profile(jbof_id);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", jbof_id);
    return 1;
  }
  handle = profile->interface->open(jbof_id);
  if (handle) {
    profile->interface->set_slot_power(handle, slot, power);
    profile->interface->close(handle);
  } else {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, jbof_id);
    return 1;
  }
  return 0;
}

int jbof_drive_remove(char *jbof_id, int slot) {
  struct jbof_profile *profile;
  struct jbof_handle *handle;
  profile = lookup_jbof_profile(jbof_id);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", jbof_id);
    return 1;
  }
  handle = profile->interface->open(jbof_id);
  if (handle) {
    profile->interface->remove_slot_dev(handle, slot);
    profile->interface->close(handle);
  } else {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, jbof_id);
    return 1;
  }
  return 0;
}

int jbof_drive_set_fault_led(char* jbof_id, int slot, bool enable) {
  struct jbof_profile* profile;
  struct jbof_handle* handle;
  profile = lookup_jbof_profile(jbof_id);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", jbof_id);
    return 1;
  }
  handle = profile->interface->open(jbof_id);
  if (handle) {
    profile->interface->set_slot_fault_led(handle, slot, enable);
    profile->interface->close(handle);
  } else {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, jbof_id);
    return 1;
  }
  return 0;
}

int jbof_show_drives(char *jbof_id) {
  struct jbof_profile *profile;
  struct jbof_handle *handle;
  profile = lookup_jbof_profile(jbof_id);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", jbof_id);
    return 1;
  }
  handle = profile->interface->open(jbof_id);
  if (handle) {
    profile->interface->print_drive_info(handle);
    profile->interface->close(handle);
  } else {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, jbof_id);
    return 1;
  }
  return 0;
}

int parse_slot_number(const char* str, flashtype_t flash_type) {
  if (flash_type == FLASH_TYPE_M2) {
    int slot, drive;
    if (sscanf(str, "%d:%d", &slot, &drive) < 2) {
      perr("m.2 slot format is required (e.g 0:1)\n");
      return -1;
    }
    return slot * 2 + drive + 1;
  }
  return atoi(str) + 1;
}

int jbof_execute_hdd(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  flashtype_t flash_type = handle->flash_type;
  profile->interface->close(handle);

  int hdd_on_id = -1;
  int hdd_off_id = -1;
  int hdd_remove_id = -1;
  int fault_led_on_id = -1;
  int fault_led_off_id = -1;
  char c;
  optind = 1;

  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch (c) {
      CASE_JSON;
      case 'O':
        hdd_on_id = parse_slot_number(optarg, flash_type);
        break;
      case 'o':
        hdd_off_id = parse_slot_number(optarg, flash_type);
        break;
      case 'R':
        hdd_remove_id = parse_slot_number(optarg, flash_type);
        break;
      case 'F':
        fault_led_on_id = parse_slot_number(optarg, flash_type);
        break;
      case 'f':
        fault_led_off_id = parse_slot_number(optarg, flash_type);
        break;
      default:
        perr("not all hdd flags supported for JBOF mode\n");
        return EXIT_FAILURE;
    }
  }

  if (hdd_on_id != -1 && hdd_off_id != -1) {
    perr("Cannot specify both hdd_on and hdd_off.\n");
    return EXIT_FAILURE;
  }

  if (hdd_on_id != -1) {
    return jbof_drive_power(devname, hdd_on_id, 1);
  }

  if (hdd_off_id != -1) {
    return jbof_drive_power(devname, hdd_off_id, 0);
  }

  if (hdd_remove_id != -1) {
    return jbof_drive_remove(devname, hdd_remove_id);
  }

  if (fault_led_on_id != -1) {
    return jbof_drive_set_fault_led(devname, fault_led_on_id, true);
  }

  if (fault_led_off_id != -1) {
    return jbof_drive_set_fault_led(devname, fault_led_off_id, false);
  }

  return jbof_show_drives(devname);
}

/* show HDD info, control HDD power on/off, fault */
int execute_hdd(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  char c;
  int hdd_on_id = -1;
  int hdd_off_id = -1;
  int fault_led_on_id = -1;
  int fault_led_off_id = -1;
  int show_all = 0;
  int jbod_count = 0;
  int timeout = 0;
  struct jbod_device jbod_devices[MAX_JBOD_PER_HOST];
  int i;
  int ret;
  int cold_storage = 0;
  int dirty = 0;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'O':
        hdd_on_id = atoi(optarg);
        break;
      case 'o':
        hdd_off_id = atoi(optarg);
        break;
      case 'F':
        fault_led_on_id = atoi(optarg);
        break;
      case 'f':
        fault_led_off_id = atoi(optarg);
        break;
      case 'a':
        show_all = 1;
        break;
      case 'm':
        timeout = atoi(optarg);
        break;
      case 'z':
        cold_storage = 1;
        break;
      case 'y':
        dirty = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  if (timeout < 0) {
    perr("Cannot specify negative timeout, %d.\n", timeout);
    return 1;
  }

  if (hdd_on_id != -1 && hdd_off_id != -1) {
    perr("Cannot specify both hdd_on and hdd_off.\n");
    return 1;
  }

  if (fault_led_on_id != -1 && fault_led_off_id != -1) {
    perr("Cannot specify both fault_led_on and fault_led_off.\n");
    return 1;
  }

  if (show_all) {
    jbod_count = lib_list_jbod(jbod_devices);
    for (i = 0; i < jbod_count; ++i) {
      PRINT_JSON_RESET_GROUP;
      jbod = detect_dev(jbod_devices[i].sg_device);
      if (jbod) {
        IF_PRINT_NONE_JSON
          printf(">>> %s \n", jbod_devices[i].sg_device);
        sg_fd = sg_cmds_open_device(jbod_devices[i].sg_device, 0 /* rw */,
                                    0 /* not verbose */);
        if (sg_fd > 0) {
          if (i) PRINT_JSON_MORE_GROUP;
          PRINT_JSON_GROUP_HEADER(jbod_devices[i].sg_device);
          jbod->print_hdd_info(sg_fd);
          PRINT_JSON_GROUP_ENDING;
          sg_cmds_close_device(sg_fd);
        }
      }
    }
    return 0;
  }

  ret = 0;
  devname = get_devname(argc, argv);
  sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
  if (sg_fd < 0) {
    perr("%s is not a jbod device (doesn't exist)\n", devname);
    return ENOTDIR;
  }
  jbod = detect_dev(devname);
  if (jbod) {
    if (hdd_on_id != -1) {
      ret = jbod->hdd_power_control(sg_fd, hdd_on_id, 1, timeout,
                                    cold_storage);
    } else if (hdd_off_id != -1) {
      if (dirty)
        timeout = -1;   /* skip graceful shutdown */
      ret = jbod->hdd_power_control(sg_fd, hdd_off_id, 0, timeout,
                                    cold_storage);
    } else if (fault_led_on_id != -1) {
      ret = jbod->hdd_led_control(sg_fd, fault_led_on_id, 1);
    } else if (fault_led_off_id != -1) {
      ret = jbod->hdd_led_control(sg_fd, fault_led_off_id, 0);
    }
    if (ret != 0) {
      perr("operation failed with return code = %d\n", ret);
    }
    PRINT_JSON_GROUP_HEADER(devname);
    jbod->print_hdd_info(sg_fd);
    PRINT_JSON_GROUP_ENDING;
  } else {
    perr("%s is not a jbod device\n", devname);
    ret = ENODEV;
  }
  sg_cmds_close_device(sg_fd);
  return ret;
}

/* show system level LEDs */
int execute_led(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_sys_led(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

static int jbof_execute_led(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  profile->interface->print_led_info(handle);
  profile->interface->close(handle);
  return EXIT_SUCCESS;
}

/* show fan rpm, control fan pwm */
int execute_fan(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int set_pwm = -1;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'p':
        set_pwm = atoi(optarg);
        break;
      case 'A':
        set_pwm = 0;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (set_pwm >= 0 && set_pwm <=100) {
        jbod->control_fan_pwm(sg_fd, set_pwm);
      } else {
        jbod->print_fan_info(sg_fd);
      }
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }

  return 0;
}

static int jbof_execute_fan(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  optind = 1;
  for (;;) {
    int c = getopt_long(argc, argv, short_options, long_options, &option_index);
    if (c == 'C') {
      bool enable = atoi(optarg) != 0;
      profile->interface->set_precool_mode(handle, enable);
      break;
    }
    if (c == -1) {
      profile->interface->print_fan_info(handle);
      break;
    }
  }
  profile->interface->close(handle);
  return EXIT_SUCCESS;
}

int jbof_execute_info(int argc, char*argv[]) {
  char *devname = get_devname(argc, argv);
  struct jbof_profile *profile;
  struct jbof_handle *handle;
  profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return 1;
  }
  handle = profile->interface->open(devname);
  if (handle) {
    profile->interface->print_enclosure_info(handle);
    profile->interface->close(handle);
  } else {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
  }
  return 0;
}

/* show info of a JBOD enclosure */
int execute_info(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  struct jbod_profile *profile;

  devname = get_devname(argc, argv);
  profile = extract_profile(devname);
  jbod = detect_dev(devname);

  if (jbod && profile) {
    jbod->print_profile(profile);

    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_enclosure_info(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  free(profile);
  return 0;
}

/* power cycle expander */
int execute_power_cycle(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;

  devname = get_devname(argc, argv);

  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->power_cycle_enclosure(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

/* show GPIO values */
int execute_gpio(int argc, char *argv[]) {
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;

  devname = get_devname(argc, argv);

  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_gpio(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

/* show/change asset tags */
int execute_asset_tag(int argc, char *argv[]) {
  int tag_id = -1;
  char *tag_name = NULL;
  char *tag = NULL;
  char c;
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'i':
        tag_id = atoi(optarg);
        tag_name = optarg;
        break;
      case 'T':
        tag = optarg;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);

  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (tag != NULL && tag_id != -1) {
        if (tag_id == 0 && (strcmp("0", tag_name) != 0)) {

          /* atoi return 0 on none-"0", try tag_name */
          jbod->set_asset_tag_by_name(sg_fd, tag_name, tag);
        } else {
          /* atoi return right value */
          jbod->set_asset_tag(sg_fd, tag_id, tag);
        }
      }
      jbod->print_asset_tag(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

static int jbof_execute_asset_tag(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  optind = 1;
  for (int opt = 0; opt != -1;) {
    opt = getopt_long(argc, argv, short_options, long_options, &option_index);
    if (opt == 'i' || opt == 'T') {
      perr("Asset tag update isn't supported in JBOF mode\n");
      return EXIT_FAILURE;
    }
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  profile->interface->print_asset_tags(handle);
  profile->interface->close(handle);
  return EXIT_SUCCESS;
}

/* show event status, event log */
int execute_event(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int show_status = 0;
  int show_log = 0;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 's':
        show_status = 1;
        break;
      case 'l':
        show_log = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (show_status) {
        jbod->print_event_status(sg_fd);
      } else if (show_log) {
        jbod->print_event_log(sg_fd);
      } else {
        usage(argc, argv);
      }
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

int execute_config(int argc, char *argv[]) {
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int power_window = -1;
  int hdd_temp_int = -1;
  int fan_profile = -1;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'w':
        power_window = atoi(optarg);
        break;
      case 'H':
        hdd_temp_int = atoi(optarg);
        break;
      case 'P':
        fan_profile = atoi(optarg);
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (power_window != -1) {
        jbod->config_power_window(sg_fd, power_window);
      }

      if (hdd_temp_int != -1) {
        jbod->config_hdd_temp_interval(sg_fd, hdd_temp_int);
      }

      if (fan_profile != -1) {
        jbod->config_fan_profile(sg_fd, fan_profile);
      }

      jbod->show_config(sg_fd);

      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

int execute_identify(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int val = 1;  /* val=1 id on, val=0 id off */
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'D':
        val = 0;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->identify_enclosure(sg_fd, val);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

static int jbof_execute_identify(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  bool enable = true;
  optind = 1;
  for (;;) {
    int c = getopt_long(argc, argv, short_options, long_options, &option_index);
    if (c == 'D' || c == -1) {
      enable = (c == -1);
      break;
    }
  }
  profile->interface->identify(handle, enable);
  profile->interface->print_led_info(handle);
  profile->interface->close(handle);
  return EXIT_SUCCESS;
}

int execute_version(int argc, char *argv[])
{
  char c;

  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      default:
        /*
         * commented out to avoid double-print from `usage(..)`.
         * usage(argc, argv);
         */
        break;
    }
  }

  IF_PRINT_NONE_JSON
    printf("Version: %s\n", VERSION_STRING);

  PRINT_JSON_LAST_ITEM("Version", "%s", VERSION_STRING);
  return 0;
}

int _execute_phyerr(char *devname, int clear, int index)
{
  struct jbod_interface *jbod;
  int sg_fd;
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (clear)
        jbod->reset_phyerr(sg_fd);
      else {
        if (index) PRINT_JSON_MORE_GROUP;
        PRINT_JSON_GROUP_HEADER(devname);
        jbod->print_phyerr(sg_fd);
        PRINT_JSON_GROUP_ENDING;
      }
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

int execute_phyerr(int argc, char *argv[])
{
  char  c;
  int show_all = 0;
  int clear = 0;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'a':
        show_all = 1;
        break;
      case 'c':
        clear = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  int result = 0;

  if (show_all) {
    struct jbod_device jbod_devices[MAX_JBOD_PER_HOST];
    int jbod_count = lib_list_jbod(jbod_devices);
    for (int i = 0; i < jbod_count; ++i) {
      PRINT_JSON_RESET_GROUP;
      if ( (result = _execute_phyerr(jbod_devices[i].sg_device, clear, i)) ) {
        /* return any non-zero return value, breaking the loop */
        break;
      }
    }
  } else {
    /* one-shot traditional behavior */
    result = _execute_phyerr(get_devname(argc, argv), clear, 0 /* index */);
  }

  return result;
}

static int jbof_execute_phyerr(int argc, char* argv[]) {
  char* devname = get_devname(argc, argv);
  assert(devname);
  jbof_profile_t* profile = lookup_jbof_profile(devname);
  if (!profile) {
    perr("Didn't understand JBOF id: %s\n", devname);
    return EXIT_FAILURE;
  }
  jbof_handle_t* handle = profile->interface->open(devname);
  if (!handle) {
    perr("Couldn't open JBOF device as %s: %s\n", profile->name, devname);
    return EXIT_FAILURE;
  }
  bool clear = false;
  optind = 1;
  for (;;) {
    int c = getopt_long(argc, argv, short_options, long_options, &option_index);
    if (c == 'c' || c == -1) {
      clear = (c == 'c');
      break;
    }
  }
  profile->interface->print_phyerr_info(handle, clear);
  profile->interface->close(handle);
  return EXIT_SUCCESS;
}

struct cmd_options all_cmds[] = {
  {LIST, "list", execute_list, jbof_execute_list, "list all enclosures\n"
   "\t\t\t--detail        \t- show some details of each JBOD"},
  {INFO, "info", execute_info, jbof_execute_info, "show info of the JBOD"},
  {SENSOR, "sensor", execute_sensor, jbof_execute_sensor,
   "print sensor values\n"
   "\t\t\t--thresholds    \t- also prints thresholds"},
  {HDD, "hdd", execute_hdd, jbof_execute_hdd, "hdd info/control\n"
   "\t\t\t--hdd-on id     \t- turn on HDD\n"
   "\t\t\t--hdd-off id    \t- turn off HDD\n"
   "\t\t\t--hdd-remove id \t- remove HDD from OS w/o powering off (JBOF only)\n"
   "\t\t\t--dirty         \t- skip graceful shutdown of HDD\n"
   "\t\t\t--fault-on id   \t- turn on fault LED\n"
   "\t\t\t--fault-off id  \t- turn off fault LED\n"
   "\t\t\t--timeout <secs>\t- wait for drive on/off for up to <secs>\n"
   "\t\t\t--cold-storage  \t- special features for cold storage\n"
   "\t\t\t--all           \t- show HDDs from all JBODs"},
  {LED, "led", execute_led, jbof_execute_led, "show status of chassis LEDs"},
  {FAN, "fan", execute_fan, jbof_execute_fan, "fan rpm/pwm\n"
   "\t\t\t--pwm  <pwm>    \t- set fan pwm\n"
   "\t\t\t--precool <0|1> \t- enable/disable precool mode (Seagate M.2 only)\n"
   "\t\t\t--auto          \t- return pwm control to firmware"},
  {POWER_CYCLE, "power_cycle", execute_power_cycle, NULL,
   "power cycle the expander"},
  {GPIO, "gpio", execute_gpio, NULL, "show GPIO status"},
  {ASSET_TAG, "tag", execute_asset_tag, jbof_execute_asset_tag,
   "show/change asset tag(s)\n"
   "\t\t\t--id <tag_id> --tag <tag>\t- update tag"},
  {EVENT, "event", execute_event, NULL, "show event log/status\n"
   "\t\t\t--log           \t- show event log\n"
   "\t\t\t--status        \t- show event status"},
  {CONFIG, "config", execute_config, NULL, "change configurations\n"
   "\t\t\t--power-win <sec> \t- config RMS window of power reading\n"
   "\t\t\t--hdd-temp-int <min> \t- config HDD temperature pooling interval\n"
   "\t\t\t--fan-profile <id> \t- select fan control profile"},
  {IDENTIFY, "identify", execute_identify, jbof_execute_identify,
   "identify enclosure tray\n"
   "\t\t\t--off \t- turn off identify (default is turn on)"},
  {PHYERR, "phyerr", execute_phyerr, jbof_execute_phyerr,
   "show/clear phy error counters\n"
   "\t\t\t--clear \t- clear phy error counters"},
  {PWM, "pwm", execute_pwm, NULL, "show scsi expander pwm"},
  {CFM, "cfm", execute_cfm, NULL, "show scsi expander cfm"},
  {VERSION, "version", execute_version, execute_version, "show version number"},
};

void usage(int argc, char *argv[])
{
  static int called_usage = 0;
  int i;

  /* TODO: see comment above `parse_cmd(..)`. */
  if (called_usage) {
    return;
  }
  called_usage++;

  if (jbof_target_count_) {
    printf("%s command [options] expander_id\n", argv[0]);
  } else {
    printf("%s command [options] sg_device\n", argv[0]);
  }

  execute_version(argc, argv);
  printf("\n"
         "Global options:\n"
         "\t\t\t--json          \t- show output in JSON format\n\n"
         "Commands:\n");

  for (i = 0; i < sizeof(all_cmds) / sizeof(struct cmd_options); i ++) {
    if (jbof_target_count_ && !all_cmds[i].execute_flash) continue;
    printf("\t%10s\t%s\n\n", all_cmds[i].key_word, all_cmds[i].help_msg);
  }
}

int parse_global_options(int argc, char *argv[])
{
  char c;

  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'h':
        /* FALLTHROUGH */
        return 1;
      default:
        /*
         * see comment above `parse_command` for why this cannot be
         */
#if 0
        return 1;
#else
        break;
#endif
    }
  }

  return 0;
}

/*
 * TODO: the option handling, in particular with the global options, is
 * unnecessarily complex.
 *
 * Items which are global should be precede commands, be parsed via
 * `parse_global_options`, and `argc`/`argv` should be adjusted via pointers
 * so all subcommands can consume the additional options.
 *
 * This is made more complex by the fact that `parse_global_options`
 * effectively ignores options in subcommands, making checking for invalid
 * options impossible.
 *
 * Adding insult to injury, --json is "marketed" as a global option, but only
 * handled in some subcommands, making it impossible to use it in said
 * subcommands.
 */
int parse_cmd(int argc, char *argv[])
{
  int i;

  /*
   * TODO: (ngie) `usage(..)` and other callers consume `jbof_target_count_`,
   * so this value must be derived up front.
   *
   * This is unintuitive at first glance, and not lazy/wasteful on platforms
   * that don't have jbofs, e.g., T7 (cold storage).
   *
   * Thought: what if fbjbod was hardlinked to an appropriate command name
   * and DTRT for handling both cases based on that fact?
   */
  jbof_target_count_ = list_jbof(jbof_targets_, false, true);

  if (argc < 2) {
    goto fail;
  }

  if (parse_global_options(argc - 1, argv + 1)) {
    goto fail;
  }

  // if cmd is "list", do both
  if (strcmp("list", argv[1]) == 0) {
    int ret = 0;
    JSON_HEADER;
    IF_PRINT_NONE_JSON printf("JBOD Devices:\n");
    ret += execute_list(argc - 1, argv + 1);
    IF_PRINT_NONE_JSON printf("JBOF Devices:\n");
    ret += jbof_execute_list(argc - 1, argv + 1);
    JSON_ENDING;
    return ret;
  }

  for (i = 0; i < sizeof(all_cmds) / sizeof(struct cmd_options); i ++) {
    if (strcmp(all_cmds[i].key_word, argv[1]) == 0) {
      int ret = 0;
      if (jbof_target_count_) {
        if (!all_cmds[i].execute_flash) {
          jbof_unsupported();
        } else {
          ret = all_cmds[i].execute_flash(argc - 1, argv + 1);
        }
      } else {
        JSON_HEADER;
        ret = all_cmds[i].execute(argc - 1, argv + 1);
        JSON_ENDING;
      }
      return ret;
    }
  }

fail:
  usage(argc, argv);
  return 1;
}
