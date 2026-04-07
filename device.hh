#pragma once

#include <string>
#include <iosfwd>
#include <stdexcept>

namespace molly
{
  enum class DeviceState
  {
    Unknown       = -2,
    Unavailable   = -1,
    LidClosed     = 21,
    ButtonPressed = 22,
    LidOpen       = 23
  };

  std::ostream& operator<<(std::ostream& os, DeviceState const& state);

  class MollyError : public std::exception
  {
  public:
    explicit MollyError(const std::string& message)
      : _message(message)
    {}

    const char* what() const noexcept override
    {
      return _message.c_str();
    }

  private:
    std::string _message;
  };

  class Device
  {
  public:
    Device();
    ~Device();

    // Non-copyable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    void open(const std::string& devicePath);
    void close();
    DeviceState sample();

    bool isOpen() const;

  private:
    int _fd;
  };
}
