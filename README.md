# TASK-05.-UART-communication

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
- Native compilation, ARM64 cross-compilation and deployment through SCP.
- Optional automatic startup using systemd.

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

`bin/mycom` is a saved static ARM64 executable. The committed files in
`logs/` contain recorded TARGET test results.

## Environment and Prerequisites

The tested TARGET is Raspberry Pi 5 running
64-bit Raspberry Pi OS Lite based on Debian 13.

| Component | Requirement |
| --- | --- |
| HOST build tools | GCC and GNU Make |
| Cross-compiler | `aarch64-linux-gnu-gcc`, including libraries for static linking |
| Deployment | SSH access to the TARGET and `scp` |
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

## Deploy

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

## Automatic Startup with systemd

The supplied unit uses the default deployment path and the
`art207-rpi5` user. If your environment differs, update `User`,
`WorkingDirectory` and `ExecStart` before installation.

Stop any manually running instance before starting the service.

Copy the unit from the HOST repository root:

```bash
scp systemd/uart-echo.service \
    art207-rpi5@ajax-rpi5:~/Projects/TASK-05_UART_communication/
```

On the TARGET, install the system service and enable automatic startup:

```bash
sudo install -m 644 \
    ~/Projects/TASK-05_UART_communication/uart-echo.service \
    /etc/systemd/system/uart-echo.service

sudo systemd-analyze verify /etc/systemd/system/uart-echo.service
sudo systemctl daemon-reload
sudo systemctl enable --now uart-echo.service
```

Inspect the service:

```bash
systemctl is-enabled uart-echo.service
systemctl status uart-echo.service --no-pager
journalctl -u uart-echo.service -b --no-pager
```

Stop or start it explicitly:

```bash
sudo systemctl stop uart-echo.service
sudo systemctl start uart-echo.service
```

The service runs as `art207-rpi5` with the supplementary `dialout` group.
Its working directory keeps application logs in the deployment directory.

Automatic startup after reboot and UART communication under systemd
were verified. See [Report.md](Report.md#6-automatic-startup-using-systemd)
for the recorded results.

## Verification

The report documents:

- UART configuration and regular-user access;
- a physical UART loopback test;
- native and static ARM64 builds;
- deployment and execution on the TARGET;
- case inversion and timestamped message logging;
- manual termination through SIGINT;
- automatic startup after reboot;
- UART communication after reboot;
- SIGTERM handling during service shutdown.

Saved TARGET logs are available in [logs/](logs/).

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
