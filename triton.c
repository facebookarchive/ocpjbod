/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <sys/types.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jbod_interface.h"
#include "scsi_buffer.h"
#include "ses.h"
#include "led.h"
#include "cooling.h"
#include "json.h"

#include "triton.h"

struct jbod_short_profile triton_get_short_profile (char *devname)
{
  struct jbod_short_profile p = {"N/A", "N/A", "N/A"};
  int sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);

  if (sg_fd > 0) {
    read_buffer_string(sg_fd, &triton_dpb_sn, p.node_sn, MAX_TAG_LENGTH);
    read_buffer_string(sg_fd, &fb_asset_tag, p.fb_asset_node, MAX_TAG_LENGTH);
    read_buffer_string(sg_fd, &fb_asset_tag, p.fb_asset_chassis,
                       MAX_TAG_LENGTH);
    sg_cmds_close_device(sg_fd);
  }

  return p;
}

void triton_print_enclosure_info (int sg_fd)
{
  jbod_print_enclosure_info(sg_fd);
  print_read_value(sg_fd, &scc_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &scc_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &triton_dpb_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &triton_dpb_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &ww_chassis_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &ww_chassis_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &fb_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &fb_asset_tag);
}

void triton_print_profile(struct jbod_profile *profile)
{
  IF_PRINT_NONE_JSON {
    printf("vendor     \t%s\n", profile->vendor);
    printf("product    \t%.6s\n", profile->product);
    printf("fw version \t%s\n", profile->specific);
  }
  PRINT_JSON_ITEM("vendor", "%s", profile->vendor);
  PRINT_JSON_ITEM("product", "%.6s", profile->product);
  PRINT_JSON_ITEM("fw version", "%s", profile->specific);
}

void triton_print_power_reading(int sg_fd)
{
  /* TODO: fix JSON or remove these permanently */
  /* print_read_value(sg_fd, &scc_power); */
  /* print_read_value(sg_fd, &dpb_power); */
  print_read_value(sg_fd, &chassis_power);
}

struct led_info triton_leds[] = {
  {SEVEN_SEG,    -1, "7 Segment LED ", NULL, NULL},
  {DUO_COLOR, -1, "Fan Module 1", "Blue", "Yellow"},
  {DUO_COLOR, -1, "Fan Module 2", "Blue", "Yellow"},
  {DUO_COLOR, -1, "Fan Module 3", "Blue", "Yellow"},
  {DUO_COLOR, -1, "Fan Module 4", "Blue", "Yellow"},
  {SINGLE_COLOR, -1, "Enclosure LED", "Amber", NULL},
};

#define TRITON_LED_BUFFER_ID 0x7a
#define TRITON_LED_BUFFER_LENGTH 6

void triton_print_sys_led(int sg_fd)
{
  int i;
  unsigned char buf[TRITON_LED_BUFFER_LENGTH];

  for (i = 0; i < TRITON_LED_BUFFER_LENGTH; ++i) {
    scsi_read_buffer(sg_fd, TRITON_LED_BUFFER_ID, i, buf + i, 1);
  }

  IF_PRINT_NONE_JSON {
    printf("%s\t%s\t%s\n", "ID", "Name", "Status");
  }

  for (i = 0; i < TRITON_LED_BUFFER_LENGTH; i ++) {
    triton_leds[i].status = buf[i];
    print_led(i, triton_leds + i);
  }
}

void trition_identify_enclosure(int sg_fd, int val)
{
  unsigned char buf[2];
  scsi_read_buffer(sg_fd, TRITON_LED_BUFFER_ID, 5, buf, 2);
  if (val == 1) {
    buf[0] = 2;
    buf[1] = 0;
    scsi_write_buffer(sg_fd, TRITON_LED_BUFFER_ID, 5, buf, 2);
  } else if (val == 0) {
    buf[0] = 0;
    buf[1] = 0xff;
    scsi_write_buffer(sg_fd, TRITON_LED_BUFFER_ID, 5, buf, 2);
  }
  triton_print_sys_led(sg_fd);
}

