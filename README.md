# TASK-05-06 UART-communication and Systemd

The project covers two tasks:

- **TASK-05 - UART communication:** serial interface configuration,
  CRLF-delimited message processing, ASCII case inversion, logging
  and signal handling.
- **TASK-06 - systemd integration:** automatic startup, restart policy,
  service management and installation through Makefile.

C UART ECHO application for Raspberry Pi 5 running Raspberry Pi OS Lite.

The application receives CRLF-terminated messages, records their original
contents with timestamps and returns each message with the case of ASCII
letters inverted.

```text
HOST sends:     Hello uart test\r\n
TARGET replies: hELLO UART TEST\r\n
```

The implementation uses `termios`, non-blocking I/O, and `poll()`.
It supports manual execution and automatic startup through systemd.

Detailed setup commands, photos and recorded results are available
in [Report.md](Report.md).

## Features

- Serial device selected through a command-line argument.
- Fixed communication settings: 115200 baud, 8 data bits, no parity,
  and 1 stop bit (8N1).
- Hardware and software flow control disabled.
- Message completion detected using CRLF (`\r\n`).
- ASCII letter case inversion; other bytes remain unchanged.
- Timestamped logging of original received messages.
- A separate log file for each execution.
- Error reporting to standard error and the log when available.
- SIGINT and SIGTERM handling with resource cleanup.
- Native compilation, ARM64 cross-compilation.
- Remote deployment and system installation through Makefile.
- Automatic startup and process management through systemd.

## Project Structure

```text
TASK-05.-UART-communication/
├── .clang-format
├── Makefile
├── README.md
├── Report.md
├── bin/
│   └── mycom
├── images/
│   ├── uart-loopback.jpg
│   ├── uart-adapter-pinout.jpg
│   └── uart-adapter-connection.jpg
├── logs/
│   └── *.log
├── src/
│   └── mycom.c
└── systemd/
    └── uart-echo.service
```

The Makefile builds `mycom` in the repository root.
It doesn't update the saved executable in `bin/`.

`bin/mycom` is a saved static ARM64 executable. The committed files in
`logs/` contain recorded TARGET test results.

## Environment and Prerequisites

The tested TARGET is Raspberry Pi 5 running
64-bit Raspberry Pi OS Lite based on Debian 13.

| Component | Requirement |
| --- | --- |
| HOST build tools | GCC and GNU Make |
| Cross-compiler | `aarch64-linux-gnu-gcc`, including libraries for static linking |
| Deployment and installation | SSH and SCP access to the TARGET |
| TARGET permissions | Permission to use `sudo` for system installation |
| Serial terminal | `picocom` |
| Formatting | `clang-format` compatible with the supplied configuration |
| Hardware | USB-to-UART adapter with 3.3 V logic levels |

The TARGET must have UART0 enabled on GPIO14 and GPIO15.
The application user must have read and write access to `/dev/ttyAMA0`.

The verified configuration, permissions and loopback procedure are
documented in [Report.md](Report.md).

## Hardware Connection

| USB-to-UART adapter | Raspberry Pi 5 |
| --- | --- |
| TX | GPIO15 / RXD0, physical pin 10 |
| RX | GPIO14 / TXD0, physical pin 8 |
| GND | GND, for example physical pin 6 |

Use 3.3 V UART logic levels. Power the Raspberry Pi through its normal
power supply; the adapter power pin isn't needed for this connection.

The verified device paths are:

- HOST adapter: `/dev/ttyUSB0`;
- TARGET UART0: `/dev/ttyAMA0`.

