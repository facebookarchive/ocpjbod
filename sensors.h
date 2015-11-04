/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef SENSORS_H
#define SENSORS_H

struct temperature_sensor {
  unsigned char common_status;          /* for status only */
  unsigned char common_control;         /* for control only */

  int temperature;

  int ot_critical_threshold;
  int ot_warning_threshold;
  int ut_critical_threshold;
  int ut_warning_threshold;

  int ot_failure;
  int ot_warning;
  int ut_failure;
  int ut_warning;

  char *name;
};

struct voltage_sensor {
  unsigned char common_status;          /* for status only */
  unsigned char common_control;         /* for control only */

  float voltage;

  float ov_critical_threshold;
  float ov_warning_threshold;
  float uv_critical_threshold;
  float uv_warning_threshold;

  int ov_failure;
  int ov_warning;
  int uv_failure;
  int uv_warning;

  char *name;
};

struct current_sensor {
  unsigned char common_status;          /* for status only */
  unsigned char common_control;         /* for control only */

  float current;

  float oc_critical_threshold;
  float oc_warning_threshold;
  float uc_critical_threshold;
  float uc_warning_threshold;

  int oc_failure;
  int oc_warning;
  char *name;
};

extern void print_temperature_sensor(struct temperature_sensor *sensor,
                                     int print_thresholds);
extern void print_volatage_sensor(struct voltage_sensor *sensor,
                                  int print_thresholds);
extern void print_current_sensor(struct current_sensor *sensor,
                                 int print_thresholds);

extern int extract_temperature_sensor_info(
  unsigned char *temperature_sensor_element,
  unsigned char *sensor_description,
  unsigned char *threshold_info,
  struct temperature_sensor *sensor);

extern int extract_voltage_sensor_info(
  unsigned char *voltage_sensor_element,
  unsigned char *sensor_description,
  unsigned char *threshold_info,
  struct voltage_sensor *sensor);

extern int extract_current_sensor_info(
  unsigned char *current_sensor_element,
  unsigned char *sensor_description,
  unsigned char *threshold_info,
  struct current_sensor *sensor);

#endif
