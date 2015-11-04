/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <string.h>
#include "enclosure_info.h"

int extract_enclosure_control(
  unsigned char *enclosure_element,
  int page_two_offset,
  struct enclosure_control *enclosurep)
{

  enclosurep->page_two_offset = page_two_offset;
  return 0;
}

int power_cycle_enclosure(
  unsigned char *page_two,
  struct enclosure_control *enclosurep)
{

  printf("Page 2 offset %d\n", enclosurep->page_two_offset);

  page_two[enclosurep->page_two_offset] &= 0xf0;
  page_two[enclosurep->page_two_offset] |= 0x80;

  page_two[enclosurep->page_two_offset + 2] = 0x40;
  page_two[enclosurep->page_two_offset + 3] = 0x04;

  return 0;
}
