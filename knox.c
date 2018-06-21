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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jbod_interface.h"
#include "scsi_buffer.h"
#include "ses.h"
#include "led.h"
#include "cooling.h"
#include "json.h"

#include "knox.h"

void knox_print_power_reading(int sg_fd)
{
  print_read_value(sg_fd, &power);
}

void knox_print_enclosure_info (int sg_fd)
{
  jbod_print_enclosure_info(sg_fd);
  print_read_value(sg_fd, &seb_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &seb_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &knox_dpb_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &knox_dpb_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &fcb_pn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &fcb_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &tray_sn); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &node_sn); PRINT_JSON_MORE_ITEM;

  print_read_value(sg_fd, &tray_asset); PRINT_JSON_MORE_ITEM;
  print_read_value(sg_fd, &chassis_tag);
}

struct scsi_buffer_parameter *knox_asset_tag_list[] =
{&tray_asset, &chassis_tag,
 &seb_pn, &seb_sn, &knox_dpb_pn, &knox_dpb_sn, &fcb_pn, &fcb_sn, &tray_pn,
 &tray_sn, &node_pn, &node_sn, &rack_pos};

void knox_config_asset_tags()
{
  asset_tag_count = sizeof(knox_asset_tag_list) /
    sizeof(knox_asset_tag_list[0]);
  asset_tag_list = knox_asset_tag_list;
}

void knox_set_asset_tag(int sg_fd, int tag_id, char *tag)
{
  knox_config_asset_tags();
  jbod_set_asset_tag(sg_fd, tag_id, tag);
}

void knox_set_asset_tag_by_name(int sg_fd, char *tag_name, char *tag)
{
  knox_config_asset_tags();
  jbod_set_asset_tag_by_name(sg_fd, tag_name, tag);
}

void knox_print_asset_tag(int sg_fd)
{
  knox_config_asset_tags();
  jbod_print_asset_tag(sg_fd);
}

void honeybadger_print_enclosure_info (int sg_fd)
{
  jbod_print_enclosure_info(sg_fd);
  print_read_value(sg_fd, &fcb_pn);
  PRINT_JSON_MORE_ITEM; print_read_value(sg_fd, &fcb_sn);
  /* PRINT_JSON_MORE_ITEM; print_read_value(sg_fd, &chassis_tag); */
}

struct scsi_buffer_parameter *honeybadger_asset_tag_list[] =
{&fcb_pn, &fcb_sn};  /* TODO: add more information and fix the chassis tag */

void honeybadger_config_asset_tags()
{
  asset_tag_count = sizeof(honeybadger_asset_tag_list)
    / sizeof(struct scsi_buffer_parameter *);
  asset_tag_list = honeybadger_asset_tag_list;
}

void honeybadger_set_asset_tag(int sg_fd, int tag_id, char *tag)
{
  honeybadger_config_asset_tags();
  jbod_set_asset_tag(sg_fd, tag_id, tag);
}

void honeybadger_set_asset_tag_by_name(int sg_fd, char *tag_name, char *tag)
{
  honeybadger_config_asset_tags();
  jbod_set_asset_tag_by_name(sg_fd, tag_name, tag);
}

void honeybadger_print_asset_tag(int sg_fd)
{
  honeybadger_config_asset_tags();
  jbod_print_asset_tag(sg_fd);
}

