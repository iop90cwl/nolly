#include "device.hh"

#include <iostream>
#include <unistd.h>
#include <csignal>
#include <atomic>

using namespace molly;

static std::atomic<bool> g_running{true};

static void handleSignal(int)
{
  g_running.store(false);
}

int main(int argc, char* argv[])
{
  const std::string devicePath = (argc > 1) ? argv[1] : "/dev/big_red_button";

  signal(SIGINT,  handleSignal);
  signal(SIGTERM, handleSignal);

  Device device;

  try
  {
    device.open(devicePath);
  }
  catch (const MollyError& err)
  {
    std::cerr << "Failed to open device: " << err.what() << "\n";
    return 1;
  }

  std::cout << "Monitoring " << devicePath << " (Ctrl+C to quit)\n";

  DeviceState lastState = DeviceState::Unknown;

  while (g_running.load())
  {
    DeviceState state;

    try
    {
      state = device.sample();
    }
    catch (const MollyError& err)
    {
      std::cerr << "Error: " << err.what() << "\n";
      return 1;
    }

    switch (state)
    {
      case DeviceState::ButtonPressed:
        if (lastState != DeviceState::ButtonPressed)
          std::cout << "PRESS!!!\n";
        break;
      case DeviceState::LidOpen:
        if (lastState != DeviceState::LidOpen && lastState != DeviceState::ButtonPressed)
          std::cout << "OPEN...\n";
        break;
      case DeviceState::LidClosed:
        if (lastState != DeviceState::LidClosed)
          std::cout << "Closed from " << lastState << "\n";
        break;
      default:
        break;
    }

    if (state != DeviceState::Unavailable)
      lastState = state;

    usleep(20 * 1000);
  }

  std::cout << "\nExiting.\n";
  return 0;
}
