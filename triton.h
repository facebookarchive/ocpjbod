/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef TRITON_H
#define TRITON_H

#include "scsi_buffer.h"

#if 0  /* TODO: implement code that uses this (currently unused). */
static struct scsi_buffer_parameter scc_power = {
  sbp_integer, 0x41, 0, 4, "SCC_Power", "W", 2, two_byte_to_int, NULL, NULL};

static struct scsi_buffer_parameter dpb_power = {
  sbp_integer, 0x41, 1, 4, "DPB_Power", "W", 2, two_byte_to_int, NULL, NULL};
#endif

static struct scsi_buffer_parameter chassis_power = {
  sbp_integer, 0x41, 2, 4, "Chassis_Power", "W", 2, two_byte_to_int, NULL,
  NULL};

static struct scsi_buffer_parameter scc_pn = {
  sbp_string, 0x40, 0, 13, "SCC_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter scc_sn = {
  sbp_string, 0x40, 0x100, 20, "SCC_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter triton_dpb_pn = {
  sbp_string, 0x30, 0, 13, "DPB_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter triton_dpb_sn = {
  sbp_string, 0x30, 0x100, 20, "DPB_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter ww_chassis_pn = {
  sbp_string, 0x30, 0x200, 11, "Chassis_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter ww_chassis_sn = {
  sbp_string, 0x30, 0x300, 13, "Chassis_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter fb_pn = {
  sbp_string, 0x30, 0x400, 13, "FB_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter fb_asset_tag = {
  sbp_string, 0x30, 0x500, 7, "asset_tag", "", 0, NULL, NULL, buf_to_string};

#endif