Photographs are available in the [hardware connection section](Report.md#51-hardware-connection).

## Build

Run the following commands from the repository root.

Native build:

```bash
make clean
make
```

Static ARM64 build for the TARGET:

```bash
make clean
make CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=-static
file mycom
```

The expected ARM64 output is an ELF 64-bit AArch64 executable identified
as statically linked.

Run `make clean` before switching architectures or changing build flags.
The Makefile doesn't automatically detect changes to command-line
variable values.

Default compiler flags:

```text
-std=c11 -O2 -Wall -Wextra -Wpedantic
```

Additional options can be supplied through `CPPFLAGS`, `CFLAGS`,
`LDFLAGS` and `LDLIBS`.

## Makefile Targets

| Target | Purpose |
| --- | --- |
| `all` | Build `mycom` in the repository root |
| `clean` | Remove the root-level `mycom` executable |
| `deploy` | Copy the executable into the TARGET project directory |
| `install` | Install the executable and service into system locations and enable automatic startup |

`all` is the default target.

`clean` preserves the saved binary, logs and photos.
Neither `deploy` or `install` starts or restarts the application.

## Manual Deployment and Execution - TASK-05

The default deployment destination is:

```text
art207-rpi5@ajax-rpi5:/home/art207-rpi5/Projects/TASK-05_UART_communication
```

If the application is already running on the TARGET, stop it before
replacing its executable. For running systemd service, use:

```bash
sudo systemctl stop uart-echo.service
```

On the HOST:

```bash
make clean
make deploy CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=-static
```

The `deploy` target builds the executable, creates the remote deployment
and log directories and copies `mycom` through SCP.

The destination can be overridden through `REMOTE` and `REMOTE_DIR`:

```bash
make deploy CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=-static \
    REMOTE=art207-rpi5@ajax-rpi5 \
    REMOTE_DIR=/home/art207-rpi5/Projects/TASK-05_UART_communication
```

Deployment doesn't start the application or install the systemd service.

## Manual Execution

On the TARGET, ensure that the systemd service and other programs aren't
using the same UART device.

Start the application from the deployment directory:

```bash
cd ~/Projects/TASK-05_UART_communication
./mycom /dev/ttyAMA0
```

The application prints the selected device, communication settings
and log filename.

On the HOST, open the USB-to-UART adapter:

```bash
picocom --baud 115200 --databits 8 --parity n \
    --stopbits 1 --flow n --omap crcrlf /dev/ttyUSB0
```

Keep local echo disabled. The `--omap crcrlf` option converts Enter into
the CRLF sequence required to complete message.

Type `Hello uart test` and press Enter. The expected response is:

```text
hELLO UART TEST
```

To stop the manually started application, press Ctrl+C in the TARGET
terminal where `mycom` is running.

## Logging

Logs are created in `logs/` relative to the application's working
directory. The user running the application needs permission to create
and write files there.

Filename format:

```text
YYYYMMDD-HHMMSS-PID.log
```

The filename contains the startup timestamp and process identifier (PID).

Example from the recorded TARGET test:

```text
2026-10-16 00:06:46+0300 START /dev/ttyAMA0
2026-10-16 00:07:13+0300 RX Hello uart test
2026-10-16 00:10:03+0300 STOP SIGINT
```

Entry types are `START`, `RX`, `ERROR`, and `STOP`.
`RX` entry preserves the message before case conversion and excludes
the terminating CRLF.

Non-printable bytes and backslashes are escaped as `\xNN` to keep each
message on one log line.

Successful signal-driven termination returns exit status `0` when
logging and cleanup succeed. Errors return exit status `1`.

## System Installation - TASK-06

The installation target runs on the HOST and performs privileged
installation commands on the TARGET through SSH.

Default remote settings:

```makefile
REMOTE ?= art207-rpi5@ajax-rpi5
REMOTE_DIR ?= /home/art207-rpi5/Projects/TASK-05_UART_communication
```

Installed paths:

| Item | TARGET path |
| --- | --- |
| Executable | `/usr/local/bin/mycom` |
| Service unit | `/etc/systemd/system/uart-echo.service` |
| Working directory | `/var/lib/uart-echo` |
| Application logs | `/var/lib/uart-echo/logs/` |

Build and install from the HOST:

```bash
make clean
make install CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=-static
```

The target:

1. Builds the executable if required.
2. Copies the executable and unit file into `$(REMOTE_DIR)/install/`.
3. Installs the executable with mode `755`.
4. Installs the unit file with mode `644`.
5. Runs `systemctl daemon-reload`.
6. Runs `systemctl enable uart-echo.service`.

The SSH session allocates a terminal so that remote `sudo` can request
a password. Running `sudo make install` on the HOST is not required.

To inspect the commands without executing them:

```bash
make -n install CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=-static
```

The destination can be overridden:

```bash
make install CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=-static \
    REMOTE=art207-rpi5@ajax-rpi5 \
    REMOTE_DIR=/home/art207-rpi5/Projects/TASK-05_UART_communication
```

`REMOTE_DIR` controls the staging location.
It doesn't change the final installation paths or the service user.

If a different TARGET account is used, update `User=` in
[systemd/uart-echo.service](systemd/uart-echo.service) before installation.

## Service Management

Before starting the service, stop any manually running instance
of `mycom` using Ctrl+C in its TARGET terminal.
Only one application instance should use the UART device.

After installation, run on the TARGET:

```bash
sudo systemd-analyze verify /etc/systemd/system/uart-echo.service
sudo systemctl restart uart-echo.service
```

Stop any manually running instance of `mycom` before starting the service.
Only one application instance should use the UART device.

Inspect the service:

```bash
systemctl is-enabled uart-echo.service
systemctl status uart-echo.service --no-pager
journalctl -u uart-echo.service -b --no-pager
```

The expected startup message includes:

```text
Ready: /dev/ttyAMA0, 115200 8N1; log: logs/...
```

The relative log path is resolved against `/var/lib/uart-echo`.

Service commands:

```bash
sudo systemctl stop uart-echo.service
sudo systemctl start uart-echo.service
sudo systemctl restart uart-echo.service
```

The service configuration provides:

| Setting | Behavior |
| --- | --- |
| `Type=simple` | systemd manages the foreground application process |
| `After=sysinit.target` | Startup is ordered after basic system initialization |
| `User=art207-rpi5` | The application runs as the regular TARGET user |
| `SupplementaryGroups=dialout` | Provides group-based UART access |
| `StateDirectory=uart-echo` | systemd creates the persistent working directory |
| `Restart=on-failure` | Requests automatic recovery after a failure |
| `RestartSec=3` | Sets a three-second delay before automatic restart |
| `KillSignal=SIGTERM` | Requests orderly shutdown |
| `TimeoutStopSec=5` | Limits the time allowed for shutdown |

Automatic restart remains subject to systemd's start-rate limits.
An explicit `systemctl stop` doesn't trigger automatic recovery.

`make install` enables startup at boot. To check it, reboot the TARGET,
reconnect through SSH and inspect the service:

```bash
sudo reboot
```

After reconnecting:

```bash
systemctl is-active uart-echo.service
journalctl -u uart-echo.service -b --no-pager
```

## Verification and Documentation

[Report.md](Report.md) contains the implementation history,
test results and conclusions for both tasks:

- Sections 1–7: TASK-05 preparation, UART loopback, application
  development, deployment and hardware tests.
- Section 8: updated systemd service configuration.
- Section 9: Makefile installation target.
- Section 10: TASK-06 conclusion.

Check source formatting without modifying files:

```bash
clang-format --style=file --dry-run --Werror src/mycom.c
```

Check C syntax and compiler diagnostics:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsyntax-only src/mycom.c
```

## Limitations

- Maximum message content length: 1024 bytes, excluding CRLF.
- A lone CR or LF doesn't complete a message.
- Overlong messages cause an error and terminate the application.
- Case conversion supports ASCII letters only.
- An incomplete message is discarded during termination.
- Disconnecting TX/RX wires doesn't necessarily produce a device error;
  no separate link-monitoring protocol is implemented.
