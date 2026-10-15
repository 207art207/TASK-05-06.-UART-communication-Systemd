# Report: UART ECHO Application

## 1. Implemented Application Functionality (Commit: Task-05: add UART echo application with logging and signal handling; and add Report.md)

### 1.1. Port Selection and Configuration

The device path is passed as a command-line argument:

```bash
./mycom /dev/ttyAMA0
```

If the number of arguments is incorrect, the application displays a usage message and exits with code `1`.

The port is opened for reading and writing using the following flags:

- `O_RDWR` — enables bidirectional communication;
- `O_NOCTTY` — prevents the port from becoming the controlling terminal of the process;
- `O_NONBLOCK` — enables non-blocking operations on the device.

The original port settings are saved using `tcgetattr()`.

The following parameters are configured using `termios`:

| Parameter | Value |
|---|---|
| Receive and transmit baud rate | 115200 bps |
| Data bits | 8 |
| Parity | Disabled |
| Stop bits | 1 |
| Hardware flow control | Disabled |
| Software flow control | Disabled |
| Data processing mode | Raw mode |

Raw mode disables terminal echo and automatic character processing. This allows the application to determine message boundaries independently.

### 1.2. Message Reception and Response Generation

The application uses `poll()` to check whether the port is ready for reading. Received bytes are accumulated sequentially in a buffer.

A message is considered complete only after receiving the following two bytes:

- `\r` — CR, Carriage Return;
- `\n` — LF, Line Feed.

An individual CR or LF does not terminate a message.

After receiving a complete message, the application:

1. Writes the original message content to the log file.
2. Converts `a–z` to `A–Z` and `A–Z` to `a–z`.
3. Sends the converted message through UART, followed by CRLF.
4. Resets the buffer length counter to prepare for the next message.

Example:

```text
Received: Hello UART 123!\r\n
Sent:     hELLO uart 123!\r\n
```

The `write_all()` function is used for transmission. It handles partial writes, since a single `write()` call may write only part of the data.

### 1.3. Logging

The log file is opened before the UART device. This makes it possible to record an error if the device does not exist or the user lacks the required access permissions.

The log filename follows this format:

```text
logs/YYYYMMDD-HHMMSS-PID.log
```

The timestamp represents the application start time, while PID is the process identifier.

The `"wx"` file opening mode prevents an existing file from being overwritten.

Each log entry begins with the local date, time, and time zone offset.

The following log entry types are supported:

| Type | Description |
|---|---|
| `START` | Successful port opening and configuration |
| `RX` | Complete message received |
| `ERROR` | Runtime error |
| `STOP` | Termination caused by SIGINT or SIGTERM |

An `RX` entry contains the original message content before case conversion, excluding the terminating CRLF.

Control bytes, bytes outside the printable ASCII range, and backslash characters are represented using the `\xNN` notation. This ensures that each received message occupies a single line in the log file.

After each log entry, `fflush()` is called.

If the log file cannot be created or written to, the corresponding error message is printed to the standard error stream (`stderr`).

### 1.4. Error and Signal Handling

The application checks the results of device opening, `termios` configuration, event polling, reading, writing, and logging operations.

Errors are reported through `stderr` and recorded in the log file whenever logging is available.

If an error occurs, the application exits with code `1`.

Handlers for `SIGINT` and `SIGTERM` are installed using `sigaction()`.

The signal handler only stores the received signal number in a variable of type `volatile sig_atomic_t`.

The main application logic detects the termination request, records the reason for stopping, restores the original port settings, and closes all opened resources.

When the application terminates successfully in response to a signal, it returns exit code `0`.

### 1.5. Limitations of the Current Implementation

- The maximum message content length is 1024 bytes, excluding CRLF.
- The buffer size is 1026 bytes, including the terminating sequence.
- Exceeding the maximum allowed message length causes an error and terminates the application.
- Case conversion is supported only for English ASCII letters.
- All other bytes are transmitted unchanged; Unicode case conversion is not supported.
- An incomplete message is discarded when the application terminates.
- The reception timestamp is recorded when the application recognizes a complete message, rather than at the hardware level when the first bit arrives.
- Disconnecting the TX/RX wires does not necessarily cause a device error; no separate communication link monitoring protocol is implemented.

### 1.6. Compilation Verification

Syntax checking and compiler diagnostics were performed using the following command:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsyntax-only src/mycom.c
```

**Result:** Exit code `0`, with no compiler diagnostic messages.

A complete native build was also performed in a temporary directory using the following compiler flags:

```text
-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
```

Compilation and linking completed successfully without warnings.