/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include <unistd.h>
#include <errno.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <getopt.h>

#include "options.h"
#include "jbod_interface.h"
#include "json.h"

#ifdef UTIL_VERSION
#define VERSION_STRING UTIL_VERSION
#else
#define VERSION_STRING "00.rc"
#endif

struct option long_options[] = {
  {"hdd-on",         required_argument,   0,    'O' },
  {"hdd-off",        required_argument,   0,    'o' },
  {"fault-on",       required_argument,   0,    'F' },
  {"fault-off",      required_argument,   0,    'f' },
  {"thresholds",     no_argument,         0,    't' },
  {"all",            no_argument,         0,    'a' },
  {"auto",           no_argument,         0,    'A' },
  {"pwm",            required_argument,   0,    'p' },
  {"tag",            required_argument,   0,    'T' },
  {"id",             required_argument,   0,    'i' },
  {"log",            no_argument,         0,    'l' },
  {"status",         no_argument,         0,    's' },
  {"power-win",      required_argument,   0,    'w' },
  {"hdd-temp-int",   required_argument,   0,    'H' },
  {"fan-profile",    required_argument,   0,    'P' },
  {"off",            no_argument,         0,    'D' },
  {"json",           no_argument,         0,    'j' },
  {"clear",          no_argument,         0,    'c' },
  {"detail",         no_argument,         0,    'd' },
  {"timeout",        required_argument,   0,    'm' },
  {"cold-storage",   no_argument,         0,    'z' },
  {0,                0,                   0,    0   },
};

char *short_options = "O:o:F:f:atpA:T:i:lsw:H:P:Djcdm:z";

int option_index = 0;

int print_json = 0;

/* extract device name from argc/argv */
static char *get_devname(int argc, char *argv[])
{

  optind = 1;
  while (getopt_long(argc, argv, short_options,
                     long_options, &option_index) != -1)
    ;

  if (optind >= argc) {
    usage(argc + 1, argv - 1);
    return NULL;
  }
  return argv[optind];  /* TODO: fix this */
}

/* list all JBODs */
int execute_list(int argc, char *argv[])
{
  char c;
  int show_detail = 0;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'd':
        show_detail = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  list_jbod(NULL, show_detail, 0);
  return 0;  /* TODO: t5933339 */
}

