// Simulator HTTP client (libcurl). With SIM_API_BASE=http://localhost:9000 every request is sent
// to that base instead (path kept, original host passed in X-Orig-Host) -- see tools/mock_cgm.py.
#include <curl/curl.h>
#include "http.h"

static size_t onData(char* p, size_t s, size_t n, void* u) {
  ((std::string*)u)->append(p, s * n);
  return s * n;
}

HttpResponse httpRequest(const char* method, const String& url0, const String& body,
                         const std::vector<HttpHeader>& headers) {
  HttpResponse r;
  std::string url = url0, origHost;
  struct curl_slist* hl = nullptr;
  if (const char* base = getenv("SIM_API_BASE")) {
    size_t p = url.find("://"), e = url.find('/', p + 3);
    origHost = url.substr(p + 3, e == std::string::npos ? std::string::npos : e - p - 3);
    url = std::string(base) + (e == std::string::npos ? "" : url.substr(e));
    hl = curl_slist_append(hl, ("X-Orig-Host: " + origHost).c_str());
  }
  for (const HttpHeader& h : headers) hl = curl_slist_append(hl, (h.name + ": " + h.value).c_str());

  std::string out;
  CURL* c = curl_easy_init();
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, method);
  if (strcmp(method, "GET")) {
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)body.size());
  }
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, hl);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, onData);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
  CURLcode rc = curl_easy_perform(c);
  long code = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
  if (rc != CURLE_OK) r.error = curl_easy_strerror(rc);
  else { r.status = (int)code; r.body = out; }
  printf("sim http: %s %s -> %d\n", method, url.substr(0, url.find('?')).c_str(), r.status);
  curl_slist_free_all(hl);
  curl_easy_cleanup(c);
  return r;
}
