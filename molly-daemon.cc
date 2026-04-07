#include "device.hh"
#include "config.hh"
#include "session_env.hh"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <csignal>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace molly;

static std::atomic<bool> g_shutdown{false};

// Session environment captured at startup for passing to child processes
static std::vector<std::string> g_sessionEnv;

static void handleSignal(int signo)
{
  syslog(LOG_INFO, "Received signal %s -- shutting down", strsignal(signo));
  g_shutdown.store(true);
}

// Safe command runner: uses execvpe instead of system() to avoid shell injection.
// Injects the captured desktop session environment so D-Bus/display tools work.
static void runCommand(const std::string& command)
{
  if (command.empty())
    return;

  // Tokenise the command into argv
  std::vector<std::string> args;
  std::istringstream iss(command);
  std::string token;
  while (iss >> token)
    args.push_back(token);

  if (args.empty())
    return;

  pid_t pid = fork();

  if (pid < 0)
  {
    syslog(LOG_ERR, "Error forking to invoke command: %s", command.c_str());
    return;
  }

  if (pid == 0)
  {
    // Build null-terminated argv
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& a : args)
      argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    syslog(LOG_INFO, "Invoking command: %s", command.c_str());

    if (!g_sessionEnv.empty())
    {
      // Merge session env on top of current env.
      // Start with the existing environ, then override with session vars.
      std::vector<std::string> merged;
      extern char** environ;
      for (char** e = environ; e && *e; ++e)
        merged.push_back(*e);

      // Override/add session vars
      for (const auto& kv : g_sessionEnv)
      {
        const auto eq = kv.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = kv.substr(0, eq) + "=";
        // Remove existing entry with same key
        merged.erase(std::remove_if(merged.begin(), merged.end(),
          [&key](const std::string& e){ return e.substr(0, key.size()) == key; }),
          merged.end());
        merged.push_back(kv);
      }

      std::vector<char*> envp;
      envp.reserve(merged.size() + 1);
      for (auto& e : merged)
        envp.push_back(const_cast<char*>(e.c_str()));
      envp.push_back(nullptr);

      execve(argv[0], argv.data(), envp.data());

      // execve with absolute path failed — try PATH search via execvpe
      // by falling through to execvp below
    }

    execvp(argv[0], argv.data());

    syslog(LOG_ERR, "exec failed for command '%s': %s", command.c_str(), strerror(errno));
    _exit(1);
  }
}

static void daemonize()
{
  pid_t pid = fork();

  if (pid < 0)
  {
    syslog(LOG_ERR, "Unable to fork child process");
    std::cerr << "Unable to fork child process\n";
    exit(EXIT_FAILURE);
  }

  if (pid > 0)
  {
    // Parent exits, child continues
    syslog(LOG_INFO, "mollyd started with PID %d", pid);
    std::cout << "mollyd started with PID " << pid << "\n";
    exit(EXIT_SUCCESS);
  }

  // Make this process the session leader
  if (setsid() < 0)
  {
    syslog(LOG_ERR, "Unable to set session ID");
    exit(EXIT_FAILURE);
  }

  // Change working directory to something guaranteed to exist
  if (chdir("/") < 0)
  {
    syslog(LOG_ERR, "Unable to set working directory");
    exit(EXIT_FAILURE);
  }

  umask(0);

  // Redirect standard streams to /dev/null
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  int devNull = open("/dev/null", O_RDWR);
  if (devNull >= 0)
  {
    dup2(devNull, STDIN_FILENO);
    dup2(devNull, STDOUT_FILENO);
    dup2(devNull, STDERR_FILENO);
    if (devNull > STDERR_FILENO)
      close(devNull);
  }
}

int main(int argc, char* argv[])
{
  const std::string configPath = (argc > 1) ? argv[1] : "/etc/mollyd.conf";

  openlog("mollyd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
  setlogmask(LOG_UPTO(LOG_INFO));
  syslog(LOG_INFO, "Starting mollyd");

  Config config;
  config.loadFromFile(configPath);

  // Capture desktop session environment before daemonizing
  g_sessionEnv = captureSessionEnv();

  daemonize();

  // Set up signal handlers after daemonizing
  struct sigaction sa{};
  sa.sa_handler = handleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT,  &sa, nullptr);

  // Reap children automatically to avoid zombies
  struct sigaction sa_chld{};
  sa_chld.sa_handler = SIG_DFL;
  sa_chld.sa_flags = SA_NOCLDWAIT;
  sigaction(SIGCHLD, &sa_chld, nullptr);

  // Ignore SIGHUP (could be used for config reload in the future)
  signal(SIGHUP, SIG_IGN);

  DeviceState lastState = DeviceState::Unknown;
  Device device;

  while (!g_shutdown.load())
  {
    if (!device.isOpen())
    {
      try
      {
        syslog(LOG_INFO, "Opening device: %s", config.devicePath.c_str());
        device.open(config.devicePath);
        syslog(LOG_INFO, "Device opened");
      }
      catch (const MollyError& err)
      {
        syslog(LOG_ERR, "Error opening device: %s", err.what());
        usleep(500 * 1000); // 500ms retry delay
        continue;
      }
    }

    DeviceState state;
    try
    {
      state = device.sample();
    }
    catch (const MollyError& err)
    {
      syslog(LOG_ERR, "Error reading from device: %s", err.what());
      try { device.close(); } catch (...) {}
      continue;
    }

    switch (state)
    {
      case DeviceState::ButtonPressed:
        if (lastState != DeviceState::ButtonPressed)
        {
          syslog(LOG_INFO, "STATE: Pressed");
          runCommand(config.onPress);
        }
        break;

      case DeviceState::LidOpen:
        if (lastState != DeviceState::LidOpen && lastState != DeviceState::ButtonPressed)
        {
          syslog(LOG_INFO, "STATE: Open");
          runCommand(config.onOpen);
        }
        break;

      case DeviceState::LidClosed:
        if (lastState != DeviceState::LidClosed)
        {
          syslog(LOG_INFO, "STATE: Closed");
          runCommand(config.onClose);
        }
        break;

      default:
        break;
    }

    if (state != DeviceState::Unavailable)
      lastState = state;

    usleep(config.pollIntervalMs * 1000);
  }

  syslog(LOG_INFO, "mollyd stopped");
  closelog();
  return EXIT_SUCCESS;
}
