#pragma once
#include <Arduino.h>
#include <vector>

struct HttpHeader {
  String name, value;
};

struct HttpResponse {
  int status = 0;  // HTTP status; <= 0 means a transport error (see error)
  String body;
  String error;
};

// Blocking HTTP(S) request. The device build lives in http.cpp, the sim build in sim/http_sim.cpp.
HttpResponse httpRequest(const char* method, const String& url, const String& body,
                         const std::vector<HttpHeader>& headers);
