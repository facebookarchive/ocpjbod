/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef SCSI_BUFFER_H
#define SCSI_BUFFER_H

/* read buffer with mode=1 */
extern int scsi_read_buffer(
  int sg_fd, int buffer_id, int buffer_offset,
  unsigned char *rsp, int max_rsp_size);

/* write buffer with mode=1 */
extern int scsi_write_buffer(
  int sg_fd, int buffer_id, int buffer_offset,
  unsigned char *msg, int msg_size);

/* directions to interpret buffer parameter */
typedef enum {sbp_integer, sbp_floatp, sbp_string} value_type;

struct scsi_buffer_parameter {
  value_type type;

  /* parameter of the buffer */
  int buf_id;
  int buf_offset;
  int len;

  /* paremeter name and unit */
  const char *name;
  const char *unit;

  /* offset of the value in the read data */
  int value_offset;

  /* how to interpret the data */
  int (*to_int_callback) (unsigned char *buf);
  float (*to_float_callback) (unsigned char *buf);
  char *(*to_string_callback) (unsigned char *buf, int len);
};

extern void read_value_as_string(
    int sg_fd, struct scsi_buffer_parameter *sbp, char out[4096]);

/* read the value and print it */
extern void print_read_value(int sg_fd, struct scsi_buffer_parameter *sbp);

/* two byte to a integer*/
extern int two_byte_to_int(unsigned char *buf);

/* four byte to a integer*/
extern int four_byte_to_int(unsigned char *buf);

/* four byte to a unsigned integer*/
extern unsigned int four_byte_to_uint(unsigned char *buf);
/* copy data from buffer to a string */
extern char *buf_to_string(unsigned char *buf, int len);

extern void read_buffer_string(int sg_fd, struct scsi_buffer_parameter *sbp,
                               char *buf, int max_length);
#endif
