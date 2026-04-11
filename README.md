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

Two options depending on your preference. The user service is recommended — it runs in your desktop session so `notify-send`, `kdialog`, and other session tools work without any extra configuration.

### Option A: User service (recommended)

Runs as your user, starts with your desktop session. No root required after install.

**1. Install the binary**

    $ sudo cp build/mollyd /usr/local/bin/mollyd

**2. Set up the config**

    $ mkdir -p ~/.config
    $ cp mollyd.conf.example ~/.config/mollyd.conf
    $ nano ~/.config/mollyd.conf

**3. Install the user service**

    $ mkdir -p ~/.config/systemd/user
    $ cp contrib/mollyd.service ~/.config/systemd/user/mollyd.service
    $ systemctl --user daemon-reload

**4. Enable and start**

    $ systemctl --user enable mollyd
    $ systemctl --user start mollyd

**5. Check it's running**

    $ systemctl --user status mollyd
    $ journalctl --user -t mollyd -f

**Stopping / restarting**

    $ systemctl --user stop mollyd
    $ systemctl --user restart mollyd

---

### Option B: System service

Runs at boot as a specific user. Use this if you want the daemon running before login or need system-wide install.

**1. Install the binary and config**

    $ sudo cp build/mollyd /usr/local/bin/mollyd
    $ sudo cp mollyd.conf.example /etc/mollyd.conf
    $ sudo nano /etc/mollyd.conf

**2. Edit the service file** — set `User=` to your username

    $ nano contrib/mollyd-system.service

**3. Install and enable**

    $ sudo cp contrib/mollyd-system.service /etc/systemd/system/mollyd.service
    $ sudo systemctl daemon-reload
    $ sudo systemctl enable mollyd
    $ sudo systemctl start mollyd

**4. Check logs**

    $ journalctl -t mollyd -f

> Note: with the system service, desktop session commands like `notify-send` require the daemon to be running after your session starts. The daemon retries session environment capture on each command invocation, so it will pick up your session automatically once you log in.

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
