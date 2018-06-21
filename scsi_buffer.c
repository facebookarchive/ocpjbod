/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scsi_buffer.h"
#include "json.h"

int scsi_read_buffer(
  int sg_fd, int buffer_id, int buffer_offset,
  unsigned char *rsp, int max_rsp_size)
{

  return sg_ll_read_buffer(sg_fd, 1, buffer_id,
                           buffer_offset, (void *) rsp,
                           max_rsp_size, 1, 0);
}

int scsi_write_buffer(
  int sg_fd, int buffer_id, int buffer_offset,
  unsigned char *msg, int msg_size)
{

  return sg_ll_write_buffer(sg_fd, 1, buffer_id,
                            buffer_offset, (void *) msg,
                            msg_size, 1, 0);
}

void read_buffer_string(int sg_fd, struct scsi_buffer_parameter *sbp,
                        char *buf, int max_length)
{

  int read_length = max_length - 1 > sbp->len ? sbp->len : max_length - 1;

  scsi_read_buffer(sg_fd, sbp->buf_id, sbp->buf_offset,
                   (unsigned char *) buf, read_length);
  buf[read_length] = '\0';
  fix_none_ascii(buf, read_length);
}

void read_value_as_string(
    int sg_fd, struct scsi_buffer_parameter *sbp, char out[4096])
{
  unsigned char buf[4096];
  char *str, *esc_str;

  scsi_read_buffer(sg_fd, sbp->buf_id, sbp->buf_offset, buf, sbp->len);

  switch (sbp->type) {
    case sbp_integer:
      snprintf(
        out, 4096, "%d %s",
        sbp->to_int_callback(buf + sbp->value_offset), sbp->unit);
      break;
    case sbp_floatp:
      snprintf(
        out, 4096, "%.2f %s",
        sbp->to_float_callback(buf + sbp->value_offset), sbp->unit);
      break;
    case sbp_string:
      str = sbp->to_string_callback(
          buf + sbp->value_offset,
          sbp->len - sbp->value_offset);
      esc_str = str_escape(str);
      snprintf(
        out, 4096, "%s%s",
        str,
        sbp->unit);
      free(str);
      free(esc_str);
      break;
  }
}

void print_read_value(int sg_fd, struct scsi_buffer_parameter *sbp)
{
  char out[4096];
  read_value_as_string(sg_fd, sbp, out);

  IF_PRINT_NONE_JSON {
    printf("%s\t%s\n", sbp->name, out);
  }
  PRINT_JSON_LAST_ITEM(
    sbp->name, "%s", out);
}

int two_byte_to_int(unsigned char *buf)
{
  return (int)buf[0] * 256 + (int)buf[1];
}

int four_byte_to_int(unsigned char *buf)
{
  return (int)buf[0] * 256 * 256 * 256 + (int)buf[1] * 256 * 256 +
    (int)buf[2] * 256 + (int)buf[3];
}

unsigned int four_byte_to_uint(unsigned char *buf)
{
  return (unsigned int)buf[0] * 256 * 256 * 256 +
    (unsigned int)buf[1] * 256 * 256 +
    (unsigned int)buf[2] * 256 + (unsigned int)buf[3];
}

char *buf_to_string(unsigned char *buf, int len)
{
  char *ret;

  ret = (char *) malloc(len + 1);
  if (!ret) {
    return ret;
  }
  memcpy(ret, buf, len);
  ret[len] = '\0';

  fix_none_ascii(ret, len);
  return ret;
}
