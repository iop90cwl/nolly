#pragma once

#include <string>
#include <vector>

namespace molly
{
  // Attempts to find a running desktop session process (plasmashell, gnome-shell, etc.)
  // and read its environment from /proc/<pid>/environ.
  // Returns a list of "KEY=VALUE" strings suitable for use with execve/execvpe.
  // Falls back to an empty list if nothing is found (execvp will use the daemon's own env).
  std::vector<std::string> captureSessionEnv();
}
