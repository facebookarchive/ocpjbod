/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdio.h>
#include "cooling.h"
#include "json.h"
#include "ses.h"

void print_cooling_fan(struct cooling_fan *fan)
{
  IF_PRINT_NONE_JSON printf("Cooling: %s RPM: %d\n", fan->name, fan->rpm);

  PRINT_JSON_GROUP_HEADER(fan->name);
  PRINT_JSON_ITEM("name", "%s", fan->name);
  PRINT_JSON_LAST_ITEM("rpm", "%d", fan->rpm);
  PRINT_JSON_GROUP_ENDING;
}

int extract_cooling_fan_info(
  unsigned char *cooling_element,
  unsigned char *fan_description,
  struct cooling_fan *fan,
  int page_two_offset)
{

  fan->common_status = cooling_element[0];
  fan->rpm = 10 * ((int)(cooling_element[1] & 0x07) * 256 + cooling_element[2]);
  fan->name = copy_description(fan_description);
  fan->page_two_offset = page_two_offset;
  return 0;
}
