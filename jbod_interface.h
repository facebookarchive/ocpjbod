/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef JBOD_INTERFACE_H
#define JBOD_INTERFACE_H

#include <limits.h>
#include "common.h"

#define INQUIRY_RESP_INITIAL_LEN 56
#define INQUIRY_VENDOR_LEN 8
#define INQUIRY_PRODUCT_LEN 16
#define INQUIRY_REVISION_LEN 4
#define INQUIRY_SPECIFIC_LEN 20

struct jbod_profile  {
  char vendor[INQUIRY_VENDOR_LEN + 1];
  char product[INQUIRY_PRODUCT_LEN + 1];
  char revision[INQUIRY_REVISION_LEN + 1];
  char specific[INQUIRY_SPECIFIC_LEN + 1];
  struct jbod_interface *interface;
  char *name;
};

struct jbod_short_profile {
  char node_sn[MAX_TAG_LENGTH];
  char fb_asset_node[MAX_TAG_LENGTH];
  char fb_asset_chassis[MAX_TAG_LENGTH];
};

struct jbod_interface {
  /* enclosure info */
  void (*print_enclosure_info) (int sg_fd);

  /* sensors */
  void (*print_temperature_reading)(int sg_fd, int print_thresholds);
  void (*print_voltage_reading)(int sg_fd, int print_thresholds);
  void (*print_current_reading)(int sg_fd, int print_thresholds);
  void (*print_power_reading)(int sg_fd);

  /* all sensors, excluding power */
  void (*print_all_sensor_reading) (int sg_fd, int print_thresholds);

  /* drive info and control */
  void (*print_hdd_info) (int sg_fd);
  int (*hdd_power_control) (int sg_fd, int slot, int op, int timeout, int cold_storage);
  int (*hdd_led_control) (int sg_fd, int slot, int op);

  /* fan info and control */
  void (*print_fan_info)(int sg_fd);
  void (*control_fan_pwm)(int sg_fd, int pwm);

  /* set and show asset tag */
  void (*set_asset_tag) (int sg_fd, int tag_id, char *tag);
  void (*set_asset_tag_by_name) (int sg_fd, char *tag_name, char *tag);
  void (*print_asset_tag) (int sg_fd);

  /* power cycle enclosure */
  void (*power_cycle_enclosure)(int sg_fd);

  /* print gpio */
  void (*print_gpio) (int sg_fd);

  /* event log and status */
  void (*print_event_log) (int sg_fd);
  void (*print_event_status) (int sg_fd);

  /* system led read and control */
  void (*print_sys_led) (int sg_fd);
  void (*control_sys_led) (int sg_fd, int led_id, int value);

  /* system configurations */
  void (*show_config) (int sg_fd);
  void (*config_power_window) (int sg_fd, int val);
  void (*config_hdd_temp_interval) (int sg_fd, int val);
  void (*config_fan_profile) (int sg_fd, int val);

  /* set identify LED of the enclosure, val=1 id on, val=0 id off */
  void (*identify_enclosure) (int sg_fd, int val);

  /* print/reset phy error counters */
  void (*print_phyerr) (int sg_fd);
  void (*reset_phyerr) (int sg_fd);

  /* print jbod profile */
  void (*print_profile) (struct jbod_profile *profile);

  struct jbod_short_profile (*get_short_profile) (char *devname);
};

extern struct jbod_interface knox;
extern struct jbod_interface honeybadger;

/* all supported JBODs: knox, honeybadger, etc. */
extern struct jbod_profile jbod_library[];
extern int library_size;

/* SES information */
extern struct ses_pages *pages;
extern struct ses_status_info *ses_info;

/* check device and figure out which jbod_interface it is */
extern struct jbod_interface *detect_dev(char *devname);

/* extrace the jbod_profile from device name */
extern struct jbod_profile *extract_profile(char *devname);

/* list all supported JBODs */
extern int list_jbod(char jbod_names[MAX_JBOD_PER_HOST][PATH_MAX],
                     int show_detail, int quiet);

/* fetch SES pages and extract information */
extern int fetch_ses_status(int sg_fd);

/* default functions for different JBODs */

extern void jbod_print_enclosure_info (int sg_fd);

extern void jbod_print_temperature_reading(int sg_fd, int print_thresholds);
extern void jbod_print_voltage_reading(int sg_fd, int print_thresholds);
extern void jbod_print_current_reading(int sg_fd, int print_thresholds);
/* no default function for power reading */

extern void jbod_print_hdd_info (int sg_fd);
extern int jbod_hdd_power_control (int sg_fd, int slot_id, int op, int timeout, int cold_storage);
extern int jbod_hdd_led_control (int sg_fd, int slot_id, int op);

extern void jbod_print_all_sensor_reading(int sg_fd, int print_thresholds);

extern void jbod_print_fan_info(int sg_fd);
extern int fan_pwm_buffer_id;
extern int fan_pwm_buffer_offset;
extern void jbod_control_fan_pwm(int sg_fd, int pwm);

extern int asset_tag_count;
extern struct scsi_buffer_parameter ** asset_tag_list;
extern void jbod_print_asset_tag(int sg_fd);
extern void jbod_set_asset_tag(int sg_fd, int tag_id, char *tag);
extern void jbod_set_asset_tag_by_name(int sg_fd, char *tag_name, char *tag);

extern void jbod_power_cycle_enclosure(int sg_fd);

extern int gpio_buffer_id;
extern int gpio_buffer_length;
extern char *(*gpio_descriptions)[3];
extern void jbod_print_gpio(int sg_fd);

extern int event_status_buffer_id;
extern int event_status_buffer_length;
extern char *(*event_status_descriptions)[3];
extern void jbod_print_event_status(int sg_fd);

extern struct led_info *jbod_leds;
extern int jbod_led_buffer_length;
extern int jbod_led_buffer_id;
extern void jbod_print_sys_led(int sg_fd);
extern void jbod_control_sys_led(int sg_fd, int led_id, int value);

struct config_item {
  int buffer_id;
  int buffer_offset;
  char *name;
};

extern struct config_item all_configs[3];

extern void jbod_show_config(int sg_fd);
extern void jbod_config_power_window(int sg_fd, int val);
extern void jbod_config_hdd_temp_interval(int sg_fd, int val);
extern void jbod_config_fan_profile(int sg_fd, int val);

extern void jbod_identify_enclosure(int sg_fd, int val);
extern void jbod_print_phyerr(int sg_fd);
extern void jbod_reset_phyerr(int sg_fd);

extern void jbod_print_profile(struct jbod_profile *profile);

extern struct jbod_short_profile jbod_get_short_profile (char *devname);
#endif
