/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef JBOF_INTERFACE_H
#define JBOF_INTERFACE_H

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"

#define JBOF_ID_MAX 100

typedef struct jbof_profile {
  uint16_t vendor;
  uint16_t device;
  uint16_t subsystem_vendor;
  uint16_t subsystem_device;
  uint32_t class_code;
  struct jbof_interface *interface;
  char *name;
} jbof_profile_t;

typedef enum {
  FLASH_TYPE_U2,
  FLASH_TYPE_M2,
} flashtype_t;

typedef struct jbof_handle {
  char jbof_id[JBOF_ID_MAX];
  char switchpath[PATH_MAX];
  flashtype_t flash_type;
  char vendor_name[PATH_MAX];
} jbof_handle_t;

struct jbof_interface {
  /**
   * Open a JBOF device from a JBOF id string.
   */
  jbof_handle_t* (*open)(const char* jbof_id);

  /**
   * Close the JBOF handle and free associated resources
   */
  void (*close)(jbof_handle_t *handle);

  /**
   * Control JBOF enclosure identify led.
   *
   * @param handle  JBOF handle
   * @param enable  Desired state of identify led
   */
  void (*identify)(jbof_handle_t* handle, bool enable);

  /**
   * Print enclsoure FRU ID info
   */
  void (*print_enclosure_info)(jbof_handle_t *handle);

  /**
   * List and print information about each drive in an enclosure
   */
  void (*print_drive_info)(jbof_handle_t *handle);

  /**
   * Print values of JBOF sensors.
   *
   * @param handle  JBOF handle
   */
  void (*print_sensor_info)(jbof_handle_t* handle);

  /**
   * Print fan information.
   * @param handle  JBOF handle
   */
  void (*print_fan_info)(jbof_handle_t* handle);

  /**
   * Print state of JBOF enclosure leds.
   *
   * @param handle  JBOF handle
   */
  void (*print_led_info)(jbof_handle_t* handle);

  /**
  * Print switch phyerr counters.
  *
  * @param handle  JBOF handle
  * @param clear   if true, clear event counters
  */
  void (*print_phyerr_info)(jbof_handle_t* handle, bool clear);

  /**
  * Print asset tags.
  *
  * @param handle  JBOF handle
  */
  void (*print_asset_tags)(jbof_handle_t* handle);

  /**
   * Turn power off or on for a slot in the expander
   */
  void (*set_slot_power)(jbof_handle_t *handle, int slot, int power);

  /**
   * Detach driver from device associated with this slot.
   *
   * Using set_slot_power to power down a drive also does this before powering
   * it down, so this isn't necessary to cleanly shut down a drive.
   */
  void (*remove_slot_dev) (jbof_handle_t *handle, int slot);

  /**
   * Control PCIe slot Attention Indicator led.
   */
  void (*set_slot_fault_led) (jbof_handle_t *handle, int slot, bool enable);

  /**
   * Turn chassis precool mode on/off (Seagate M.2 only)
   */
  void (*set_precool_mode)(jbof_handle_t* handle, bool enable);
};

struct jbof_profile *jbof_detect_pci_dev(char *pcidev_path);

extern struct jbof_interface lightning;

/* check device and figure out which jbod_interface it is */
extern struct jbof_interface *detect_jbof_dev(char *devname);

/* list all supported JBOFs */
extern int list_jbof(char jbof_names[MAX_JBOF_PER_HOST][PATH_MAX],
                     int show_detail, int quiet);

/* get jbof_profile* given a jbof_id */
extern struct jbof_profile *lookup_jbof_profile(char* jbof_id);

#ifdef SYSFS_READ_IMPL
#define MK_SYSFS_READ(T) \
int sysfs_read_ ## T (char *basepath, char *file, T ## _t *out) { \
  char fullpath[PATH_MAX]; \
  char buffer[32]; \
  char *end; \
  int fd; \
  snprintf(fullpath, PATH_MAX, "%s/%s", basepath, file); \
  fd = open(fullpath, O_RDONLY); \
  if (fd < 0) { \
    return 0; \
  } \
  if (read(fd, buffer, sizeof(buffer)) <= 0) { \
    close(fd); \
    return 0; \
  }; \
  *out = strtoull(buffer, &end, 0); \
  if (end == buffer) { \
    close(fd); \
    return 0; \
  } \
  close(fd); \
  return 1; \
}
#else
#define MK_SYSFS_READ(T) \
  int sysfs_read_ ## T (char *basepath, char *file, T ## _t *out);
#endif
MK_SYSFS_READ(uint16);
MK_SYSFS_READ(uint32);

#endif // JBOF_INTERFACE_H
