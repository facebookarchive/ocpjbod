/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef EXPANDER_H
#define EXPANDER_H

#include "common.h"
#include "array_device_slot.h"

struct sas_expander {
  SAS_ADDR(sas_addr);
  SAS_ADDR_STR(sas_addr_str);
  char *name;
};

extern void print_expander_info(struct sas_expander *expander);

extern int extract_expander_info(
  unsigned char *expander_element,
  unsigned char *additional_expander_element,
  unsigned char *expander_description,
  struct sas_expander *expander,
  struct array_device_slot *slots);

#endif