/* show sensor readings */
int execute_sensor(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int print_thresholds = 0;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 't':
        print_thresholds = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_all_sensor_reading(sg_fd, print_thresholds);
      PRINT_JSON_MORE_ITEM;
      jbod->print_power_reading(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

/* show HDD info, control HDD power on/off, fault */
int execute_hdd(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  char c;
  int hdd_on_id = -1;
  int hdd_off_id = -1;
  int fault_led_on_id = -1;
  int fault_led_off_id = -1;
  int show_all = 0;
  int jbod_count = 0;
  int timeout = 0;
  char jbod_names[MAX_JBOD_PER_HOST][PATH_MAX];
  int i;
  int ret;
  int cold_storage = 0;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'O':
        hdd_on_id = atoi(optarg);
        break;
      case 'o':
        hdd_off_id = atoi(optarg);
        break;
      case 'F':
        fault_led_on_id = atoi(optarg);
        break;
      case 'f':
        fault_led_off_id = atoi(optarg);
        break;
      case 'a':
        show_all = 1;
        break;
      case 'm':
        timeout = atoi(optarg);
        break;
      case 'z':
        cold_storage = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  if (timeout < 0) {
    perr("Cannot specify negative timeout, %d.\n", timeout);
    return 1;
  }

  if (hdd_on_id != -1 && hdd_off_id != -1) {
    perr("Cannot specify both hdd_on and hdd_off.\n");
    return 1;
  }

  if (fault_led_on_id != -1 && fault_led_off_id != -1) {
    perr("Cannot specify both fault_led_on and fault_led_off.\n");
    return 1;
  }

  if (show_all) {
    jbod_count = list_jbod(jbod_names, 0, 1);
    for (i = 0; i < jbod_count; ++i) {
      PRINT_JSON_RESET_GROUP;
      jbod = detect_dev(jbod_names[i]);
      if (jbod) {
        IF_PRINT_NONE_JSON
          printf(">>> %s \n", jbod_names[i]);
        sg_fd = sg_cmds_open_device(jbod_names[i], 0 /* rw */,
                                    0 /* not verbose */);
        if (sg_fd > 0) {
          if (i) PRINT_JSON_MORE_GROUP;
          PRINT_JSON_GROUP_HEADER(jbod_names[i]);
          jbod->print_hdd_info(sg_fd);
          PRINT_JSON_GROUP_ENDING;
          sg_cmds_close_device(sg_fd);
        }
      }
    }
    return 0;
  }

  ret = 0;
  devname = get_devname(argc, argv);
  sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
  if (sg_fd > 0) {
    jbod = detect_dev(devname);
    if (jbod) {
      if (hdd_on_id != -1) {
        ret = jbod->hdd_power_control(sg_fd, hdd_on_id, 1, timeout, cold_storage);
      } else if (hdd_off_id != -1) {
        ret = jbod->hdd_power_control(sg_fd, hdd_off_id, 0, timeout, cold_storage);
      } else if (fault_led_on_id != -1) {
        ret = jbod->hdd_led_control(sg_fd, fault_led_on_id, 1);
      } else if (fault_led_off_id != -1) {
        ret = jbod->hdd_led_control(sg_fd, fault_led_off_id, 0);
      }
      PRINT_JSON_GROUP_HEADER(devname);
      jbod->print_hdd_info(sg_fd);
      PRINT_JSON_GROUP_ENDING;
      sg_cmds_close_device(sg_fd);
    } else {
      perr("%s is not a jbod device\n", devname);
      ret = ENODEV;
    }
  }
  else {
    perr("No such file or directory as %s\n", devname);
    return ENOTDIR;
  }
  return ret;
}

/* show system level LEDs */
int execute_led(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_sys_led(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;
}

/* show fan rpm, control fan pwm */
int execute_fan(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int set_pwm = -1;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'p':
        set_pwm = atoi(optarg);
        break;
      case 'A':
        set_pwm = 0;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (set_pwm >= 0 && set_pwm <=100) {
        jbod->control_fan_pwm(sg_fd, set_pwm);
      } else {
        jbod->print_fan_info(sg_fd);
      }
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }

  return 0;  /* TODO: t5933339 */
}

/* show info of a JBOD enclosure */
int execute_info(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  struct jbod_profile *profile;

  devname = get_devname(argc, argv);
  profile = extract_profile(devname);
  jbod = detect_dev(devname);

  if (jbod && profile) {
    jbod->print_profile(profile);

    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_enclosure_info(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  free(profile);
  return 0;
}

/* power cycle expander */
int execute_power_cycle(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;

  devname = get_devname(argc, argv);

  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->power_cycle_enclosure(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

/* show GPIO values */
int execute_gpio(int argc, char *argv[]) {
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  struct jbod_profile *profile;

  devname = get_devname(argc, argv);

  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->print_gpio(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

/* show/change asset tags */
int execute_asset_tag(int argc, char *argv[]) {
  int tag_id = -1;
  char *tag_name = NULL;
  char *tag = NULL;
  char c;
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  struct jbod_profile *profile;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'i':
        tag_id = atoi(optarg);
        tag_name = optarg;
        break;
      case 'T':
        tag = optarg;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);

  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (tag != NULL && tag_id != -1) {
        if (tag_id == 0 && (strcmp("0", tag_name) != 0)) {

          /* atoi return 0 on none-"0", try tag_name */
          jbod->set_asset_tag_by_name(sg_fd, tag_name, tag);
        } else {
          /* atoi return right value */
          jbod->set_asset_tag(sg_fd, tag_id, tag);
        }
      }
      jbod->print_asset_tag(sg_fd);
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

/* show event status, event log */
int execute_event(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int show_status = 0;
  int show_log = 0;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 's':
        show_status = 1;
        break;
      case 'l':
        show_log = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (show_status) {
        jbod->print_event_status(sg_fd);
      } else if (show_log) {
        jbod->print_event_log(sg_fd);
      } else {
        usage(argc, argv);
      }
      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

int execute_config(int argc, char *argv[]) {
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int power_window = -1;
  int hdd_temp_int = -1;
  int fan_profile = -1;
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'w':
        power_window = atoi(optarg);
        break;
      case 'H':
        hdd_temp_int = atoi(optarg);
        break;
      case 'P':
        fan_profile = atoi(optarg);
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (power_window != -1) {
        jbod->config_power_window(sg_fd, power_window);
      }

      if (hdd_temp_int != -1) {
        jbod->config_hdd_temp_interval(sg_fd, hdd_temp_int);
      }

      if (fan_profile != -1) {
        jbod->config_fan_profile(sg_fd, fan_profile);
      }

      jbod->show_config(sg_fd);

      sg_cmds_close_device(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

int execute_identify(int argc, char *argv[])
{
  struct jbod_interface *jbod;
  char *devname;
  int sg_fd;
  int val = 1;  /* val=1 id on, val=0 id off */
  char c;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'D':
        val = 0;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);
  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      jbod->identify_enclosure(sg_fd, val);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

int execute_version(int argc, char *argv[])
{
  char c;

  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  IF_PRINT_NONE_JSON
    printf("Version: %s\n", VERSION_STRING);

  PRINT_JSON_LAST_ITEM("Version", "%s", VERSION_STRING);
  return 0;  /* TODO: t5933339 */
}

int execute_phyerr(int argc, char *argv[])
{
  char  c;
  char *devname;
  int sg_fd;
  struct jbod_interface *jbod;
  int clear = 0;

  optind = 1;
  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      case 'c':
        clear = 1;
        break;
      default:
        usage(argc, argv);
        return 1;
    }
  }

  devname = get_devname(argc, argv);
  jbod = detect_dev(devname);

  if (jbod) {
    sg_fd = sg_cmds_open_device(devname, 0 /* rw */, 0 /* not verbose */);
    if (sg_fd > 0) {
      if (clear)
        jbod->reset_phyerr(sg_fd);
      else
        jbod->print_phyerr(sg_fd);
    }
  } else {
    perr("%s is not a jbod device\n", devname);
    return ENODEV;
  }
  return 0;  /* TODO: t5933339 */
}

struct cmd_options all_cmds[] = {
  {LIST, "list", execute_list, "list all enclosures\n"
   "\t\t\t--detail        \t- show some details of each JBOD"},
  {INFO, "info", execute_info, "show info of the JBOD"},
  {SENSOR, "sensor", execute_sensor, "print sensor values\n"
   "\t\t\t--thresholds    \t- also prints thresholds"},
  {HDD, "hdd", execute_hdd, "hdd info/control\n"
   "\t\t\t--hdd-on  id    \t- turn on HDD\n"
   "\t\t\t--hdd-off id    \t- turn off HDD\n"
   "\t\t\t--fault-on  id  \t- turn on fault LED\n"
   "\t\t\t--fault-off id  \t- turn off fault LED\n"
   "\t\t\t--timeout <secs>\t- wait for drive on/off for up to <secs>\n"
   "\t\t\t--cold-storage  \t- special features for cold storage\n"
   "\t\t\t--all           \t- show HDDs from all JBODs"},
  {LED, "led", execute_led, "show status of chassis LEDs"},
  {FAN, "fan", execute_fan, "fan rpm/pwm\n"
   "\t\t\t--pwm  <pwm>    \t- set fan pwm\n"
   "\t\t\t--auto          \t- return pwm control to firmware"},
  {POWER_CYCLE, "power_cycle", execute_power_cycle,
   "power cycle the expander"},
  {GPIO, "gpio", execute_gpio, "show GPIO status"},
  {ASSET_TAG, "tag", execute_asset_tag, "show/change asset tag(s)\n"
   "\t\t\t--id <tag_id> --tag <tag>\t- update tag"},
  {EVENT, "event", execute_event, "show event log/status\n"
   "\t\t\t--log           \t- show event log\n"
   "\t\t\t--status        \t- show event status"},
  {CONFIG, "config", execute_config, "change configurations\n"
   "\t\t\t--power-win <sec> \t- config RMS window of power reading\n"
   "\t\t\t--hdd-temp-int <min> \t- config HDD temperature pooling interval\n"
   "\t\t\t--fan-profile <id> \t- select fan control profile"},
  {IDENTIFY, "identify", execute_identify, "identify enclosure tray\n"
   "\t\t\t--off \t- turn off identify (default is turn on)"},
  {PHYERR, "phyerr", execute_phyerr, "show/clear phy error counters\n"
   "\t\t\t--clear \t- clear phy error counters"},
  {VERSION, "version", execute_version, "show version number"},
};

void usage(int argc, char *argv[])
{
  int i;

  printf("%s command [options] sg_device\n", argv[0]);
  execute_version(argc, argv);
  printf("\n"
         "Global options:\n"
         "\t\t\t--json          \t- show output in JSON format\n\n"
         "Commands:\n");

  for (i = 0; i < sizeof(all_cmds) / sizeof(struct cmd_options); i ++) {
    printf("\t%10s\t%s\n\n", all_cmds[i].key_word, all_cmds[i].help_msg);
  }
}

void parse_global_options(int argc, char *argv[])
{
  char c;

  while ((c = getopt_long(argc, argv, short_options,
                          long_options, &option_index)) != -1) {
    switch(c) {
      CASE_JSON;
      default:
        break;
    }
  }
}

int parse_cmd(int argc, char *argv[])
{
  int i;
  int ret = 0;

  if (argc < 2) {
    usage(argc, argv);
    return 1;
  }

  parse_global_options(argc - 1, argv + 1);

  for (i = 0; i < sizeof(all_cmds) / sizeof(struct cmd_options); i ++) {
    if (strcmp(all_cmds[i].key_word, argv[1]) == 0) {
      JSON_HEADER;
      ret = all_cmds[i].execute(argc - 1, argv + 1);
      JSON_ENDING;
      return ret;
    }
  }
  usage(argc, argv);
  return 1;
}
