/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#ifndef KNOX_H
#define KNOX_H

#include "scsi_buffer.h"

static struct scsi_buffer_parameter power = {
  sbp_integer, 0x41, 0, 4, "Power", "W", 2, two_byte_to_int, NULL, NULL};

static struct scsi_buffer_parameter seb_pn = {
  sbp_string, 0x20, 0, 11, "SEB_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter seb_sn = {
  sbp_string, 0x20, 0x100, 11, "SEB_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter knox_dpb_pn = {
  sbp_string, 0x30, 0, 11, "DPB_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter knox_dpb_sn = {
  sbp_string, 0x30, 0x100, 11, "DPB_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter tray_pn = {
  sbp_string, 0x30, 0x200, 11, "Tray_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter tray_sn = {
  sbp_string, 0x30, 0x300, 12, "Tray_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter node_pn = {
  sbp_string, 0x30, 0x200, 11, "Node_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter node_sn = {
  sbp_string, 0x30, 0x300, 12, "Node_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter fcb_pn = {
  sbp_string, 0x40, 0, 11, "FCB_PN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter fcb_sn = {
  sbp_string, 0x40, 0x100, 11, "FCB_SN", "", 0, NULL, NULL, buf_to_string};

static struct scsi_buffer_parameter tray_asset = {
  sbp_string, 0x30, 0x500, 7, "FB_Asset_Node", "", 0, NULL, NULL,
  buf_to_string};

static struct scsi_buffer_parameter chassis_tag = {
  sbp_string, 0x40, 0x500, 7, "FB_Asset_Chassis", "", 0, NULL, NULL,
  buf_to_string};

static struct scsi_buffer_parameter rack_pos = {
  sbp_string, 0x40, 0x600, 2, "Rack_Position", "", 0, NULL, NULL,
  buf_to_string};

#endif
