#include "config.hh"

#include <fstream>
#include <sstream>
#include <syslog.h>
#include <algorithm>

// Trim leading/trailing whitespace
static std::string trim(const std::string& s)
{
  const auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool molly::Config::loadFromFile(const std::string& path)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    syslog(LOG_WARNING, "Config file not found at %s, using defaults", path.c_str());
    return false;
  }

  std::string line;
  int lineNum = 0;

  while (std::getline(file, line))
  {
    ++lineNum;

    // Strip comments and blank lines
    const auto commentPos = line.find('#');
    if (commentPos != std::string::npos)
      line = line.substr(0, commentPos);

    line = trim(line);
    if (line.empty())
      continue;

    const auto eq = line.find('=');
    if (eq == std::string::npos)
    {
      syslog(LOG_WARNING, "Config: malformed line %d (no '='): %s", lineNum, line.c_str());
      continue;
    }

    const std::string key   = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));

    if (key == "device")
      devicePath = value;
    else if (key == "on_open")
      onOpen = value;
    else if (key == "on_press")
      onPress = value;
    else if (key == "on_close")
      onClose = value;
    else
      syslog(LOG_WARNING, "Config: unknown key '%s' on line %d", key.c_str(), lineNum);
  }

  syslog(LOG_INFO, "Config loaded from %s", path.c_str());
  return true;
}
