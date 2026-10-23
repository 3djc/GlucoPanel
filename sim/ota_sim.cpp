// Simulator stand-in for ota.cpp: a pretend newer release, and an install that fails after a pause.
#include "ota.h"
#include <unistd.h>

static OtaInfo last;

OtaInfo otaCheck() {
  last = OtaInfo();
  last.ok = true;
  last.newer = true;
  last.version = "9.9.9";
  last.checked = time(nullptr);
  return last;
}

const OtaInfo& otaLast() { return last; }

bool otaInstall(const OtaInfo&, String& error) {
  sleep(2);
  error = "hash mismatch";
  return false;
}

void otaTick(bool) {}
