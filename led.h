/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef LED_H
#define LED_H

enum led_type {SINGLE_COLOR, DUO_COLOR, SEVEN_SEG};

enum single_led_status {
  SINGLE_LED_OFF = 0x00,
  SINGLE_LED_ON = 0x01,
  SINGLE_LED_BLINK = 0x02,
  SINGLE_LED_UNKNOWN = 0x03,
  SINGLE_LED_AUTO = 0xff,
};

enum duo_color_led_status {
  DUO_LED_OFF = 0x00,
  DUO_LED_COLOR_ONE_ON = 0x01,
  DUO_LED_COLOR_TWO_ON = 0x10,
  DUO_LED_COLOR_ONE_BLINK = 0x02,
  DUO_LED_COLOR_TWO_BLINK = 0x20,
  DUO_LED_COLOR_ONE_COLOR_TWO = 0x22,
  DUO_LED_UNKNOWN = 0x33,
  DUO_LED_AUTO = 0xff,
};

struct led_info {
  enum led_type type;
  int status;
  char *desc;
  char *color_one;
  char *color_two;
};

extern void print_led(int id, struct led_info *info);

#endif
