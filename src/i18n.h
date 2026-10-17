#pragma once
#include <Arduino.h>
#include <time.h>

// Languages, chosen in the settings page ("lang"): en (default), fr, de, it, es.
enum Lang { LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_ES };

// Reads the "lang" setting. Call after it may have changed (start-up, saving the settings).
void i18nReload();
const char* i18nCode();  // "en", "fr", ...

// Translates an English UI string. The English text is the key, so an untranslated string simply
// shows in English. Strings with printf-style placeholders (%d, %s) keep them in every language.
// Translations only use ASCII and Latin-1 characters: that is what the panel fonts contain.
const char* T(const char* en);

// "17 Oct", "17 oct", "17. Okt", ...
String i18nDate(const struct tm& t);
