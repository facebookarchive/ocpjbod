/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <scsi/sg_lib.h>

#include "common.h"
#include "drive_control.h"

/* /dev/sdXX => sdXX */
char *dev_short_name(const char *devname)
{
  if (!devname)
    return NULL;
  return strstr(devname, "sd");
}

static char *sysfs_scsi_disk_handle(const char *devname, char *fname,
                                    const char *handle)
{
  DIR *dir;
  struct dirent *ent;
  char *shortname = dev_short_name(devname);

  if (!devname || !fname || !handle || !shortname)
    return NULL;

  snprintf(fname, PATH_MAX, "/sys/block/%s/device/scsi_disk", shortname);
  if ((dir = opendir(fname)) == NULL) {
    perr("Cannot open directory %s.\n", fname);
    return NULL;
  }

  fname[0] = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] == '.')  /* skip . and .. */
      continue;
    snprintf(fname, PATH_MAX,
             "/sys/block/%s/device/scsi_disk/%s/%s",
             shortname, ent->d_name, handle);
    break;
  }
  closedir(dir);
  if (fname[0] == 0) {
    perr("Cannot find valid entry in %s.\n", fname);
    return NULL;
  }

  return fname;
}

/* used to do echo XXX > /sys/block/sdXX/device/... */
static int write_string_to_file(const char *fname, const char *s)
{
  int fd, ret;

  if (!fname)
    return 1;

  fd = open(fname, O_WRONLY);
  if (fd < 0) {
    perr("Cannot open %s.\n", fname);
    return 1;
  }

  ret = write(fd, s, strlen(s));
  if (ret != strlen(s)) {
    perr("Failed to write %s to %s\n", s, fname);
    close(fd);
    return 1;
  }
  fsync(fd);
  ret = close(fd);
  if (ret != 0)
    perr("Failed to close %s (ret=%d).\n", fname, ret);
  return ret;
}

static int enable_manage_start_stop(const char *devname)
{
  char fname[PATH_MAX];

  if (!devname)
    return 1;

  if (sysfs_scsi_disk_handle(devname, fname, "manage_start_stop") == NULL)
    return 1;
  return write_string_to_file(fname, "1");
}

/*
 * In older kernel (say 3.10), SCSI device delete is handled asynchronizely,
 * So we need to check that the device is actually deleted.
 *
 * return 0 on successful delete; and 1 on timeout
 */
int wait_device_delete(char *sys_device_path, int timeout)
{
  int count = 0;
  struct stat s = {0};

  if (!sys_device_path || timeout <= 0)
    return 1;

  while (count < timeout) {
    if (stat(sys_device_path, &s) < 0) /* path deleted, done */
      return 0;
    ++count;
    sleep(1);
  }
  perr("wait_device_delete failed after %d seconds.\n", timeout);
  return 1;
}

/* returns 1 for match,
 *         0 for not match
 */
int sas_address_matches(const char *shortname, const char *sas_addr_str)
{
  char sas_address_file[PATH_MAX];
  char buf[SAS_ADDR_STR_LENGTH + 4] = {0}; /* "0x" + sas address str + "\n\0" */
  int fd, readlen;

  if (!shortname || !sas_addr_str)
    return 0;  /* not match */
  snprintf(sas_address_file, PATH_MAX,
           "/sys/block/%s/device/sas_address", shortname);

  fd = open(sas_address_file, O_RDONLY);
  if (fd < 0) {
    perr("Cannot open %s\n", sas_address_file);
    return 0;
  }

  readlen = read(fd, buf, SAS_ADDR_STR_LENGTH + 2);
  close(fd);

  if (readlen != SAS_ADDR_STR_LENGTH + 2)
    return 0;

  if (strncmp(buf + 2, sas_addr_str, SAS_ADDR_STR_LENGTH) == 0)
    return 1;
  return 0;
}

/* returns 1 for device in "running" state,
 *         0 for any other state
 */
int hdd_in_running_state(const char *shortname)
{
  char device_state_file[PATH_MAX];
  char buf[64] = {0}; /* 0x + sas address in str */
  int fd;
  const char *running_str = "running";

  if (!shortname)
    return 0;  /* not running */

  snprintf(device_state_file, PATH_MAX,
           "/sys/block/%s/device/state", shortname);

  fd = open(device_state_file, O_RDONLY);
  if (fd < 0) {
    perr("Cannot open %s\n", device_state_file);
    return 0;
  }

  read(fd, buf, 64);
  close(fd);

  if (strncmp(buf, running_str, strlen(running_str)) == 0)
    return 1;
  return 0;
}

int remove_hdd(const char *devname, const char *sas_addr_str)
{
  char sysfs_handle[PATH_MAX];
  char sys_disk_device_path[PATH_MAX];  /* /sys/block/sdX/device */
  char sys_device_path[PATH_MAX];  /* /sys/devices/X, for wait_device_delete */
  char *shortname = dev_short_name(devname);

  if (!devname || !shortname || !sas_addr_str)
    return 1;

  if (enable_manage_start_stop(devname)) {
    perr("Failed to enable manage_start_stop for device %s.\n", devname);
  }

  snprintf(sys_disk_device_path, PATH_MAX, "/sys/block/%s/device/", shortname);
  if (!realpath(sys_disk_device_path, sys_device_path)) {
    perr("Device %s is not available.\n", sys_disk_device_path);
    return 1;
  }

  /* IF SAS address of device does not matches SAS address from expander
   * OR the device is not in "running" state
   * skip echo 1 > delete
   */
  if (sas_address_matches(shortname, sas_addr_str) == 0 ||
      hdd_in_running_state(shortname) == 0)
    return 1;

  snprintf(sysfs_handle, PATH_MAX, "/sys/block/%s/device/delete", shortname);
  if (write_string_to_file(sysfs_handle, "1") != 0)
    perr("Failed to write 1 to %s.\n", sysfs_handle);

  return wait_device_delete(sys_device_path, 30);
}
