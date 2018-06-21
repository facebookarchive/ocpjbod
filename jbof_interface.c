/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <stdint.h>
#include <json-c/json.h>

#define SYSFS_READ_IMPL
#include "jbof_interface.h"
#include "json.h"

#include "lightning.c"

#define SCSI_BUFFER_MAX_SIZE 64

static const char *pci_devs_path = "/sys/bus/pci/devices/";

struct jbof_profile jbof_library[] = {
  // switchtec pcie switch
  {0x11f8, 0x8536, 0, 0, 0x58000, &lightning, "Lightning"},
};

static int jbof_library_size =
  sizeof(jbof_library) / sizeof(struct jbof_profile);


struct jbof_profile *jbof_detect_pci_dev(char *pcidev_path) {
  uint16_t vendor = 0;
  uint16_t device = 0;
  uint16_t subsystem_device = 0;
  uint16_t subsystem_vendor = 0;
  uint32_t class_code = 0;
  struct jbof_profile *profile;
  int i;
  if (sysfs_read_uint16(pcidev_path, "vendor", &vendor) &&
      sysfs_read_uint16(pcidev_path, "device", &device) &&
      sysfs_read_uint16(pcidev_path, "subsystem_vendor", &subsystem_vendor) &&
      sysfs_read_uint16(pcidev_path, "subsystem_device", &subsystem_device) &&
      sysfs_read_uint32(pcidev_path, "class", &class_code)) {
    for (i = 0; i < jbof_library_size; i++) {
      profile = &jbof_library[i];
      if (profile->vendor != 0 && profile->vendor != vendor) continue;
      if (profile->device != 0 && profile->device != device) continue;
      if (profile->subsystem_vendor != 0 &&
          profile->subsystem_vendor != subsystem_vendor) continue;
      if (profile->subsystem_device != 0 &&
          profile->subsystem_device != subsystem_device) continue;
      if (profile->class_code != 0 &&
          profile->class_code != class_code) continue;
      return profile;
    }
  }
  return NULL;
}

struct jbof_profile *lookup_jbof_profile(char* jbofid) {
  char *saveptr;
  char local_jbof_id[JBOF_ID_MAX];
  struct jbof_profile* profile;
  char *profile_name;
  int i;

  if (!jbofid) return NULL;

  memcpy(local_jbof_id, jbofid, strlen(jbofid) + 1);
  profile_name = strtok_r(local_jbof_id, ":", &saveptr);

  for (i = 0; i < jbof_library_size; i++) {
    profile = &jbof_library[i];
    if (strcmp(profile_name, profile->name) == 0) {
      return profile;
    }
  }
  return NULL;
}

int list_jbof(char jbof_names[MAX_JBOF_PER_HOST][PATH_MAX],
              int show_detail, int quiet)
{
  DIR *dir;
  struct dirent *ent;
  struct jbof_profile *profile;
  char pcidev_path[PATH_MAX];
  char jbof_id[JBOF_ID_MAX];
  int jbof_count = 0;
  const char *header_string = "jbof_id\tname\n";
  json_object *enclosure_list = json_object_new_object();

  if (!quiet) {
    IF_PRINT_NONE_JSON printf("%s", header_string);
  }
  if ((dir = opendir(pci_devs_path)) != NULL) {
    while ((ent = readdir (dir)) != NULL) {
      snprintf(pcidev_path, PATH_MAX, "%s%s", pci_devs_path, ent->d_name);
      profile = jbof_detect_pci_dev(pcidev_path);
      if (profile) {
        if (jbof_count >= MAX_JBOF_PER_HOST) {
          perr("too many jbofs found, may be unable to access some of them\n");
          return jbof_count;
        }
        snprintf(jbof_id, JBOF_ID_MAX, "%s:pcidev=%s",
                 profile->name, ent->d_name);
        if (jbof_names) {
          memcpy(jbof_names[jbof_count], jbof_id, strlen(jbof_id) + 1);
        }
        jbof_count++;
        if (!quiet) {
          if (!print_json) {
            printf("%s\t%s\n", jbof_id, profile->name);
          } else {
            json_object *enclosure = json_object_new_object();
            json_object_object_add(enclosure, "jbof_id",
                json_object_new_string(jbof_id));
            json_object_object_add(enclosure, "name",
                json_object_new_string(profile->name));
            json_object_object_add(enclosure_list, jbof_id, enclosure);
          }
        }
      }
    }
    closedir(dir);
  }
  if (!quiet && print_json) {
    // strip off outer { and }
    char *jstr = strdup(json_object_get_string(enclosure_list));
    jstr[strlen(jstr) - 1] = 0;
    printf("%s\n", jstr + 1);
    free(jstr);
  }
  json_object_put(enclosure_list);
  return jbof_count;
}
