# nolly

Modernized Linux tools for the "Big Red Button" USB device manufactured by Dream Cheeky.

Forked from [drewnoakes/molly](https://github.com/drewnoakes/molly).

![Big Red Button](big-red-button.png)

Run arbitrary commands in response to open, close, and button press events.

## Build

Requires CMake 3.14+ and a C++17 compiler.

    cmake -B build
    cmake --build build

## Configure udev

Set up a `udev` rule to mount the USB device under a known path with read/write permissions.

    $ sudo nano /etc/udev/rules.d/99-big-red-button.rules

Add:

    ACTION=="add", ENV{ID_MODEL}=="DL100B_Dream_Cheeky_Generic_Controller", SYMLINK+="big_red_button", MODE="0666"

Reload the rules:

    $ sudo udevadm control --reload-rules

Unplug and replug the device. `/dev/big_red_button` should now exist.

## Test

    $ ./build/molly-test
    Monitoring /dev/big_red_button (Ctrl+C to quit)
    Closed from Unknown
    OPEN...
    PRESS!!!
    Closed from LidOpen

You can also pass a custom device path:

    $ ./build/molly-test /dev/my_device

## Daemon

`mollyd` is a background daemon that monitors the button and runs configured commands on state changes.

    $ ./build/mollyd
    $ ./build/mollyd /path/to/mollyd.conf   # custom config path

Logs go to syslog (`journalctl -t mollyd` on systemd systems).

Stop it with:

    $ kill $(pgrep mollyd)

## Installing as a systemd service

This is the recommended way to run `mollyd` so it starts automatically on boot.

**1. Install the binary**

    $ sudo cp build/mollyd /usr/local/bin/mollyd

**2. Set up the config**

    $ sudo cp mollyd.conf.example /etc/mollyd.conf
    $ sudo nano /etc/mollyd.conf

**3. Install the service unit**

    $ sudo cp contrib/mollyd.service /etc/systemd/system/mollyd.service
    $ sudo systemctl daemon-reload

**4. Enable and start**

    $ sudo systemctl enable mollyd   # start on boot
    $ sudo systemctl start mollyd

**5. Check it's running**

    $ sudo systemctl status mollyd
    $ journalctl -t mollyd -f        # follow logs

**Stopping / restarting**

    $ sudo systemctl stop mollyd
    $ sudo systemctl restart mollyd

> Note: `mollyd` captures the desktop session environment at startup to support tools like `notify-send`. If you log out and back in, restart the daemon so it picks up the new session.

## mollyd.conf

Copy `mollyd.conf.example` to `/etc/mollyd.conf` and edit to taste. All keys are optional.

| Key | Default | Description |
|-----|---------|-------------|
| `device` | `/dev/big_red_button` | Path to the USB HID device |
| `on_open` | _(none)_ | Command to run when the lid is opened |
| `on_press` | _(none)_ | Command to run when the button is pressed |
| `on_close` | _(none)_ | Command to run when the lid is closed |
| `poll_interval_ms` | `20` | Device polling interval in milliseconds |

Example:

    device = /dev/big_red_button
    on_open  = notify-send "Big Red Button" "Lid opened"
    on_press = notify-send "Big Red Button" "Button pressed!"
    on_close = notify-send "Big Red Button" "Lid closed"
    poll_interval_ms = 20

Commands are executed directly without a shell, so pipes and redirects won't work inline — use a script for anything complex.

### Desktop session commands (notify-send, kdialog, etc.)

The daemon automatically captures the desktop session environment at startup (D-Bus address, display, XDG runtime dir, etc.) from a running session process like `plasmashell` or `gnome-shell`. This means tools like `notify-send` work directly in the config without any wrapper scripts.

### Polling interval

The default 20ms interval gives responsive feel with negligible CPU usage. You can raise it (e.g. `poll_interval_ms = 100`) if you want to reduce CPU use further, or lower it for faster response. Values below ~10ms are unlikely to be useful given USB HID report rates.

## Security

Commands are run via `execvp` rather than `system()`, which avoids shell injection vulnerabilities. Each command is forked as a child process and the daemon continues polling while it runs.
