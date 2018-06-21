/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <linux/pci_regs.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <json-c/json.h>
#include <switchtec/switchtec.h>

#include "jbof_interface.h"
#include "json.h"

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ULL
#endif

#define BMC_API_DEFAULT_URL "http://[fe80::1%usb0]:8080/api"
#define BMC_API_DEFAULT_SSH "sshpass -p 0penBmc ssh -o UserKnownHostsFile=/dev/null root@fe80::1%usb0"
#define BMC_REQUEST_RETRY_COUNT 5
// wait 5s to retry REST API
#define BMC_REQUEST_RETRY_INTERVAL_NS (NSEC_PER_SEC * 5)

#define SWITCHTEC_DEV_PATH "/dev/switchtec0"
#define FB_SWITCHTEC_MAX_STACKS (6)

typedef struct switchtec_dev switchtec_dev_t;
typedef struct switchtec_status switchtec_status_t;
typedef struct switchtec_evcntr_setup switchtec_evcntr_setup_t;

struct jbof_interface lightning;

typedef struct {
  void* data;
  size_t pos;
  size_t size;
} buffer_t;

size_t buffer_append(void* data, size_t size, size_t count, buffer_t* buffer) {
  size_t data_size = size * count;
  if (data_size > buffer->size - buffer->pos) {
    if (!buffer->data) {
      buffer->size = data_size;
      buffer->data = malloc(buffer->size);
    } else {
      buffer->size += data_size > buffer->size ? data_size : buffer->size;
      buffer->data = realloc(buffer->data, buffer->size);
    }
    assert(buffer->data != NULL);
    if (buffer->data == NULL) {
      return 0;
    }
  }
  memcpy(buffer->data + buffer->pos, data, data_size);
  buffer->pos += data_size;
  return data_size;
}

static json_object* bmc_request(const char* url, const char* args) {
  int retry_count;
  for (retry_count = 0; retry_count <= BMC_REQUEST_RETRY_COUNT; retry_count++) {
    CURL* curl = curl_easy_init();
    if (!curl) {
      perr("unable to initialize curl library\n");
      return NULL;
    }

    buffer_t buffer = {};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buffer_append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    if (args) {
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, args);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(args));
    }


    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) {
      perr("bmc request failed, url='%s', args='%s', error='%s'\n", url, args,
            curl_easy_strerror(result));
      free(buffer.data);
      if (retry_count >= BMC_REQUEST_RETRY_COUNT) {
        perr("Giving up after %d retries\n", BMC_REQUEST_RETRY_COUNT);
        return NULL;
      } else {
        struct timespec delay = {.tv_nsec = BMC_REQUEST_RETRY_INTERVAL_NS};
        nanosleep(&delay, NULL);
        continue;
      }
    }

    if (!buffer_append("\0", 1, 1, &buffer)) {
      perr("out of memory, url='%s', args='%s'\n", url, args);
      return NULL;
    }

    json_object* response = json_tokener_parse(buffer.data);
    free(buffer.data);
    if (!response) {
      perr("unable to parse bmc response, url='%s', args='%s'", url, args);
      return NULL;
    }

    if (args) {
      return response;
    }

    json_object* info = NULL;
    if (!json_object_object_get_ex(response, "Information", &info)) {
      perr("bmc response lacks <Information> field, url='%s', args='%s'\n",
           url, args);
      return NULL;
    }

    info = json_object_get(info);
    json_object_put(response);
    return info;
  }
  return NULL;
}

#define RET_ERR do { err = -1; goto cleanup; } while (0)
#define CHECK(x) do { err = x; if (err != 0) goto cleanup; } while (0)

