#include "session_env.hh"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>

// Desktop session processes to look for, in priority order.
// The first one found wins.
static const char* SESSION_PROCS[] = {
  "plasmashell",
  "kwin_wayland",
  "kwin_x11",
  "gnome-shell",
  "xfce4-session",
  nullptr
};

// Environment variables we want to capture from the session.
static const char* WANTED_VARS[] = {
  "DBUS_SESSION_BUS_ADDRESS",
  "DISPLAY",
  "WAYLAND_DISPLAY",
  "XDG_RUNTIME_DIR",
  "XDG_SESSION_TYPE",
  "HOME",
  "USER",
  "LOGNAME",
  "LANG",
  "LC_ALL",
  nullptr
};

static std::string readProcName(const std::string& pid)
{
  std::ifstream f("/proc/" + pid + "/comm");
  std::string name;
  std::getline(f, name);
  return name;
}

static std::vector<std::string> readProcEnviron(const std::string& pid)
{
  std::vector<std::string> result;
  std::string path = "/proc/" + pid + "/environ";

  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
    return result;

  // environ entries are null-separated
  std::string content;
  char buf[4096];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0)
    content.append(buf, n);
  close(fd);

  size_t start = 0;
  for (size_t i = 0; i <= content.size(); ++i)
  {
    if (i == content.size() || content[i] == '\0')
    {
      if (i > start)
        result.push_back(content.substr(start, i - start));
      start = i + 1;
    }
  }

  return result;
}

std::vector<std::string> molly::captureSessionEnv()
{
  // Build set of wanted var names for fast lookup
  std::set<std::string> wanted;
  for (int i = 0; WANTED_VARS[i]; ++i)
    wanted.insert(WANTED_VARS[i]);

  // Scan /proc for each candidate process name
  for (int si = 0; SESSION_PROCS[si]; ++si)
  {
    const std::string target = SESSION_PROCS[si];

    DIR* dir = opendir("/proc");
    if (!dir)
      break;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
      // Only numeric entries are PIDs
      const std::string name = entry->d_name;
      if (name.find_first_not_of("0123456789") != std::string::npos)
        continue;

      if (readProcName(name) != target)
        continue;

      auto fullEnv = readProcEnviron(name);
      if (fullEnv.empty())
        continue;

      // Filter to only the vars we care about
      std::vector<std::string> filtered;
      for (const auto& kv : fullEnv)
      {
        const auto eq = kv.find('=');
        if (eq == std::string::npos)
          continue;
        const std::string key = kv.substr(0, eq);
        if (wanted.count(key))
          filtered.push_back(kv);
      }

      if (!filtered.empty())
      {
        closedir(dir);
        syslog(LOG_INFO, "Captured session environment from %s (PID %s)", target.c_str(), name.c_str());
        return filtered;
      }
    }

    closedir(dir);
  }

  syslog(LOG_WARNING, "Could not find a desktop session process to capture environment from");
  return {};
}