void knox_print_gpio(int sg_fd)
{
  gpio_buffer_id = 0x70;
  gpio_buffer_length = 15;
  char *knox_gpio_description[15][3] = {
    {"Expander ID ", "Expander A", "Expander B"},
    {"Debug board detection ", "Bottom Tray", "Top Tray"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {"Peer SEB Heartbeat Detection ", "No Heartbeat", "Heartbeat alive"},
    {"Pulling Out Detection For Self Tray ", "Tray not out", "Tray out"},
    {"Pulling Out Detection For Peer Tray ", "Tray not out", "Tray out"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {"Peer SEB Detection ", "Peer SEB attached", "Peer SEB not attached"},
    {"FCB HW Revision ", "Normal storage", "Cold storage"},
    {"DPB HW Revision ", "No interconnection", "x1 SAS interconnection"},
    {"Peer Tray SEB A Heartbeat Detection ", "No Heartbeat", "Heartbeat alive"},
    {"Peer Tray SEB B Heartbeat Detection ", "No Heartbeat", "Heartbeat alive"},
  };

  gpio_descriptions = knox_gpio_description;
  jbod_print_gpio(sg_fd);
}

void honeybadger_print_gpio(int sg_fd)
{
  gpio_buffer_id = 0x70;
  gpio_buffer_length = 15;
  char *honeybadger_gpio_description[15][3] = {
    {"FCB HW Revision", "Low", "High"},
    {"DPB HW Revision", "Low", "High"},
    {"Pulling Out Detection For Self Tray ", "Tray not out", "Tray out"},
    {NULL, "Low", "High"},
    {"Pulling Out Detection For Peer Tray ", "Tray not out", "Tray out"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {"Expander ID ", "Expander A", "Expander B"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
    {NULL, "Low", "High"},
  };

  gpio_descriptions = honeybadger_gpio_description;
  jbod_print_gpio(sg_fd);
}


void knox_print_event_log(int sg_fd)
{
#define EVENT_LOG_BUFFER_SIZE 8192
  unsigned char buf[EVENT_LOG_BUFFER_SIZE];
  unsigned char *buf_ptr;
  int64_t timestamp = 0;
  int i;
  char id_str[16];

  perr("NOTE: Event log in Knox is not complete at this time...\n");

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

void knox_print_event_status(int sg_fd)
{
  event_status_buffer_id = 0x76;
  event_status_buffer_length = 100;
  char *knox_event_status_description[100][3] = {
    {NULL, "Off", "On"},
    {"Expander A fault ", "Off", "On"},
    {"Expander B fault ", "Off", "On"},
    {"I2C bus A crash ", "Off", "On"},
    {"I2C bus B crash ", "Off", "On"},
    {"I2C bus C crash ", "Off", "On"},
    {"I2C bus D crash ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"Fan 1 front fault ", "Off", "On"},
    {"Fan 1 rear fault ", "Off", "On"},
    {"Fan 2 front fault ", "Off", "On"},
    {"Fan 2 rear fault ", "Off", "On"},
    {"Fan 3 front fault ", "Off", "On"},
    {"Fan 3 rear fault ", "Off", "On"},
    {"Fan 4 front fault ", "Off", "On"},
    {"Fan 4 rear fault ", "Off", "On"},
    {"Fan 5 front fault ", "Off", "On"},
    {"Fan 5 rear fault ", "Off", "On"},
    {"Fan 6 front fault ", "Off", "On"},
    {"Fan 6 rear fault ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"DPB temp. sensor 1 warning ", "Off", "On"},
    {"DPB temp. sensor 2 warning ", "Off", "On"},
    {"DPB temp. sensor 3 warning ", "Off", "On"},
    {"DPB temp. sensor 4 warning ", "Off", "On"},
    {"SEB temp. sensor A warning ", "Off", "On"},
    {"SEB temp. sensor B warning ", "Off", "On"},
    {"Ambient temp. sensor A1 warning ", "Off", "On"},
    {"Ambient temp. sensor A2 warning ", "Off", "On"},
    {"Ambient temp. sensor B1 warning ", "Off", "On"},
    {"Ambient temp. sensor B2 warning ", "Off", "On"},
    {"BJT temp. sensor 1 ", "Off", "On"},
    {"BJT temp. sensor 2 warning ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"SEB voltage sensor warning ", "Off", "On"},
    {"DPB voltage sensor warning ", "Off", "On"},
    {"FCB voltage sensor warning ", "Off", "On"},
    {"FCB current sensor warning ", "Off", "On"},
    {NULL, "Off", "On"},
    {"HDD0 SMART temp. warning ", "Off", "On"},
    {"HDD1 SMART temp. warning ", "Off", "On"},
    {"HDD2 SMART temp. warning ", "Off", "On"},
    {"HDD3 SMART temp. warning ", "Off", "On"},
    {"HDD4 SMART temp. warning ", "Off", "On"},
    {"HDD5 SMART temp. warning ", "Off", "On"},
    {"HDD6 SMART temp. warning ", "Off", "On"},
    {"HDD7 SMART temp. warning ", "Off", "On"},
    {"HDD8 SMART temp. warning ", "Off", "On"},
    {"HDD9 SMART temp. ", "Off", "On"},
    {"HDD10 SMART temp. warning ", "Off", "On"},
    {"HDD11 SMART temp. warning ", "Off", "On"},
    {"HDD12 SMART temp. warning ", "Off", "On"},
    {"HDD13 SMART temp. warning ", "Off", "On"},
    {"HDD14 SMART temp. warning ", "Off", "On"},
    {"Expander A Internal temp. warning ", "Off", "On"},
    {"Expander B Internal temp. warning ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"HDD0 fault ", "Off", "On"},
    {"HDD1 fault ", "Off", "On"},
    {"HDD2 fault ", "Off", "On"},
    {"HDD3 fault ", "Off", "On"},
    {"HDD4 fault ", "Off", "On"},
    {"HDD5 fault ", "Off", "On"},
    {"HDD6 fault ", "Off", "On"},
    {"HDD7 fault ", "Off", "On"},
    {"HDD8 fault ", "Off", "On"},
    {"HDD9 fault ", "Off", "On"},
    {"HDD10 fault ", "Off", "On"},
    {"HDD11 fault ", "Off", "On"},
    {"HDD12 fault ", "Off", "On"},
    {"HDD13 fault ", "Off", "On"},
    {"HDD14 fault ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"External Mini-SAS link error ", "Off", "On"},
    {"Internal Mini-SAS 1 link error ", "Off", "On"},
    {"Internal Mini-SAS 2 link error ", "Off", "On"},
    {"Self tray is pulled out ", "Off", "On"},
    {"Peer tray is pulled out ", "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {NULL, "Off", "On"},
    {"Firmware and hardware not match ", "Off", "On"},
  };

  event_status_descriptions = knox_event_status_description;
  jbod_print_event_status(sg_fd);
}

void honeybadger_print_event_status(int sg_fd)
{
  perr("Event Status is not yet supported for Honey Badger\n");
}

void set_knox_config()
{
  all_configs[0].buffer_id = -1;
  all_configs[0].buffer_offset = -1;
  all_configs[1].buffer_id = -1;
  all_configs[1].buffer_offset = -1;
  all_configs[2].buffer_id = 0x44;
  all_configs[2].buffer_offset = 0;
}

void knox_show_config(int sg_fd)
{
  set_knox_config();
  jbod_show_config(sg_fd);
}

void knox_config_power_window(int sg_fd, int val)
{
  set_knox_config();
  jbod_config_power_window(sg_fd, val);
}

void knox_config_hdd_temp_interval(int sg_fd, int val)
{
  set_knox_config();
  jbod_config_hdd_temp_interval(sg_fd, val);
}

void knox_config_fan_profile(int sg_fd, int val)
{
  set_knox_config();
  jbod_config_fan_profile(sg_fd, val);
}

void set_honeybadger_config()
{
  all_configs[0].buffer_id = -1;
  all_configs[0].buffer_offset = -1;
  all_configs[1].buffer_id = 0x72;
  all_configs[1].buffer_offset = 3;
  all_configs[2].buffer_id = -1;
  all_configs[2].buffer_offset = -1;
}

void honeybadger_show_config(int sg_fd)
{
  set_honeybadger_config();
  jbod_show_config(sg_fd);
}

void honeybadger_config_power_window(int sg_fd, int val)
{
  set_honeybadger_config();
  jbod_config_power_window(sg_fd, val);
}

void honeybadger_config_hdd_temp_interval(int sg_fd, int val)
{
  set_honeybadger_config();
  jbod_config_hdd_temp_interval(sg_fd, val);
}

void honeybadger_config_fan_profile(int sg_fd, int val)
{
  set_honeybadger_config();
  jbod_config_fan_profile(sg_fd, val);
}

struct led_info knox_leds[] = {
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, "Ext. Mini SAS", "Red", NULL},
  {SINGLE_COLOR, -1, "Ext. Mini SAS", "Blue", NULL},
  {SINGLE_COLOR, -1, "Int. Mini SAS 1", "Red", NULL},
  {SINGLE_COLOR, -1, "Int. Mini SAS 1", "Blue", NULL},
  {SINGLE_COLOR, -1, "Int. Mini SAS 2", "Red", NULL},
  {SINGLE_COLOR, -1, "Int. Mini SAS 2", "Blue", NULL},
  {SINGLE_COLOR, -1, "Enc. Status", "Red", NULL},
  {SINGLE_COLOR, -1, "Enc. Status", "Blue", NULL},
  {SINGLE_COLOR, -1, "Fan Module 1", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 2", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 3", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 4", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 5", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 6", "Red", NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SEVEN_SEG,    -1, "7 Segment LED ", NULL, NULL},
};

#define KNOX_LED_BUFFER_ID 0x75
#define KNOX_LED_BUFFER_LENGTH 17

void knox_print_sys_led(int sg_fd)
{
  jbod_led_buffer_length = KNOX_LED_BUFFER_LENGTH;
  jbod_led_buffer_id = KNOX_LED_BUFFER_ID;
  jbod_leds = knox_leds;
  jbod_print_sys_led(sg_fd);
}

struct led_info honeybadger_leds[] = {
  {SEVEN_SEG,    -1, "7 Segment LED ", NULL, NULL},
  {SINGLE_COLOR, -1, "Fan Module 1", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 2", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 3", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 4", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 5", "Red", NULL},
  {SINGLE_COLOR, -1, "Fan Module 6", "Red", NULL},
  {SINGLE_COLOR, -1, "Ext. Mini SAS", "Red", NULL},
  {SINGLE_COLOR, -1, "Ext. Mini SAS", "Blue", NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
  {SINGLE_COLOR, -1, NULL, NULL, NULL},
};
#define HONEYBADGER_LED_BUFFER_ID 0x7a
#define HONEYBADGER_LED_BUFFER_LENGTH 16

void honeybadger_print_sys_led(int sg_fd)
{
  int i;
  unsigned char buf[HONEYBADGER_LED_BUFFER_LENGTH];

  for (i = 0; i < HONEYBADGER_LED_BUFFER_LENGTH; ++i) {
    scsi_read_buffer(sg_fd, HONEYBADGER_LED_BUFFER_ID, i, buf + i, 1);
  }

  IF_PRINT_NONE_JSON {
    printf("%s\t%s\t%s\n", "ID", "Name", "Status");
  }

  for (i = 0; i < HONEYBADGER_LED_BUFFER_LENGTH; i ++) {
    honeybadger_leds[i].status = buf[i];
    print_led(i, honeybadger_leds + i);
  }
}

void knox_control_sys_led(int sg_fd, int led_id, int value)
{

}

static void knox_ses_pwm_control(struct cooling_fan *fan, int pwm,
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

void knox_control_fan_pwm(int sg_fd, int pwm)
{
  struct ses_pages pages;
  struct ses_status_info ses_status;
  int page_two_size;
  int i;

  perr("NOTE: PWM control in Knox has some bug at this time...\n");

  read_ses_pages(sg_fd, &pages, &page_two_size);

  interpret_ses_pages(&pages, &ses_status);

  for (i = 0; i < ses_status.fan_count; i ++)
    knox_ses_pwm_control(ses_status.fans + i, pwm, pages.page_two);

  sg_send_ses_page(sg_fd, pages.page_two, page_two_size);
}

void knox_power_cycle_enclosure(int sg_fd)
{
  unsigned char buf[3] = {0, 1, 0};
  scsi_write_buffer(sg_fd, 0xe9, 0, buf, 3);
}

void knox_identify_enclosure(int sg_fd, int val)
{
  unsigned char buf[KNOX_LED_BUFFER_LENGTH];
  scsi_read_buffer(sg_fd, KNOX_LED_BUFFER_ID, 0, buf, KNOX_LED_BUFFER_LENGTH);
  if (val == 1) {
    buf[0] = 1;
    buf[8] = 2;
    scsi_write_buffer(sg_fd, KNOX_LED_BUFFER_ID, 0,
                      buf, KNOX_LED_BUFFER_LENGTH);
  } else if (val == 0) {
    buf[0] = 0;
    buf[8] = 1;
    scsi_write_buffer(sg_fd, KNOX_LED_BUFFER_ID, 0,
                      buf, KNOX_LED_BUFFER_LENGTH);
  }
  knox_print_sys_led(sg_fd);
}

void honeybadger_identify_enclosure(int sg_fd, int val)
{
  unsigned char buf[HONEYBADGER_LED_BUFFER_LENGTH];
  scsi_read_buffer(sg_fd, HONEYBADGER_LED_BUFFER_ID, 0,
                   buf, HONEYBADGER_LED_BUFFER_LENGTH);

  /*
  if (val == 1) {
    buf[0] = 1;
    buf[8] = 2;
    scsi_write_buffer(sg_fd, HONEYBADGER_LED_BUFFER_ID, 0,
                      buf, HONEYBADGER_LED_BUFFER_LENGTH);
  } else if (val == 0) {
    buf[0] = 0;
    buf[8] = 1;
    scsi_write_buffer(sg_fd, HONEYBADGER_LED_BUFFER_ID, 0,
                      buf, HONEYBADGER_LED_BUFFER_LENGTH);
  }
  */
  perr("Identify is not yet implemented for Honey Badger. \n");
  honeybadger_print_sys_led(sg_fd);
}

#define KNOX_PHYERR_BUFFER_ID 0x77
#define KNOX_PHYERR_BUFFER_PHY_COUNT 20
#define KNOX_PHYERR_BUFFER_LENGTH 16

#define HONEYBADGER_PHYERR_BUFFER_PHY_COUNT 24
int phyerr_buffer_phy_count = KNOX_PHYERR_BUFFER_PHY_COUNT;

void knox_print_phyerr(int sg_fd)
{
  unsigned char buf[KNOX_PHYERR_BUFFER_LENGTH];
  int i;
  char json_key[16];

  IF_PRINT_NONE_JSON
    printf("PHY\tInvalid Dword\tRunning Disparity\tLoss of Dword Sync"
           "\tPhy Reset Problem\n");

  for (i = 0; i < phyerr_buffer_phy_count; i ++) {
    scsi_read_buffer(sg_fd, KNOX_PHYERR_BUFFER_ID, i,
                     buf, KNOX_PHYERR_BUFFER_LENGTH);
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

void honeybadger_print_phyerr(int sg_fd)
{
  phyerr_buffer_phy_count = HONEYBADGER_PHYERR_BUFFER_PHY_COUNT;
  knox_print_phyerr(sg_fd);
}

void knox_reset_phyerr(int sg_fd)
{
  unsigned char buf[16] = {0};

  scsi_write_buffer(sg_fd, KNOX_PHYERR_BUFFER_ID, 0, buf, 16);
}

void knox_print_profile(struct jbod_profile *profile)
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

struct jbod_short_profile knox_get_short_profile (char *devname)
{
  struct jbod_short_profile p = {"N/A", "N/A", "N/A"};
  int sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);

  if (sg_fd > 0) {
    read_buffer_string(sg_fd, &node_sn, p.node_sn, MAX_TAG_LENGTH);
    read_buffer_string(sg_fd, &tray_asset, p.fb_asset_node, MAX_TAG_LENGTH);
    read_buffer_string(sg_fd, &chassis_tag, p.fb_asset_chassis, MAX_TAG_LENGTH);
    sg_cmds_close_device(sg_fd);
  }

  return p;
}

struct jbod_short_profile honeybadger_get_short_profile (char *devname)
{
  struct jbod_short_profile p = {"N/A", "N/A", "N/A"};
  int sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);

  if (sg_fd > 0) {
    /*
    read_buffer_string(sg_fd, &node_sn, p.node_sn, MAX_TAG_LENGTH);
    read_buffer_string(sg_fd, &tray_asset, p.fb_asset_node, MAX_TAG_LENGTH);
    read_buffer_string(sg_fd, &chassis_tag, p.fb_asset_chassis, MAX_TAG_LENGTH);
    */
    sg_cmds_close_device(sg_fd);
  }

  return p;
}

struct jbod_interface knox = {
  knox_print_enclosure_info,
  jbod_print_temperature_reading,
  jbod_print_voltage_reading,
  jbod_print_current_reading,
  knox_print_power_reading,
  jbod_print_all_sensor_reading,
  jbod_print_hdd_info,
  jbod_hdd_power_control,
  jbod_hdd_led_control,
  jbod_print_fan_info,
  knox_control_fan_pwm,
  knox_set_asset_tag,
  knox_set_asset_tag_by_name,
  knox_print_asset_tag,
  knox_power_cycle_enclosure,
  knox_print_gpio,
  knox_print_event_log,
  knox_print_event_status,
  knox_print_sys_led,
  knox_control_sys_led,
  knox_show_config,
  knox_config_power_window,
  knox_config_hdd_temp_interval,
  knox_config_fan_profile,
  knox_identify_enclosure,
  knox_print_phyerr,
  knox_reset_phyerr,
  knox_print_profile,
  knox_get_short_profile,
};

struct jbod_interface honeybadger = {
  honeybadger_print_enclosure_info,
  jbod_print_temperature_reading,
  jbod_print_voltage_reading,
  jbod_print_current_reading,
  knox_print_power_reading,
  jbod_print_all_sensor_reading,
  jbod_print_hdd_info,
  jbod_hdd_power_control,
  jbod_hdd_led_control,
  jbod_print_fan_info,
  knox_control_fan_pwm,
  knox_set_asset_tag,
  knox_set_asset_tag_by_name,
  honeybadger_print_asset_tag,
  knox_power_cycle_enclosure,
  honeybadger_print_gpio,
  knox_print_event_log,
  honeybadger_print_event_status,
  honeybadger_print_sys_led,
  knox_control_sys_led,
  honeybadger_show_config,
  honeybadger_config_power_window,
  honeybadger_config_hdd_temp_interval,
  knox_config_fan_profile,
  honeybadger_identify_enclosure,
  honeybadger_print_phyerr,
  knox_reset_phyerr,
  knox_print_profile,
  honeybadger_get_short_profile,
};
