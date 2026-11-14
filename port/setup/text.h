#ifndef SETUP_TEXT_H
#define SETUP_TEXT_H
#include <string>

// Load once on the main thread, before any installer workers start.
void SetupText_Load(const std::string &base);
const char *SetupText(const char *key);
#endif
