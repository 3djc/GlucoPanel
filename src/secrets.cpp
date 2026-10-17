#include "secrets.h"
#include <string.h>
#ifdef ARDUINO
#include <esp_random.h>
#else
#include <random>
#endif

static uint32_t randomWord() {
#ifdef ARDUINO
  return esp_random();
#else
  static std::random_device rd;
  return rd();
#endif
}

String randomToken(int len, const char* alphabet) {
  String s;
  size_t n = strlen(alphabet);
  for (int i = 0; i < len; i++) s += alphabet[randomWord() % n];  // the tiny modulo bias is irrelevant here
  return s;
}
