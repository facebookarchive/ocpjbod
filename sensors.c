/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdio.h>
#include "sensors.h"
#include "ses.h"
#include "options.h"
#include "json.h"

void print_temperature_sensor_threshold(struct temperature_sensor *sensor)
{
  IF_PRINT_NONE_JSON
    printf("\t\tThresholds:\t%d C, %d C, %d C, %d C",
           sensor->ot_critical_threshold,
           sensor->ot_warning_threshold,
           sensor->ut_warning_threshold,
           sensor->ut_critical_threshold);
  PRINT_JSON_MORE_ITEM;
  PRINT_JSON_ITEM("OT critical", "%d", sensor->ot_critical_threshold);
  PRINT_JSON_ITEM("OT warning", "%d", sensor->ot_warning_threshold);
  PRINT_JSON_ITEM("UT warning", "%d", sensor->ut_warning_threshold);
  PRINT_JSON_LAST_ITEM("UT critical", "%d", sensor->ut_critical_threshold);
}

void print_volatage_sensor_threshold(struct voltage_sensor *sensor)
{
  IF_PRINT_NONE_JSON
    printf("\t\tThresholds:\t%.2f %%, %.2f %% %.2f %%, %.2f %%",
           sensor->ov_critical_threshold * 100,
           sensor->ov_warning_threshold * 100,
           sensor->uv_warning_threshold * 100,
           sensor->uv_critical_threshold * 100);
  PRINT_JSON_MORE_ITEM;
  PRINT_JSON_ITEM("OV critical", "%.2f", sensor->ov_critical_threshold);
  PRINT_JSON_ITEM("OV warning", "%.2f", sensor->ov_warning_threshold);
  PRINT_JSON_ITEM("UV warning", "%.2f", sensor->uv_warning_threshold);
  PRINT_JSON_LAST_ITEM("UV critical", "%.2f", sensor->uv_critical_threshold);
}

void print_current_sensor_threshold(struct current_sensor *sensor)
{
  IF_PRINT_NONE_JSON
    printf("\t\tThresholds:\t%.2f %%, %.2f %%",
           sensor->oc_critical_threshold * 100,
           sensor->oc_warning_threshold * 100);
  PRINT_JSON_MORE_ITEM;
  PRINT_JSON_ITEM("OC critical", "%.2f %%", sensor->oc_critical_threshold);
  PRINT_JSON_LAST_ITEM("OC warning", "%.2f %%", sensor->oc_warning_threshold);
}

void print_temperature_sensor(struct temperature_sensor *sensor,
                              int print_thresholds)
{
  IF_PRINT_NONE_JSON {
    printf("Temperature: %s\t%d C",
           sensor->name, sensor->temperature);
  }
  PRINT_JSON_GROUP_HEADER(sensor->name);
  PRINT_JSON_ITEM("type", "%s", "temperature");
  PRINT_JSON_ITEM("unit", "%s", "Celcius");
  PRINT_JSON_LAST_ITEM("value", "%d", sensor->temperature);

  if (print_thresholds) {
    print_temperature_sensor_threshold(sensor);
  }

  IF_PRINT_NONE_JSON printf("\n");
  PRINT_JSON_GROUP_ENDING;
}

void print_volatage_sensor(struct voltage_sensor *sensor,
                           int print_thresholds)
{
  IF_PRINT_NONE_JSON {
    printf("Voltage: %s\t%.2f V",
           sensor->name, sensor->voltage);
  }
  PRINT_JSON_GROUP_HEADER(sensor->name);
  PRINT_JSON_ITEM("type", "%s", "voltage");
  PRINT_JSON_ITEM("unit", "%s", "Volt");
  PRINT_JSON_LAST_ITEM("value", "%.2f", sensor->voltage);

  if (print_thresholds) {
    print_volatage_sensor_threshold(sensor);
  }
  IF_PRINT_NONE_JSON printf("\n");
  PRINT_JSON_GROUP_ENDING;
}

