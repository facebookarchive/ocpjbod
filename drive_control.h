/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
#ifndef DRIVE_CONTROL_H
#define DRIVE_CONTROL_H

#include "common.h"

/* remove HDD from OS by echo into /sys/block/sdXXX/device/delete */
int remove_hdd(char *devname, const char *sas_addr_str);

#endif
