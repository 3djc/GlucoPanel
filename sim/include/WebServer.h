// Tiny single-threaded HTTP server shim for the simulator (subset of the ESP32 WebServer API).
// Listens on 127.0.0.1:8080 (ports below 1024 are remapped).
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <functional>
#include <map>
#include <vector>

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };

class WebServer {
 public:
  typedef std::function<void()> THandlerFunction;
  WebServer(int port = 80) : port_(port < 1024 ? 8080 : port) {}

  void on(const char* uri, HTTPMethod m, THandlerFunction h) { routes_.push_back({uri, m, h}); }
  void on(const char* uri, THandlerFunction h) { on(uri, HTTP_ANY, h); }

  void begin() {
    signal(SIGPIPE, SIG_IGN);
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port_);
    if (bind(fd_, (sockaddr*)&a, sizeof(a)) || listen(fd_, 8)) {
      perror("WebServer bind/listen");
      return;
    }
    fcntl(fd_, F_SETFL, O_NONBLOCK);
    printf("WebServer: http://localhost:%d/\n", port_);
  }

  void handleClient() {
    sockaddr_in peer{};
    socklen_t peerLen = sizeof(peer);
    int c = accept(fd_, (sockaddr*)&peer, &peerLen);
    if (c < 0) return;
    uint32_t pa = ntohl(peer.sin_addr.s_addr);
    for (int i = 0; i < 4; i++) remote_.o[i] = pa >> (24 - 8 * i);
    fcntl(c, F_SETFL, fcntl(c, F_GETFL) & ~O_NONBLOCK);  // accepted sockets inherit O_NONBLOCK on macOS
    timeval tv{0, 300000};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string req;
    char buf[4096];
    size_t he;
    while ((he = req.find("\r\n\r\n")) == std::string::npos) {
      ssize_t n = recv(c, buf, sizeof(buf), 0);
      if (n <= 0) { close(c); return; }
      req.append(buf, n);
    }
    std::string head = req.substr(0, he), lower = head;
    reqHeaders_.clear();
    {
      size_t pos = head.find("\r\n");
      while (pos != std::string::npos && pos < head.size()) {
        size_t eol = head.find("\r\n", pos + 2);
        std::string line = head.substr(pos + 2, eol == std::string::npos ? std::string::npos : eol - pos - 2);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
          std::string v = line.substr(colon + 1);
          v.erase(0, v.find_first_not_of(" \t"));
          std::string k = line.substr(0, colon);
          for (auto& ch : k) ch = tolower(ch);
          reqHeaders_[k] = v;
        }
        pos = eol;
      }
    }
    for (auto& ch : lower) ch = tolower(ch);
    size_t cl = lower.find("content-length:");
    size_t len = cl == std::string::npos ? 0 : (size_t)atoi(head.c_str() + cl + 15);
    while (req.size() < he + 4 + len) {
      ssize_t n = recv(c, buf, sizeof(buf), 0);
      if (n <= 0) break;
      req.append(buf, n);
    }
    std::string body = req.substr(he + 4);

    char m[16] = {}, target[2048] = {};
    sscanf(head.c_str(), "%15s %2047s", m, target);
    HTTPMethod method = !strcmp(m, "POST") ? HTTP_POST : HTTP_GET;
    std::string path = target, query;
    size_t q = path.find('?');
    if (q != std::string::npos) { query = path.substr(q + 1); path = path.substr(0, q); }
    args_.clear();
    parseForm(query);
    if (method == HTTP_POST) parseForm(body);

    cur_ = c;
    hdrs_.clear();
    bool handled = false;
    for (auto& r : routes_)
      if (r.uri == path && (r.method == HTTP_ANY || r.method == method)) { r.fn(); handled = true; break; }
    if (!handled) send(404, "text/plain", "Not found");
    close(c);
  }

  struct Client {
    IPAddress ip;
    IPAddress remoteIP() const { return ip; }
  };
  Client client() { return {remote_}; }

  String arg(const char* name) {
    auto it = args_.find(name);
    return it == args_.end() ? String() : String(it->second);
  }
  bool hasArg(const char* name) { return args_.count(name) > 0; }
  // Like the ESP32 WebServer, request headers are only kept when asked for with collectHeaders().
  void collectHeaders(const char* keys[], size_t n) {
    wanted_.clear();
    for (size_t i = 0; i < n; i++) {
      std::string k = keys[i];
      for (auto& ch : k) ch = tolower(ch);
      wanted_.push_back(k);
    }
  }
  String header(const char* name) {
    std::string k = name;
    for (auto& ch : k) ch = tolower(ch);
    for (auto& w : wanted_)
      if (w == k) {
        auto it = reqHeaders_.find(k);
        return it == reqHeaders_.end() ? String() : String(it->second);
      }
    return String();
  }
  void sendHeader(const char* n, const String& v) { hdrs_ += std::string(n) + ": " + v + "\r\n"; }
  void send(int code, const char* type, const String& body) {
    const char* text = code == 200 ? "OK" : code == 303 ? "See Other" : code == 400 ? "Bad Request" : code == 401 ? "Unauthorized" : code == 403 ? "Forbidden" : code == 429 ? "Too Many Requests" : "Not Found";
    std::string r = "HTTP/1.1 " + std::to_string(code) + " " + text + "\r\nContent-Type: " + type +
                    "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n" +
                    hdrs_ + "\r\n" + body;
    for (size_t off = 0; off < r.size();) {
      ssize_t n = write(cur_, r.data() + off, r.size() - off);
      if (n <= 0) break;
      off += n;
    }
  }

 private:
  struct Route { std::string uri; HTTPMethod method; THandlerFunction fn; };
  std::vector<Route> routes_;
  std::map<std::string, std::string> args_, reqHeaders_;
  std::vector<std::string> wanted_;
  std::string hdrs_;
  IPAddress remote_;
  int port_, fd_ = -1, cur_ = -1;

  static std::string urldecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '+') o += ' ';
      else if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
      else o += s[i];
    }
    return o;
  }
  void parseForm(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
      size_t amp = s.find('&', i);
      if (amp == std::string::npos) amp = s.size();
      std::string kv = s.substr(i, amp - i);
      size_t eq = kv.find('=');
      args_[urldecode(kv.substr(0, eq))] = eq == std::string::npos ? "" : urldecode(kv.substr(eq + 1));
      i = amp + 1;
    }
  }
};
