/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "json.h"
#include "led.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void print_led(int id, struct led_info *info)
{
  char val[64];

  if (info->desc == NULL)
      return;

  if (info->type == SINGLE_COLOR) {
    switch (info->status) {
      case SINGLE_LED_OFF:
        snprintf(val, 64, "Off");
        break;
      case SINGLE_LED_ON:
        snprintf(val, 64, info->color_one);
        break;
      case SINGLE_LED_BLINK:
        snprintf(val, 64, "Blink %s", info->color_one);
        break;
      default:
        snprintf(val, 64, "Unknown");
    }
  } else if (info->type == DUO_COLOR) {
    switch (info->status) {
      case DUO_LED_OFF:
        snprintf(val, 64, "Off");
        break;
      case DUO_LED_COLOR_ONE_ON:
        snprintf(val, 64, info->color_one);
        break;
      case DUO_LED_COLOR_ONE_BLINK:
        snprintf(val, 64, "Blink %s", info->color_one);
        break;
      case DUO_LED_COLOR_TWO_ON:
        snprintf(val, 64, "%s", info->color_two);
        break;
      case DUO_LED_COLOR_TWO_BLINK:
        snprintf(val, 64, "Blink %s", info->color_two);
        break;
      case DUO_LED_COLOR_ONE_COLOR_TWO:
        snprintf(val, 64, "Blink %s %s", info->color_one, info->color_two);
      default:
        snprintf(val, 64, "Unknown");
    }
  } else { // info->type == SEVEN_SEG
    snprintf(val, 64, "0x%02x", info->status);
  }

  IF_PRINT_NONE_JSON printf("%d\t%s\t%s\n", id, info->desc, val);

  PRINT_JSON_GROUP_SEPARATE;

  PRINT_JSON_GROUP_HEADER(info->desc);
  PRINT_JSON_ITEM("id", "%d", id);
  PRINT_JSON_ITEM("name", "%s", info->desc);
  PRINT_JSON_LAST_ITEM("status", "%s", val);
  PRINT_JSON_GROUP_ENDING;
}
