#pragma once

// Set by tools/version.py on the device builds; the simulator uses these defaults.
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#ifndef BOARD_ID
#define BOARD_ID "sim"
#endif

// True when `candidate` ("1.2.3", a leading "v" is fine) is a higher release than `current`. A build that
// is not a release ("dev") never counts as older, so a development build is not overwritten.
#include <stdio.h>
inline bool versionParse(const char* s, int v[3]) {
  if (*s == 'v') s++;
  return sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}
inline bool versionNewer(const char* candidate, const char* current) {
  int c[3], k[3];
  if (!versionParse(candidate, c) || !versionParse(current, k)) return false;
  for (int i = 0; i < 3; i++)
    if (c[i] != k[i]) return c[i] > k[i];
  return false;
}
