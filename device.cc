#include "device.hh"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>

using namespace molly;

#define INVALID_FD (-1)

Device::Device()
  : _fd(INVALID_FD)
{}

Device::~Device()
{
  if (_fd != INVALID_FD)
    ::close(_fd);
}

void Device::open(const std::string& devicePath)
{
  if (_fd != INVALID_FD)
    throw MollyError("Device already open");

  struct stat buffer;
  if (stat(devicePath.c_str(), &buffer) == -1)
  {
    std::ostringstream msg;
    msg << "Device does not exist: " << strerror(errno);
    throw MollyError(msg.str());
  }

  int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);

  if (fd < 0)
  {
    std::ostringstream msg;
    msg << "Error opening device: " << strerror(errno);
    throw MollyError(msg.str());
  }

  _fd = fd;
}

void Device::close()
{
  if (_fd == INVALID_FD)
    throw MollyError("Device not open");

  int res = ::close(_fd);
  _fd = INVALID_FD;

  if (res != 0)
    throw MollyError("Error closing device");
}

DeviceState Device::sample()
{
  if (_fd == INVALID_FD)
    throw MollyError("Device not yet open");

  // Write the HID report to request state
  const unsigned char buf[8] = { 0x08, 0, 0, 0, 0, 0, 0, 0x02 };

  ssize_t res = ::write(_fd, buf, sizeof(buf));

  if (res < 0)
  {
    std::ostringstream msg;
    msg << "Error writing to device: " << strerror(errno);
    throw MollyError(msg.str());
  }

  unsigned char code = 0;
  res = ::read(_fd, &code, 1);

  if (res != 1)
  {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return DeviceState::Unavailable;

    std::ostringstream msg;
    msg << "Error reading from device: " << strerror(errno);
    throw MollyError(msg.str());
  }

  switch (code)
  {
    case static_cast<unsigned char>(DeviceState::ButtonPressed):
      return DeviceState::ButtonPressed;
    case static_cast<unsigned char>(DeviceState::LidClosed):
      return DeviceState::LidClosed;
    case static_cast<unsigned char>(DeviceState::LidOpen):
      return DeviceState::LidOpen;
    default:
    {
      std::ostringstream msg;
      msg << "Unexpected response code: " << static_cast<int>(code);
      throw MollyError(msg.str());
    }
  }
}

bool Device::isOpen() const
{
  if (_fd == INVALID_FD)
    return false;
  // fcntl returns -1 with EBADF if fd is invalid/closed
  return fcntl(_fd, F_GETFD) != -1;
}

std::ostream& molly::operator<<(std::ostream& os, DeviceState const& state)
{
  switch (state)
  {
    case DeviceState::Unknown:       os << "Unknown";        break;
    case DeviceState::Unavailable:   os << "Unavailable";    break;
    case DeviceState::LidClosed:     os << "LidClosed";      break;
    case DeviceState::ButtonPressed: os << "ButtonPressed";  break;
    case DeviceState::LidOpen:       os << "LidOpen";        break;
  }
  return os;
}