void triton_print_gpio(int sg_fd)
{
  char *triton_gpio_description[19][3] = {
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {"SLOT", "Rear", "Front"},
    {NULL, "Front", "Rear"},
    {"Type", "JBOD", "Server"},
    {"SCC_TYPE_2", "0", "1"},
    {"SCC_TYPE_1", "0", "1"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {"EXP_GPIO_2", "0", "1"},
    {"EXP_GPIO_1", "0", "1"},
    {"EXP_GPIO_0", "0", "1"},
    {"BMC_A_HB", "No Heartbeat", "Heartbeat alive"},
    {"BMC_B_HB", "No Heartbeat", "Heartbeat alive"},
    {"Peer_EXP_HB", "No Heartbeat", "Heartbeat alive"},
  };

  gpio_buffer_id = 0x70;
  gpio_buffer_length = sizeof(triton_gpio_description) /
    sizeof(triton_gpio_description[0]);

  gpio_descriptions = triton_gpio_description;
  jbod_print_gpio(sg_fd);
}


#define TRITON_PHYERR_BUFFER_ID 0x77
#define TRITON_PHYERR_BUFFER_PHY_COUNT 48
#define TRITON_PHYERR_BUFFER_LENGTH 16

void triton_print_phyerr(int sg_fd)
{
  unsigned char buf[TRITON_PHYERR_BUFFER_LENGTH];
  int i;
  char json_key[16];

  IF_PRINT_NONE_JSON
    printf("PHY\tInvalid Dword\tRunning Disparity\tLoss of Dword Sync"
           "\tPhy Reset Problem\n");

  for (i = 0; i < TRITON_PHYERR_BUFFER_PHY_COUNT; i ++) {
    scsi_read_buffer(sg_fd, TRITON_PHYERR_BUFFER_ID, i,
                     buf, TRITON_PHYERR_BUFFER_LENGTH);
    IF_PRINT_NONE_JSON
      printf("%d\t%u\t%u\t%u\t%u\n",
             i,
             four_byte_to_uint(buf + 0),
             four_byte_to_uint(buf + 4),
             four_byte_to_uint(buf + 8),
             four_byte_to_uint(buf + 12)
            );
    if (i) PRINT_JSON_MORE_GROUP;
    snprintf(json_key, 16, "Phy_%u", i);
    PRINT_JSON_GROUP_HEADER(json_key);
    PRINT_JSON_ITEM("Invalid Dword", "%u",
                    four_byte_to_uint(buf + 0));
    PRINT_JSON_ITEM("Running Disparity", "%u",
                    four_byte_to_uint(buf + 4));
    PRINT_JSON_ITEM("Loss of Dword Sync", "%u",
                    four_byte_to_uint(buf + 8));
    PRINT_JSON_LAST_ITEM("Phy Reset Problem", "%u",
                         four_byte_to_uint(buf + 12));
    PRINT_JSON_GROUP_ENDING;
  }
}

void triton_reset_phyerr(int sg_fd)
{
  unsigned char buf[16] = {0};

  scsi_write_buffer(sg_fd, TRITON_PHYERR_BUFFER_ID, 0, buf, 16);
}

void triton_show_power_window(int sg_fd, struct config_item *config)
{
  unsigned char buf[4];

  scsi_read_buffer(sg_fd, config->buffer_id, 0, buf, 4);

  IF_PRINT_NONE_JSON printf("%s:\t %d\n", config->name, buf[1] & 0xff);

  PRINT_JSON_GROUP_SEPARATE;
  PRINT_JSON_GROUP_HEADER(config->name);
  PRINT_JSON_ITEM("name", "%s", config->name);
  PRINT_JSON_LAST_ITEM("value", "%d", buf[1]);
  PRINT_JSON_GROUP_ENDING;
}

void triton_change_power_window(int sg_fd, int val, struct config_item *config)
{
  int i;
  unsigned char buf[2] = {0, (unsigned char) (val & 0xff)};

  for (i = 0; i < 3; ++i) {
    buf[0] = (unsigned char)i;
    scsi_write_buffer(sg_fd, config->buffer_id, i, buf, 2);
  }
}

void set_triton_config()
{
  /* Power Window */
  all_configs[0].buffer_id = 0x41;
  all_configs[0].buffer_offset = 4;
  all_configs[0].show_func = triton_show_power_window;
  all_configs[0].change_func = triton_change_power_window;
  /* HDD Temperature Interval */
  all_configs[1].buffer_id = 0x72;
  all_configs[1].buffer_offset = 3;
  /* Fan Profile */
  all_configs[2].buffer_id = -1;
  all_configs[2].buffer_offset = -1;
}

void triton_show_config(int sg_fd)
{
  set_triton_config();
  jbod_show_config(sg_fd);
}

void triton_config_power_window(int sg_fd, int val)
{
  set_triton_config();
  jbod_config_power_window(sg_fd, val);
}

void triton_config_hdd_temp_interval(int sg_fd, int val)
{
  set_triton_config();
  jbod_config_hdd_temp_interval(sg_fd, val);
}

void triton_config_fan_profile(int sg_fd, int val)
{
  set_triton_config();
  jbod_config_fan_profile(sg_fd, val);
}

struct scsi_buffer_parameter *triton_asset_tag_list[] = {
  &scc_pn, &scc_sn, &triton_dpb_pn, &triton_dpb_sn, &ww_chassis_pn,
  &ww_chassis_sn, &fb_pn, &fb_asset_tag
};

void triton_config_asset_tags()
{
  asset_tag_count = sizeof(triton_asset_tag_list)
    / sizeof(struct scsi_buffer_parameter *);
  asset_tag_list = triton_asset_tag_list;
}

void triton_set_asset_tag(int sg_fd, int tag_id, char *tag)
{
  triton_config_asset_tags();
  jbod_set_asset_tag(sg_fd, tag_id, tag);
}

void triton_set_asset_tag_by_name(int sg_fd, char *tag_name, char *tag)
{
  triton_config_asset_tags();
  jbod_set_asset_tag_by_name(sg_fd, tag_name, tag);
}

void triton_print_asset_tag(int sg_fd)
{
  triton_config_asset_tags();
  jbod_print_asset_tag(sg_fd);
}

void triton_print_event_log(int sg_fd)
{
#define EVENT_LOG_BUFFER_SIZE 8192
  unsigned char buf[EVENT_LOG_BUFFER_SIZE];
  unsigned char *buf_ptr;
  int64_t timestamp = 0;
  int i;
  char id_str[16];

  perr("NOTE: Event log in Triton is not complete at this time...\n");

  sg_ll_read_buffer(sg_fd, 2, 0xe5, 0, (void *) buf,
                    EVENT_LOG_BUFFER_SIZE, 1, 0);

  for (i = 0; i < EVENT_LOG_BUFFER_SIZE; i += 64) {
    buf_ptr = buf + i;
    timestamp = *((int64_t *)(buf_ptr));
    IF_PRINT_NONE_JSON
      printf("[%11ld][Event %d] %d %x %s\n", timestamp,
             buf_ptr[13] * 256 + buf_ptr[12],
             buf_ptr[15] * 256 + buf_ptr[14],
             *((int *)(buf_ptr + 16)),
             (char *)(buf_ptr + 24));

    PRINT_JSON_GROUP_SEPARATE;

    snprintf(id_str, 16, "%d", buf_ptr[13] * 256 + buf_ptr[12]);
    PRINT_JSON_GROUP_HEADER(id_str);
    PRINT_JSON_ITEM("timestamp", "%ld", timestamp);
    PRINT_JSON_ITEM("id", "%d", buf_ptr[13] * 256 + buf_ptr[12]);
    fix_none_ascii((char *)(buf_ptr + 24), strlen((char *)(buf_ptr + 24)));
    PRINT_JSON_LAST_ITEM("description", "%s", (char *)(buf_ptr + 24));
    PRINT_JSON_GROUP_ENDING;
  }
}

void triton_print_event_status(int sg_fd)
{
  event_status_buffer_id = 0x76;
  event_status_buffer_length = 100;
  char *triton_event_status_description[100][3] = {
    /* 0 */
    {NULL, "Off", "On"},
    {"I2C bus 0 crash ", "Off", "On"},
    {"I2C bus 1 crash ", "Off", "On"},
    {"I2C bus 2 crash ", "Off", "On"},
    {"I2C bus 3 crash ", "Off", "On"},
    /* 5 */
    {"I2C bus 4 crash ", "Off", "On"},
    {"I2C bus 5 crash ", "Off", "On"},
    {"I2C bus 6 crash ", "Off", "On"},
    {"I2C bus 7 crash ", "Off", "On"},
    {"I2C bus 8 crash ", "Off", "On"},
    /* 10 */
    {"I2C bus 9 crash ", "Off", "On"},
    {"I2C bus 10 cras h", "Off", "On"},
    {"I2C bus 11 crash ", "Off", "On"},
    {"Fan 1 front fault ", "Off", "On"},
    {"Fan 1 rear fault ", "Off", "On"},
    /* 15 */
    {"Fan 2 front fault ", "Off", "On"},
    {"Fan 2 rear fault ", "Off", "On"},
    {"Fan 3 front fault ", "Off", "On"},
    {"Fan 3 rear fault ", "Off", "On"},
    {"Fan 4 front fault ", "Off", "On"},
    /* 20 */
    {"Fan 4 rear fault ", "Off", "On"},
    {"SCC voltage warning ", "Off", "On"},
    {"DPB voltage warning ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    /* 25 */
    {"SCC current warning ", "Off", "On"},
    {"DPB current warning ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"DPB temp. sensor 1 warning ", "Off", "On"},
    /* 30 */
    {"DPB temp. sensor 2 warning ", "Off", "On"},
    {"SCC expander temp warning ", "Off", "On"},
    {"SCC IOC temp warning ", "Off", "On"},
    {"HDD0 X SMART temp warning ", "Off", "On"},
    {NULL, "Off", "On"},
    /* 35 */
    {NULL, "Off", "On"},
    {"HDD0 fault ", "Off", "On"},
    {"HDD1 fault ", "Off", "On"},
    {"HDD2 fault ", "Off", "On"},
    {"HDD3 fault ", "Off", "On"},
    /* 40 */
    {"HDD4 fault ", "Off", "On"},
    {"HDD5 fault ", "Off", "On"},
    {"HDD6 fault ", "Off", "On"},
    {"HDD7 fault ", "Off", "On"},
    {"HDD8 fault ", "Off", "On"},
    /* 45 */
    {"HDD9 fault ", "Off", "On"},
    {"HDD10 fault ", "Off", "On"},
    {"HDD11 fault ", "Off", "On"},
    {"HDD12 fault ", "Off", "On"},
    {"HDD13 fault ", "Off", "On"},
    /* 50 */
    {"HDD14 fault ", "Off", "On"},
    {"HDD15 fault ", "Off", "On"},
    {"HDD16 fault ", "Off", "On"},
    {"HDD17 fault ", "Off", "On"},
    {"HDD18 fault ", "Off", "On"},
    /* 55 */
    {"HDD19 fault ", "Off", "On"},
    {"HDD20 fault ", "Off", "On"},
    {"HDD21 fault ", "Off", "On"},
    {"HDD22 fault ", "Off", "On"},
    {"HDD23 fault ", "Off", "On"},
    /* 60 */
    {"HDD24 fault ", "Off", "On"},
    {"HDD25 fault ", "Off", "On"},
    {"HDD26 fault ", "Off", "On"},
    {"HDD27 fault ", "Off", "On"},
    {"HDD28 fault ", "Off", "On"},
    /* 65 */
    {"HDD29 fault ", "Off", "On"},
    {"HDD30 fault ", "Off", "On"},
    {"HDD31 fault ", "Off", "On"},
    {"HDD32 fault ", "Off", "On"},
    {"HDD33 fault ", "Off", "On"},
    /* 70 */
    {"HDD34 fault ", "Off", "On"},
    {"HDD35 fault ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    /* 75 */
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    /* 80 */
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    /* 85 */
    {NULL, "Off", "On"},
    {"Fan 1 removed", "Off", "On"},
    {"Fan 2 removed", "Off", "On"},
    {"Fan 3 removed", "Off", "On"},
    {"Fan 4 removed", "Off", "On"},
    /* 90 */
    {"Internal Mini-SAS link error 1 ", "Off", "On"},
    {"Internal Mini-SAS link error 2 ", "Off", "On"},
    {"Drawer pulled out ", "Off", "On"},
    {"Peer SCC pulled out ", "Off", "On"},
    {"IOM A pulled out ", "Off", "On"},
    /* 95 */
    {"IOM B pulled out ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"HW config mismatch ", "Off", "On"},
  };

  event_status_descriptions = triton_event_status_description;
  jbod_print_event_status(sg_fd);
}

void triton_power_cycle_enclosure(int sg_fd)
{
  unsigned char buf[3] = {0, 1, 0};
  scsi_write_buffer(sg_fd, 0xe9, 0, buf, 3);
}

static void triton_ses_pwm_control(struct cooling_fan *fan, int pwm,
                          unsigned char *page_two)
{
  unsigned char *fan_element_ptr = page_two + fan->page_two_offset;

  fan->common_control = fan->common_status & 0xf0;
  fan->common_control |= 0x80;

  fan_element_ptr[0] = fan->common_control;
  fan_element_ptr[1] = 0;
  fan_element_ptr[2] = pwm & 0xff;
  fan_element_ptr[3] = (pwm == 0) ? 0x00 : 0x20;
}

void triton_control_fan_pwm(int sg_fd, int pwm)
{
  struct ses_pages pages;
  struct ses_status_info ses_status;
  int page_two_size;
  int i;

  read_ses_pages(sg_fd, &pages, &page_two_size);

  interpret_ses_pages(&pages, &ses_status);

  for (i = 0; i < ses_status.fan_count; i ++)
    triton_ses_pwm_control(ses_status.fans + i, pwm, pages.page_two);

  sg_send_ses_page(sg_fd, pages.page_two, page_two_size);
}

enum {
  TRITON_PWM_BUFFER_ID = 0x50,
  TRITON_PWM_BUFFER_SIZE = 2,
};

void triton_print_pwm(int sg_fd) {
  uint8_t data[TRITON_PWM_BUFFER_SIZE] = {0};
  int     result = scsi_read_buffer(sg_fd,
                                    TRITON_PWM_BUFFER_ID, 0,
                                    data, sizeof(data));
  if (result < 0) {
    perr("unable to read expander pwm, error=%d\n", result);
  } else if (print_json) {
    printf("\"PWM_SENSOR_%d\": %d\n", data[0], data[1]);
  } else {
    printf("PWM_SENSOR_%d\t%d%%\n", data[0], data[1]);
  }
}

enum {
  TRITON_CFM_BUFFER_ID = 0x48,
  TRITON_CFM_BUFFER_SIZE = 2,
};

void triton_print_cfm(int sg_fd) {
  uint8_t data[TRITON_CFM_BUFFER_SIZE] = {0};
  int     result = scsi_read_buffer(sg_fd,
                                    TRITON_CFM_BUFFER_ID, 0,
                                    data, sizeof(data));
  if (result < 0) {
    perr("unable to read expander pwm, error=%d\n", result);
  } else if (print_json) {
    printf("\"AIR_FLOW_SENSOR_%d\": %d\n", data[0], data[1]);
  } else {
    printf("AIR_FLOW_SENSOR_%d\t%d CFM\n", data[0], data[1]);
  }
}

struct jbod_interface triton = {
  triton_print_enclosure_info,
  jbod_print_temperature_reading,
  jbod_print_voltage_reading,
  jbod_print_current_reading,
  triton_print_power_reading,
  jbod_print_all_sensor_reading,
  jbod_print_hdd_info,
  jbod_hdd_power_control,
  jbod_hdd_led_control,
  jbod_print_fan_info,
  triton_control_fan_pwm,
  triton_set_asset_tag,
  triton_set_asset_tag_by_name,
  triton_print_asset_tag,
  triton_power_cycle_enclosure,
  triton_print_gpio,
  triton_print_event_log,
  triton_print_event_status,
  triton_print_sys_led,
  NULL, /* knox_control_sys_led, */
  triton_show_config,
  triton_config_power_window,
  triton_config_hdd_temp_interval,
  NULL, /* knox_config_fan_profile, */
  trition_identify_enclosure,
  triton_print_phyerr,
  triton_reset_phyerr,
  triton_print_profile,
  triton_get_short_profile,
  .print_pwm = triton_print_pwm,
  .print_cfm = triton_print_cfm,
};
