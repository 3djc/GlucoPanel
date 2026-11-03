#pragma once
#ifdef ARDUINO  // device only
#include <WiFiClientSecure.h>

// The SDK embeds the Mozilla root certificate list (~68 KB, already part of the firmware).
extern const uint8_t rootca_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_end[] asm("_binary_x509_crt_bundle_end");

// A TLS client that checks servers against the embedded root certificates and keeps doing so after stop().
//
// Arduino-ESP32 3.3.x bug: setCACertBundle() remembers the bundle in two places, and stop() wipes one of them
// (the whole TLS context is zeroed) but not the other. The next connection made with the same client then fails
// with "SSL - No CA Chain is set, but required to operate". HTTPClient stops and reconnects the client when it
// follows a redirect, which is what GitHub does for the update manifest and the firmware download.
class BundleClient : public WiFiClientSecure {
 public:
  void useBundle() {
    wantBundle = true;
    setCACertBundle(rootca_start, rootca_end - rootca_start);
  }

  void stop() override {
    WiFiClientSecure::stop();
    if (wantBundle) setCACertBundle(rootca_start, rootca_end - rootca_start);
  }

 private:
  bool wantBundle = false;
};
#endif
