/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <signal.h>

#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include "ses.h"
#include "enclosure_info.h"
#include "scsi_buffer.h"
#include "jbod_interface.h"
#include "options.h"

int main(int argc, char *argv[])
{
  (void) prctl(PR_SET_PDEATHSIG, SIGINT, 0, 0, 0);
  return parse_cmd(argc, argv);
}
