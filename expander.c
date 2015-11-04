/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdio.h>
#include <string.h>
#include "expander.h"
#include "ses.h"

void print_expander_info(struct sas_expander *expander)
{
  printf("Expander: %s SAS addr: 0x%s\n",
         expander->name, expander->sas_addr_str);
}

int extract_expander_info(
  unsigned char *expander_element,
  unsigned char *additional_expander_element,
  unsigned char *expander_description,
  struct sas_expander *expander,
  struct array_device_slot *slots)
{
  int phy_count = (int) additional_expander_element[4];
  int i;
  unsigned char phy_id, slot_id;

  verify_additional_element_eip_sas(additional_expander_element);

  memcpy(expander->sas_addr, additional_expander_element + 8, 8);

  print_sas_addr_a(expander->sas_addr, expander->sas_addr_str);
  expander->name = copy_description(expander_description);

  for (i = 0; i < phy_count; i ++) {
    phy_id = additional_expander_element[16 + 2 * i];
    slot_id = additional_expander_element[16 + 2 * i + 1];
    if (slot_id != 0xff) {
      slots[slot_id].phy = phy_id;
    }
#ifdef DEBUG
    printf("Expander phys: %d %d\n",
           additional_expander_element[16 + 2 * i],
           additional_expander_element[16 + 2 * i + 1]);
#endif /* DEBUG */
  }
  return 0;
}
