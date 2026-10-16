// Preferences shim (sim only): persisted to ./.sim_prefs as "ns/key<TAB>value" lines.
#pragma once
#include <Arduino.h>
#include <fstream>
#include <map>

class Preferences {
 public:
  bool begin(const char* ns, bool = false) { ns_ = ns; load(); return true; }
  void end() {}
  String getString(const char* k, const String& def = String()) {
    auto it = db().find(ns_ + "/" + k);
    return it == db().end() ? def : String(it->second);
  }
  size_t putString(const char* k, const String& v) {
    db()[ns_ + "/" + k] = v;
    save();
    return v.length();
  }
 private:
  std::string ns_;
  static std::map<std::string, std::string>& db() { static std::map<std::string, std::string> m; return m; }
  static void load() {
    static bool done = false;
    if (done) return;
    done = true;
    std::ifstream f(".sim_prefs");
    std::string line;
    while (std::getline(f, line)) {
      size_t t = line.find('\t');
      if (t != std::string::npos) db()[line.substr(0, t)] = line.substr(t + 1);
    }
  }
  static void save() {
    std::ofstream f(".sim_prefs");
    for (auto& kv : db()) f << kv.first << '\t' << kv.second << '\n';
  }
};
