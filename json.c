/**
 * Copyright (c) 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "json.h"

#include <stdio.h>
#include <stdlib.h>

int do_new_item = 0;
int print_json = 0;

char* str_escape(const char* src) {
  size_t i, j, max_len = 256;
  char *dst;

  dst = malloc(max_len);
  if (dst == NULL)
    return dst;

  for (i = 0, j = 0; src[i]; ++i) {
    if (j + 6 >= max_len) {
      max_len = max_len << 1;
      dst = realloc(dst, max_len);
      if (dst == NULL) {
        return dst;
      }
    }
    // ASCII
    if (src[i] >= ' ' && src[i] < 127 && src[i] != '\"' && src[i] != '\\') {
      dst[j] = src[i];
      ++j;
    } else {
      // Special cases
      dst[j]='\\';
      ++j;
      switch (src[i]) {
      case '\"': dst[j]='\"'; ++j; break;
      case '\\': dst[j]='\\'; ++j; break;
      case '\b': dst[j]='b'; ++j; break;
      case '\f': dst[j]='f'; ++j; break;
      case '\n': dst[j]='n'; ++j; break;
      case '\r': dst[j]='r'; ++j; break;
      case '\t': dst[j]='t'; ++j; break;
      default:
        // Assume Latin-1 encoding
        snprintf(&dst[j], 5, "u%04x", *src);
        j += 5;
        break;
      }
    }
  }

  dst[j] = 0;
  return dst;
}