#define config_read(fd, into, offset) \
  if(pread(fd, &into, sizeof(into), offset) != sizeof(into)) { \
    perror("read " #into " from pci config space"); \
    RET_ERR; \
  }

#define config_write(fd, into, offset) \
  if(pwrite(fd, &into, sizeof(into), offset) != sizeof(into)) { \
    perror("read " #into " from pci config space"); \
    RET_ERR; \
  }

#define PCI_EXP_TYPE_MASK(type) ((type) << 4)
#define PCI_EXP_SLTCAP_SLOT(slot_cap) (((slot_cap) & PCI_EXP_SLTCAP_PSN) >> 19)

#define PCI_EXP_SLTCTL_AIC 0x00c0 /* Attention Indicator Control */
#define PCI_EXP_SLTCTL_ATTN_IND_ON 0x0040 /* Attention Indicator on */
#define PCI_EXP_SLTCTL_ATTN_IND_BLINK 0x0080 /* Attention Indicator blinking */
#define PCI_EXP_SLTCTL_ATTN_IND_OFF 0x00c0 /* Attention Indicator off */

// wait 10ms when polling link status
#define LINK_POLL_INTERVAL_NS (NSEC_PER_SEC / 100)
// wait up to 10s total
#define LINK_POLL_TRIES 1000

static void pci_remove_children(char *devpath);

// search pci config space for a capability where
// cap_id == id && (cap_flags & flags_mask) == flags
static int pci_cfg_find_capability(
    int configfd,
    uint8_t id,
    uint16_t flags,
    uint16_t flags_mask,
    off_t *located) {
  int err = 0;
  uint8_t cap_offset;
  config_read(configfd, cap_offset, PCI_CAPABILITY_LIST);
  off_t pos = cap_offset;
  while (TRUE) {
    uint8_t cap_id;
    config_read(configfd, cap_id, pos + PCI_CAP_LIST_ID);
    uint8_t cap_next_offset;
    config_read(configfd, cap_next_offset, pos + PCI_CAP_LIST_NEXT);
    uint16_t cap_flags;
    config_read(configfd, cap_flags, pos + PCI_CAP_FLAGS);
    if (cap_id == id && (cap_flags & flags_mask) == flags) {
      *located = pos;
      err = 0;
      goto cleanup;
    }
    if (cap_next_offset == 0) {
      RET_ERR;
    }
    pos = cap_next_offset;
  }
cleanup:
  return err;
}

int pci_cfg_open(const char* path, int mode) {
  char config_path[PATH_MAX];
  snprintf(config_path, sizeof(config_path), "%s/config", path);
  int config_fd = open(config_path, mode);
  return config_fd;
}

#define pci_cfg_read(fd, offset, data) \
  (pread((fd), (data), sizeof(*(data)), (offset)) == sizeof(*(data)))

#define pci_cfg_write(fd, offset, data) \
  (pwrite((fd), (data), sizeof(*(data)), (offset)) == sizeof(*(data)))

static bool pci_cfg_write_bits(
    int fd,
    off_t offset,
    uint64_t mask,
    uint64_t bits) {
  uint64_t data = 0;
  if (!pci_cfg_read(fd, offset, &data)) {
    return false;
  }
  data = (data & ~mask) | bits;
  return pci_cfg_write(fd, offset, &data);
}

static off_t pci_cfg_find_pci_exp(int fd) {
  uint16_t status = 0;
  if (!pci_cfg_read(fd, PCI_STATUS, &status)) {
    perr("unable to read PCI_STATUS\n");
    return 0;
  }
  if ((status & PCI_STATUS_CAP_LIST) == 0) {
    perr("pci device has no capability list, status=%02x\n", status);
    return 0;
  }
  off_t pci_exp = 0;
  if (pci_cfg_find_capability(
        fd,
        PCI_CAP_ID_EXP,
        PCI_EXP_TYPE_MASK(PCI_EXP_TYPE_DOWNSTREAM) | PCI_EXP_FLAGS_SLOT,
        PCI_EXP_FLAGS_TYPE | PCI_EXP_FLAGS_SLOT,
        &pci_exp)) {
    return 0;
  }
  return pci_exp;
}

static bool wait_for_link_state(int fd, off_t pci_exp, bool state) {
  for (int i = 0; i < LINK_POLL_TRIES; ++i) {
    uint32_t link_status = 0;
    if (!pci_cfg_read(fd, pci_exp + PCI_EXP_LNKSTA, &link_status)) {
      perr("unable to read PCI_EXP_LNKSTA\n");
      return false;
    }
    bool link_state = (link_status & PCI_EXP_LNKSTA_DLLLA) != 0;
    if (link_state == state) {
      return true;
    }
    struct timespec delay = {.tv_nsec = LINK_POLL_INTERVAL_NS};
    nanosleep(&delay, NULL);
  }
  perr("link status poll attempts limit exceeded\n");
  return false;
}

static bool rescan_dev(const char* path) {
  char rescan_path[PATH_MAX];
  snprintf(rescan_path, PATH_MAX, "%s/rescan", path);
  int fd = open(rescan_path, O_WRONLY);
  if (fd == -1) {
    perr("unable to open sysfs rescan file, path='%s'\n", path);
    return false;
  }
  if (write(fd, "1\n", 2) != 2) {
    close(fd);
    perr("unable to write to sysfs rescan file, path='%s'\n", path);
    return false;
  }
  close(fd);
  return true;
}

static int set_slot_power(char* path, int power) {
  // if we're powering the port down, go ahead and try to nicely
  // remove anything connected to it
  if (!power) {
    pci_remove_children(path);
  }

  int fd = pci_cfg_open(path, O_RDWR);
  if (fd == -1) {
    perr("unable to open sysfs config file, path='%s'\n", path);
    return -1;
  }

  off_t pci_exp = pci_cfg_find_pci_exp(fd);
  if (!pci_exp) {
    close(fd);
    perr("unable to find pcie capability list, path='%s'\n", path);
    return -1;
  }

  if (!pci_cfg_write_bits(
        fd,
        pci_exp + PCI_EXP_SLTCTL,
        PCI_EXP_SLTCTL_PCC,
        power ? 0 : PCI_EXP_SLTCTL_PCC)) {
    close(fd);
    perr("unable to modify PCI_EXP_SLTCTL_PCC, path='%s'\n", path);
    return -1;
  }
  bool result = wait_for_link_state(fd, pci_exp, power);
  close(fd);

  result &= rescan_dev(path);
  return result ? 0 : -1;
}

static int pci_slot_num(const char* path) {
  int fd = pci_cfg_open(path, O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  off_t pci_exp = pci_cfg_find_pci_exp(fd);
  if (!pci_exp) {
    close(fd);
    return -1;
  }

  uint32_t slot_cap = 0;
  bool result = pci_cfg_read(fd, pci_exp + PCI_EXP_SLTCAP, &slot_cap);
  close(fd);
  if (!result) {
    perr("unable to read PCI_EXP_SLTCAP, path='%s'\n", path);
    return -1;
  }
  // assume that only slots that may contain drives have Attention Indicator
  // and Power Controller
  if (!(slot_cap & PCI_EXP_SLTCAP_AIP) || !(slot_cap & PCI_EXP_SLTCAP_PCP)) {
    return -1;
  }
  return PCI_EXP_SLTCAP_SLOT(slot_cap);
}

static int pci_link_state(const char* path) {
  int fd = pci_cfg_open(path, O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  off_t pci_exp = pci_cfg_find_pci_exp(fd);
  if (!pci_exp) {
    close(fd);
    return -1;
  }

  uint32_t link_status = 0;
  bool result = pci_cfg_read(fd, pci_exp + PCI_EXP_LNKSTA, &link_status);
  close(fd);
  if (!result) {
    perr("unable to read PCI_EXP_LNKSTA, path='%s'\n", path);
    return -1;
  }
  // check Data Link Layer Link Active flag
  return (link_status & PCI_EXP_LNKSTA_DLLLA) != 0;
}

#define WALK_STOP 1
static int walk_pci_dev(char* path,
                        int (*walkcb)(char *devpath, void* data),
                        void* ctx) {
  int err = 0;
  char cb_path[PATH_MAX];
  DIR* dir = opendir(path);
  struct dirent *e;
  if(!dir) {
    RET_ERR;
  }
  while((e = readdir(dir))) {
    if(e->d_name[0] == '0' &&
       e->d_type == DT_DIR) {
      snprintf(cb_path, PATH_MAX, "%s/%s", path, e->d_name);
      if (walkcb(cb_path, ctx) == WALK_STOP) {
        goto cleanup;
      }
      walk_pci_dev(cb_path, walkcb, ctx);
    }
  }
cleanup:
  if(dir) {
    closedir(dir);
  }
  return err;
}

static int walk_pci_switch(char* ctrldev,
                           int (*walkcb)(char *devpath, void* data),
                           void* ctx) {
  int err = 0;
  char curpath[PATH_MAX];
  char npath[PATH_MAX];
  if(realpath(ctrldev, curpath) == NULL) {
    RET_ERR;
  }
  strncat(curpath, "/../", PATH_MAX);
  if(realpath(curpath, npath) == NULL) {
    RET_ERR;
  }
  CHECK(walk_pci_dev(npath, walkcb, ctx));
cleanup:
  return err;
}

static int try_remove_dev(char* devpath, void* ctx) {
  int removefd = -1;
  char removepath[PATH_MAX];
  snprintf(removepath, PATH_MAX, "%s/remove", devpath);
  removefd = open(removepath, O_WRONLY);
  if (removefd == -1) return 0;
  write(removefd, "1\n", 2);
  close(removefd);
  return WALK_STOP;
}

static void pci_remove_children(char *devpath) {
  walk_pci_dev(devpath, try_remove_dev, NULL);
}

static bool init_flash_type(jbof_handle_t* handle) {
  bool result = false;
  json_object* flash = bmc_request(BMC_API_DEFAULT_URL "/pdpb/flash", NULL);
  if (!flash) {
    return result;
  }

  json_object* type = NULL;
  if (!json_object_object_get_ex(flash, "flash type", &type)) {
    perr("bmc flash info lacks <flash type> field\n");
  } else {
    const char* type_name = json_object_get_string(type);
    if (strcmp(type_name, "U2") == 0) {
      result = true;
      handle->flash_type = FLASH_TYPE_U2;

    } else if (strcmp(type_name, "M2") == 0) {
      result = true;
      handle->flash_type = FLASH_TYPE_M2;
    } else {
      perr("unknown flash type reported by bmc: flash_type=%s\n", type_name);
    }
  }

  json_object_put(flash);
  return result;
}

static void lightning_close(jbof_handle_t* handle) {
  free(handle);
}

#define ID_PFX "Lightning:pcidev="
#define ID_PFX_LEN (sizeof(ID_PFX) - 1)

static jbof_handle_t* lightning_open(const char* jbof_id) {
  if (strncmp(jbof_id, ID_PFX, ID_PFX_LEN) == 0) {
    jbof_handle_t *handle = calloc(sizeof(jbof_handle_t), 1);
    if (!handle) {
      return NULL;
    }
    memcpy(handle->jbof_id, jbof_id, strlen(jbof_id) + 1);
    snprintf(handle->switchpath,
        PATH_MAX, "/sys/bus/pci/devices/%s", jbof_id + ID_PFX_LEN);

    // verify that we are talking to a device that would be detected as a
    // Lightning tray
    struct jbof_profile *profile = jbof_detect_pci_dev(handle->switchpath);
    if (!profile || profile->interface != &lightning) {
      lightning_close(handle);
      return NULL;
    }

    if (!init_flash_type(handle)) {
      lightning_close(handle);
      return NULL;
    }

    return handle;
  }
  return NULL;
}

#define PCI_CLASS_STORAGE_EXPRESS 0x010802

typedef struct check_drive_ctx_t {
  json_object*  drives;
  flashtype_t   flash_type;
} check_drive_ctx_t;

void check_drive_add(
    check_drive_ctx_t* ctx,
    int slot,
    const char* devname,
    const char* pci_addr) {

  char str[32];
  if (ctx->flash_type == FLASH_TYPE_U2) {
    snprintf(str, sizeof(str), "%d", slot - 1);
  } else {
    snprintf(str, sizeof(str), "%d:%d", (slot - 1) / 2, (slot - 1) & 1);
  }

  json_object* drive = json_object_new_object();
  assert(drive);
  json_object_object_add(drive, "slot", json_object_new_string(str));

  if (devname) {
    char dev[32];
    snprintf(dev, sizeof(dev), "/dev/%s", devname);
    json_object_object_add(drive, "device", json_object_new_string(dev));

    char ns1[32];
    snprintf(ns1, sizeof(ns1), "/dev/%sn1", devname);
    json_object_object_add(drive, "devname", json_object_new_string(ns1));

    json_object_object_add(drive, "pci_addr", json_object_new_string(pci_addr));
  } else {
    json_object_object_add(drive, "device", NULL);
    json_object_object_add(drive, "devname", NULL);
    json_object_object_add(drive, "pci_addr", NULL);
  }

  char name[32];
  snprintf(name, sizeof(name), "ArrayDevice%02d", slot - 1);
  json_object_object_add(ctx->drives, name, drive);
}

static int check_drive(char* path, void* arg) {
  check_drive_ctx_t* ctx = arg;
  uint32_t pci_class;

  if (sysfs_read_uint32(path, "class", &pci_class) &&
      pci_class == PCI_CLASS_STORAGE_EXPRESS) {

    char port_path[PATH_MAX];
    snprintf(port_path, PATH_MAX, "%s/..", path);
    int slot = pci_slot_num(port_path);
    if (slot == -1) {
      return 0;
    }

    char driver_path[PATH_MAX];
    snprintf(driver_path, PATH_MAX, "%s/nvme", path);
    DIR* dir = opendir(driver_path);
    if (!dir) {
      return 0;
    }

    struct dirent *e;
    while ((e = readdir(dir))) {
      if (strncmp(e->d_name, "nvme", 4) == 0) {
        check_drive_add(ctx, slot, e->d_name, basename(path));
        break;
      }
    }
    closedir(dir);

  } else if (pci_link_state(path) == 0) {
    int slot = pci_slot_num(path);
    if (slot != -1) {
      check_drive_add(ctx, slot, NULL, NULL);
    }
  }
  return 0;
}

struct slot_walk_ctx {
  int slot;
  int power;
};

static int slot_power_walker(char *devpath, void *ctx) {
  struct slot_walk_ctx *wctx = ctx;
  int slot = pci_slot_num(devpath);
  if (slot != -1 && slot == wctx->slot) {
    set_slot_power(devpath, wctx->power);
    return WALK_STOP;
  }
  return 0;
}

static void lightning_set_slot_power(
    jbof_handle_t* handle,
    int slot,
    int power) {

  struct slot_walk_ctx ctx = {
    .slot = slot,
    .power = power,
  };
  walk_pci_switch(handle->switchpath, slot_power_walker, &ctx);
}

static int slot_remove_walker(char *devpath, void *ctx) {
  struct slot_walk_ctx *wctx = ctx;
  int slot = pci_slot_num(devpath);
  if (slot != -1 && slot == wctx->slot) {
    pci_remove_children(devpath);
    return WALK_STOP;
  }
  return 0;
}

static void lightning_remove_slot_dev(jbof_handle_t* handle, int slot) {
  struct slot_walk_ctx ctx = {
    .slot = slot,
  };
  walk_pci_switch(handle->switchpath, slot_remove_walker, &ctx);
}

static void lightning_print_drive_info(jbof_handle_t* handle) {
  json_object *enclosure = json_object_new_object();
  json_object *drives = json_object_new_object();
  check_drive_ctx_t ctx = {.drives=drives, .flash_type=handle->flash_type};
  walk_pci_switch(handle->switchpath, check_drive, &ctx);
  json_object_object_add(enclosure, handle->jbof_id, drives);
  if(print_json) {
    printf("%s\n", json_object_get_string(enclosure));
  } else {
    json_object_object_foreach(drives, drivedev, drive) {
      printf("%s\t", drivedev);
      json_object_object_foreach(drive, field, fv) {
        (void)field; /* unused */
        printf("%s\t", json_object_get_string(fv));
      }
      printf("\n");
    }
  }
  json_object_put(drives);
  json_object_put(enclosure);
}

// TODO detect openbmc usbnet interface rather than assuming usb0
// and if we ever have multiple trays, detect BMC<->Tray mapping
static void lightning_print_enclosure_info(jbof_handle_t* handle) {
  json_object* info = json_object_new_object();

  json_object_object_add(info, "fcb",
    bmc_request(BMC_API_DEFAULT_URL "/fcb/fruid", NULL));
  json_object_object_add(info, "pdpb",
    bmc_request(BMC_API_DEFAULT_URL "/pdpb/fruid", NULL));
  json_object_object_add(info, "peb",
    bmc_request(BMC_API_DEFAULT_URL "/peb/fruid", NULL));
  json_object_object_add(info, "flash",
    bmc_request(BMC_API_DEFAULT_URL "/pdpb/flash", NULL));

  if (print_json) {
    printf("%s\n",
        json_object_to_json_string_ext(info, JSON_C_TO_STRING_PRETTY));
  } else {
    json_object_object_foreach(info, board, bobj) {
      json_object_object_foreach(bobj, field, fv) {
        printf("(%s) %s: %s\n", board, field, json_object_get_string(fv));
      }
    }
  }

  json_object_put(info);
}

static void lightning_print_sensor_info(jbof_handle_t* handle) {
  json_object* sensors = json_object_new_object();
  if (!sensors) {
    perr("unable to allocated new json object\n");
    return;
  }

  json_object_object_add(sensors, "fcb",
    bmc_request(BMC_API_DEFAULT_URL "/fcb/sensors", NULL));
  json_object_object_add(sensors, "peb",
    bmc_request(BMC_API_DEFAULT_URL "/peb/sensors", NULL));
  json_object_object_add(sensors, "pdpb",
    bmc_request(BMC_API_DEFAULT_URL "/pdpb/sensors", NULL));

  if (print_json) {
    printf("%s\n",
      json_object_to_json_string_ext(sensors, JSON_C_TO_STRING_PRETTY));
  } else {
    json_object_object_foreach(sensors, board, bobj) {
      json_object_object_foreach(bobj, field, fv) {
        printf("(%s) %s: %s\n", board, field, json_object_get_string(fv));
      }
    }
  }

  json_object_put(sensors);
}

static void lightning_print_fan_info(jbof_handle_t* handle) {
  json_object* fans = bmc_request(BMC_API_DEFAULT_URL "/fcb/fans", NULL);
  if (!fans) {
    perr("unable to get fans info from bmc\n");
    return;
  }

  if (print_json) {
    printf("%s\n",
      json_object_to_json_string_ext(fans, JSON_C_TO_STRING_PRETTY));
  } else {
    json_object_object_foreach(fans, field, fv) {
      printf("%s: %s\n", field, json_object_get_string(fv));
    }
  }

  json_object_put(fans);
}

static void lightning_print_led_info(jbof_handle_t* handle) {
  json_object* leds = json_object_new_object();
  if (!leds) {
    perr("unable to allocated new json object\n");
    return;
  }

  json_object_object_add(leds, "health",
    bmc_request(BMC_API_DEFAULT_URL "/peb/health", NULL));
  json_object_object_add(leds, "identify",
    bmc_request(BMC_API_DEFAULT_URL "/peb/identify", NULL));

  if (print_json) {
    printf("%s\n",
      json_object_to_json_string_ext(leds, JSON_C_TO_STRING_PRETTY));
  } else {
    json_object_object_foreach(leds, board, bobj) {
      (void)board; /* unused */
      json_object_object_foreach(bobj, field, fv) {
        printf("%s: %s\n", field, json_object_get_string(fv));
      }
    }
  }

  json_object_put(leds);
}

static void lightning_identify(jbof_handle_t* handle, bool enable) {
  const char* args = enable ? "{\"action\":\"on\"}" : "{\"action\":\"off\"}";
  json_object_put(bmc_request(BMC_API_DEFAULT_URL "/peb/identify", args));
}

static int compare_status(const void* p1, const void* p2) {
  const switchtec_status_t* first = p1;
  const switchtec_status_t* second = p2;
  if (first->port.partition != second->port.partition) {
    return first->port.partition -  second->port.partition;
  }
  if (first->port.upstream != second->port.upstream) {
    return first->port.upstream - second->port.upstream;
  }
  return first->port.log_id -  second->port.log_id;
}

static void clear_phyerr_info(switchtec_dev_t* dev) {
  for (int i = 0; i < FB_SWITCHTEC_MAX_STACKS; ++i) {
    uint32_t values[SWITCHTEC_MAX_EVENT_COUNTERS];
    int result = switchtec_evcntr_get(dev, i, 0, SWITCHTEC_MAX_EVENT_COUNTERS,
                                      values, 1);
    if (result < 0) {
      perr("unable to clear event counters, path='%s', stack=%d, error=%d\n",
          SWITCHTEC_DEV_PATH, i, result);
    }
  }
}

static void print_port_evcntrs(
    switchtec_evcntr_setup_t* evcntrs,
    uint32_t* values,
    size_t count,
    uint8_t port) {
  bool first = true;
  for (int i = 0; i < count; ++i) {
    if (evcntrs[i].port_mask & (1 << port)) {
      int type_mask = evcntrs[i].type_mask;
      assert(type_mask);
      if (print_json) {
        if (!first) {
          printf(",\n");
        }
        first = false;
        printf("\t\t\t{\"%s\" : \"%d\"}",
            switchtec_evcntr_type_str(&type_mask), values[i]);
      } else {
        printf("  %10u\t%s\n",
            values[i], switchtec_evcntr_type_str(&type_mask));
      }
      assert(!type_mask);
    }
  }
}

static void print_port_phyerr_info(
    switchtec_dev_t* dev,
    switchtec_status_t* status,
    flashtype_t flash_type) {

  if (print_json) {
    printf("\t\"/Partition%02d/Stack%02d/Port%02d\" : {\n",
        status->port.partition,
        status->port.stack,
        status->port.stk_id);
    printf("\t\t\"type\" : \"%s\",\n", status->port.upstream ? "USP" : "DSP");
    if (status->port.upstream) {
      printf("\t\t\"slot\" : \"%d\",\n", status->port.log_id);
    } else if (flash_type == FLASH_TYPE_U2) {
      printf("\t\t\"slot\" : \"%d\",\n", status->port.log_id - 1);
    } else {
      printf("\t\t\"slot\" : \"%d:%d\",\n",
          (status->port.log_id - 1) / 2, (status->port.log_id - 1) & 1);
    }
    printf("\t\t\"phy\" : \"%d\",\n", status->port.phys_id);
    printf("\t\t\"status\" : \"%s\",\n", status->link_up ? "UP" : "DOWN");
    printf("\t\t\"counters\" : [\n");
  } else {
    printf("/Partition%02d/Stack%02d/Port%02d  %s_",
        status->port.partition,
        status->port.stack,
        status->port.stk_id,
        status->port.upstream ? "USP" : "DSP");
    if (status->port.upstream) {
      printf("%d  ", status->port.log_id);
    } else if (flash_type == FLASH_TYPE_U2) {
      printf("%d  ", status->port.log_id - 1);
    } else {
      printf("%d:%d  ",
          (status->port.log_id - 1) / 2, (status->port.log_id - 1) & 1);
    }
    printf("PHY_%02d  Status: %s\n",
        status->port.phys_id,
        status->link_up ? "UP" : "DOWN");
  }

  switchtec_evcntr_setup_t evcntrs[SWITCHTEC_MAX_EVENT_COUNTERS];
  uint32_t values[SWITCHTEC_MAX_EVENT_COUNTERS];
  int count = switchtec_evcntr_get_both(
                dev,
                status->port.stack,
                0,
                SWITCHTEC_MAX_EVENT_COUNTERS,
                evcntrs,
                values,
                0);
  if (count < 0) {
    perr("switchtec get evcntr request failed, stack=%d, error=%d\n",
        status->port.stack, count);
  } else {
    print_port_evcntrs(evcntrs, values, count, status->port.stk_id);
  }
  if (print_json) {
      printf("\n\t\t]\n");
      printf("\t}");
  }
}

static void lightning_print_phyerr_info(jbof_handle_t* handle, bool clear) {
  switchtec_dev_t* dev = switchtec_open(SWITCHTEC_DEV_PATH);
  if (!dev) {
    perr("unable to open switchtec device, path='%s'\n", SWITCHTEC_DEV_PATH);
    return;
  }
  switchtec_status_t* status = NULL;
  int count = switchtec_status(dev, &status);
  if (count < 0) {
    perr("switchtec port status request failed, error=%d\n", count);
    switchtec_close(dev);
    return;
  }
  assert(status);
  qsort(status, count, sizeof(*status), compare_status);

  if (print_json) {
    printf("{\n");
  }
  for (int i = 0; i < count; ++i) {
    if (status[i].port.partition != SWITCHTEC_UNBOUND_PORT) {
      if (print_json && i) {
        printf(",\n");
      }
      print_port_phyerr_info(dev, &status[i], handle->flash_type);
    }
  }
  if (print_json) {
    printf("\n}\n");
  }

  if (clear) {
    clear_phyerr_info(dev);
  }
  switchtec_status_free(status, count);
  switchtec_close(dev);
}

static void lightning_print_asset_tags(jbof_handle_t* handle) {
  json_object* assets = json_object_new_object();

  json_object_object_add(assets, "fcb",
    bmc_request(BMC_API_DEFAULT_URL "/fcb/fruid", NULL));
  json_object_object_add(assets, "pdpb",
    bmc_request(BMC_API_DEFAULT_URL "/pdpb/fruid", NULL));
  json_object_object_add(assets, "peb",
    bmc_request(BMC_API_DEFAULT_URL "/peb/fruid", NULL));

  int tag_id = 0;
  if (!print_json) {
    printf("ID\tName\tValue\n");
  }
  json_object_object_foreach(assets, board, obj) {
    json_object* value = NULL;
    if (json_object_object_get_ex(obj, "Product Asset Tag", &value)) {
      if (print_json) {
        json_object_object_add(assets, board, value);
      } else {
        printf("%d\t%s\t%s\n", tag_id, board, json_object_get_string(value));
        ++tag_id;
      }
    }
  }
  if (print_json) {
    printf("%s\n",
      json_object_to_json_string_ext(assets, JSON_C_TO_STRING_PRETTY));
  }

  json_object_put(assets);
}

static int slot_set_fault_led(const char* path, bool enable) {
  int fd = pci_cfg_open(path, O_RDWR);
  if (fd == -1) {
    return -1;
  }

  off_t pci_exp = pci_cfg_find_pci_exp(fd);
  if (!pci_exp) {
    return -1;
  }

  uint32_t slot_cap = 0;
  if (!pci_cfg_read(fd, pci_exp + PCI_EXP_SLTCAP, &slot_cap)) {
    close(fd);
    return -1;
  }
  if ((slot_cap & PCI_EXP_SLTCAP_AIP) == 0) {
    close(fd);
    return -1;
  }

  if (!pci_cfg_write_bits(
        fd,
        pci_exp + PCI_EXP_SLTCTL,
        PCI_EXP_SLTCTL_AIC,
        enable ? PCI_EXP_SLTCTL_ATTN_IND_BLINK : PCI_EXP_SLTCTL_ATTN_IND_OFF)) {
    close(fd);
    return -1;
  }

  return 0;
}

typedef struct {
  int slot;
  bool enable;
} slot_fault_led_param_t;

static int slot_fault_led_cb(char* path, void* arg) {
  slot_fault_led_param_t* param = arg;
  int slot = pci_slot_num(path);
  if (slot != param->slot) {
    return 0;
  }
  if (slot_set_fault_led(path, param->enable)) {
    perr("unable to change fault led state, path='%s'\n", path);
  }
  return WALK_STOP;
}

static void lightning_set_slot_fault_led(
    jbof_handle_t* handle,
    int slot,
    bool enable) {
  slot_fault_led_param_t param = {.slot = slot, .enable = enable};
  walk_pci_switch(handle->switchpath, slot_fault_led_cb, &param);
}

static void lightning_set_precool_mode(jbof_handle_t* handle, bool enable) {
  if (handle->flash_type != FLASH_TYPE_M2 ||
      strcmp(handle->vendor_name, "seagate") != 0) {
    perr("precool mode is valid only for seagate m.2\n");
    return;
  }
  char payload[PATH_MAX];
  const char* cmds[] = {
    "sv start fscd",
    "sv stop fscd && /usr/local/bin/fan-util --set 90",
  };
  snprintf(payload, PATH_MAX, "%s \"%s\"", BMC_API_DEFAULT_SSH, cmds[enable]);
  int result = system(payload);
  if (result) {
    perr("unable to set precool mode, error=%d, enable=%d\n", result, enable);
  }
}

struct jbof_interface lightning = {
  .open = lightning_open,
  .close = lightning_close,
  .identify = lightning_identify,
  .print_enclosure_info = lightning_print_enclosure_info,
  .print_drive_info = lightning_print_drive_info,
  .print_sensor_info = lightning_print_sensor_info,
  .print_fan_info = lightning_print_fan_info,
  .print_led_info = lightning_print_led_info,
  .print_phyerr_info = lightning_print_phyerr_info,
  .print_asset_tags = lightning_print_asset_tags,
  .set_slot_power = lightning_set_slot_power,
  .remove_slot_dev = lightning_remove_slot_dev,
  .set_slot_fault_led = lightning_set_slot_fault_led,
  .set_precool_mode = lightning_set_precool_mode,
};