void print_current_sensor(struct current_sensor *sensor,
                          int print_thresholds)
{
  IF_PRINT_NONE_JSON {
    printf("Current: %s\t%.2f A",
           sensor->name, sensor->current);
  }
  PRINT_JSON_GROUP_HEADER(sensor->name);
  PRINT_JSON_ITEM("type", "%s", "current");
  PRINT_JSON_ITEM("unit", "%s", "Amphere");
  PRINT_JSON_LAST_ITEM("value", "%.2f", sensor->current);

  if (print_thresholds) {
    print_current_sensor_threshold(sensor);
  }
  IF_PRINT_NONE_JSON printf("\n");
  PRINT_JSON_GROUP_ENDING;
}

int extract_temperature_sensor_info(
  unsigned char *temperature_sensor_element,
  unsigned char *sensor_description,
  unsigned char *threshold_info,
  struct temperature_sensor *sensor)
{

  sensor->common_status = temperature_sensor_element[0];
  sensor->temperature = (int)temperature_sensor_element[2] - 20;

  sensor->ot_critical_threshold = (int) threshold_info[0] - 20;
  sensor->ot_warning_threshold = (int) threshold_info[1] - 20;
  sensor->ut_warning_threshold = (int) threshold_info[2] - 20;
  sensor->ut_critical_threshold = (int) threshold_info[3] - 20;

  sensor->ot_failure = temperature_sensor_element[3] & 0x08;
  sensor->ot_warning = temperature_sensor_element[3] & 0x04;
  sensor->ut_failure = temperature_sensor_element[3] & 0x02;
  sensor->ut_warning = temperature_sensor_element[3] & 0x01;

  sensor->name = copy_description(sensor_description);

  if (STATUS_CODE(sensor->common_status) == ELEMENT_STATUS_NOT_INSTALLED) {
    sensor->temperature = 0;
  }
  return 0;
}

int extract_voltage_sensor_info(
  unsigned char *voltage_sensor_element,
  unsigned char *sensor_description,
  unsigned char *threshold_info,
  struct voltage_sensor *sensor) {

  sensor->common_status = voltage_sensor_element[0];
  sensor->voltage =
    ((int)voltage_sensor_element[2] * 256 + voltage_sensor_element[3]) * 0.01;

  sensor->ov_critical_threshold = threshold_info[0] * 0.005;
  sensor->ov_warning_threshold = threshold_info[1] * 0.005;
  sensor->uv_warning_threshold = threshold_info[2] * 0.005;
  sensor->uv_critical_threshold = threshold_info[3] * 0.005;

  sensor->ov_failure = voltage_sensor_element[1] & 0x02;
  sensor->ov_warning = voltage_sensor_element[1] & 0x08;
  sensor->uv_failure = voltage_sensor_element[1] & 0x01;
  sensor->uv_warning = voltage_sensor_element[1] & 0x04;

  sensor->name = copy_description(sensor_description);

  if (STATUS_CODE(sensor->common_status) == ELEMENT_STATUS_NOT_INSTALLED) {
    sensor->voltage = 0.0;
  }
  return 0;
}

int extract_current_sensor_info(
  unsigned char *current_sensor_element,
  unsigned char *sensor_description,
  unsigned char *threshold_info,
  struct current_sensor *sensor) {

  sensor->common_status = current_sensor_element[0];
  sensor->current =
    ((int)current_sensor_element[2] * 256 + current_sensor_element[3]) * 0.01;

  sensor->oc_critical_threshold = threshold_info[0] * 0.005;
  sensor->oc_warning_threshold = threshold_info[1] * 0.005;

  sensor->oc_failure = current_sensor_element[1] & 0x02;
  sensor->oc_warning = current_sensor_element[1] & 0x08;

  sensor->name = copy_description(sensor_description);

  if (STATUS_CODE(sensor->common_status) == ELEMENT_STATUS_NOT_INSTALLED) {
    sensor->current = 0.0;
  }
  return 0;
}
