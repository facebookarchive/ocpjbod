/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "common.h"

void print_sas_addr_a(unsigned char *sas_addr, char *sas_addr_str)
{
  int i;
  for (i = 0; i < SAS_ADDR_LENGTH; i ++) {
    snprintf(sas_addr_str + 2 * i, SAS_ADDR_STR_LENGTH - 2 * i,
             "%02x", sas_addr[i]);
  }
}

int sas_addr_invalid(unsigned char *addr)
{
  if (addr == NULL)
    return 1;

  /* if first 4 bytes are all zero, the address is invalid */
  return (addr[0] == 0 && addr[1] == 0 && addr[2] == 0 && addr[3] == 0);
}
