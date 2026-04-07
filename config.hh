#pragma once

#include <string>

namespace molly
{
  struct Config
  {
    std::string devicePath   = "/dev/big_red_button";
    std::string onOpen       = "";
    std::string onPress      = "";
    std::string onClose      = "";
    int         pollIntervalMs = 20;

    // Load from file; missing keys keep their defaults.
    // Returns false and logs a warning if the file cannot be opened.
    bool loadFromFile(const std::string& path);
  };
}
