// Minimal Arduino shim so firmware sources compile natively (sim env only).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#define PROGMEM

class String : public std::string {
 public:
  String() = default;
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
  String(int v) : std::string(std::to_string(v)) {}
  String(float v) : std::string(std::to_string(v)) {}
  const char* c_str() const { return std::string::c_str(); }
  int length() const { return (int)size(); }
};

struct SerialShim {
  void begin(unsigned long) {}
  void print(const char* s) { fputs(s, stdout); }
  void println(const char* s = "") { puts(s); }
  void println(int v) { printf("%d\n", v); }
  template <typename... A> void printf(const char* f, A... a) { ::printf(f, a...); }
};
extern SerialShim Serial;

#define INPUT 0
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
void pinMode(int, int);
int digitalRead(int pin);  // sim: keys 1/2/3 = KEY0/1/2 (pins 3/4/5), or env SIM_PRESS=<pin>

unsigned long millis();
void delay(unsigned long ms);

void setup();
void loop();
