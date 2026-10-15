#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGE 1024

static volatile sig_atomic_t stop_signal = 0;

static void handle_signal(int signo)
{
	stop_signal = signo;
}

static int timestamp(char *buffer, size_t size, const char *format)
{
	time_t now = time(NULL);
	struct tm local;

	if (now == (time_t)-1 || localtime_r(&now, &local) == NULL ||
	    strftime(buffer, size, format, &local) == 0) {
		errno = EOVERFLOW;
		return -1;
	}
	return 0;
}

static int log_record(FILE *log, const char *type, const unsigned char *data,
		      size_t length)
{
	char stamp[64];

	if (timestamp(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S%z") < 0)
		return -1;
	if (fprintf(log, "%s %s ", stamp, type) < 0)
		return -1;

	for (size_t i = 0; i < length; ++i) {
		if (data[i] >= 32 && data[i] <= 126 && data[i] != '\\')
			fputc(data[i], log);
		else
			fprintf(log, "\\x%02X", (unsigned int)data[i]);
	}

	if (fputc('\n', log) == EOF || fflush(log) == EOF || ferror(log))
		return -1;
	return 0;
}

static void report_error(FILE *log, const char *operation)
{
	int saved_errno = errno;
	char message[256];

	snprintf(message, sizeof(message), "%s: %s", operation,
		 strerror(saved_errno));
	fprintf(stderr, "%s\n", message);
	if (log != NULL &&
	    log_record(log, "ERROR", (const unsigned char *)message,
		       strlen(message)) < 0)
		fprintf(stderr, "Cannot write to log file\n");
}

/* Return 1 when ready, 0 on termination, and -1 on error. */
static int wait_port(int fd, short events)
{
	struct pollfd port = { .fd = fd, .events = events };

	while (!stop_signal) {
		int result = poll(&port, 1, 200);

		if (result < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (port.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			errno = EIO;
			return -1;
		}
		if (result > 0 && (port.revents & events))
			return 1;
	}
	return 0;
}

static int write_all(int fd, const unsigned char *data, size_t length)
{
	while (length > 0) {
		int ready = wait_port(fd, POLLOUT);
		if (ready <= 0)
			return ready;

		ssize_t written = write(fd, data, length);
		if (written < 0) {
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				continue;
			return -1;
		}
		if (written == 0) {
			errno = EIO;
			return -1;
		}
		data += written;
		length -= (size_t)written;
	}
	return 1;
}

int main(int argc, char **argv)
{
	FILE *log = NULL;
	int fd = -1;
	int have_settings = 0;
	int status = EXIT_FAILURE;
	struct termios original, settings;
	struct sigaction action = { 0 };
	unsigned char message[MAX_MESSAGE + 2];
	size_t used = 0;
	char stamp[64], filename[128];
	const char *operation = "create log directory";

	if (argc != 2) {
		fprintf(stderr, "Usage: %s /dev/ttyAMA0\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (mkdir("logs", 0755) < 0 && errno != EEXIST)
		goto error;
	operation = "create log filename";
	if (timestamp(stamp, sizeof(stamp), "%Y%m%d-%H%M%S") < 0)
		goto error;
	snprintf(filename, sizeof(filename), "logs/%s-%ld.log", stamp,
		 (long)getpid());
	operation = "open log file";
	log = fopen(filename, "wx");
	if (log == NULL)
		goto error;

	operation = "install signal handlers";
	action.sa_handler = handle_signal;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0 ||
	    sigaction(SIGTERM, &action, NULL) < 0)
		goto error;

	operation = argv[1];
	fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		goto error;
	operation = "tcgetattr";
	if (tcgetattr(fd, &original) < 0)
		goto error;
	have_settings = 1;
	settings = original;

	cfmakeraw(&settings);
	settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
	settings.c_cflag |= CS8 | CLOCAL | CREAD;
	settings.c_iflag &= ~(IXON | IXOFF | IXANY);
	settings.c_cc[VMIN] = 1;
	settings.c_cc[VTIME] = 0;

	operation = "set baud rate";
	if (cfsetispeed(&settings, B115200) < 0 ||
	    cfsetospeed(&settings, B115200) < 0)
		goto error;
	operation = "tcsetattr";
	if (tcsetattr(fd, TCSANOW, &settings) < 0)
		goto error;

	operation = "write startup log";
	if (log_record(log, "START", (const unsigned char *)argv[1],
		       strlen(argv[1])) < 0)
		goto error;
	fprintf(stderr, "Ready: %s, 115200 8N1; log: %s\n", argv[1], filename);

	while (!stop_signal) {
		operation = "read UART";
		int ready = wait_port(fd, POLLIN);
		if (ready < 0)
			goto error;
		if (ready == 0)
			break;

		unsigned char byte;
		ssize_t count = read(fd, &byte, 1);
		if (count < 0) {
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				continue;
			goto error;
		}
		if (count == 0) {
			errno = EIO;
			goto error;
		}

		message[used++] = byte;
		if (used >= 2 && message[used - 2] == '\r' &&
		    message[used - 1] == '\n') {
			operation = "write received message to log";
			if (log_record(log, "RX", message, used - 2) < 0)
				goto error;

			for (size_t i = 0; i < used - 2; ++i) {
				if (message[i] >= 'a' && message[i] <= 'z')
					message[i] =
						(unsigned char)(message[i] -
								'a' + 'A');
				else if (message[i] >= 'A' && message[i] <= 'Z')
					message[i] =
						(unsigned char)(message[i] -
								'A' + 'a');
			}

			operation = "write UART";
			int result = write_all(fd, message, used);
			if (result < 0)
				goto error;
			if (result == 0)
				break;
			used = 0;
		} else if (used == sizeof(message)) {
			operation = "message exceeds 1024 bytes without CRLF";
			errno = EMSGSIZE;
			goto error;
		}
	}

	operation = "write shutdown log";
	const char *reason = stop_signal == SIGINT ? "SIGINT" : "SIGTERM";
	if (log_record(log, "STOP", (const unsigned char *)reason,
		       strlen(reason)) < 0)
		goto error;
	status = EXIT_SUCCESS;
	goto cleanup;

error:
	report_error(log, operation);

cleanup:
	if (have_settings && tcsetattr(fd, TCSANOW, &original) < 0) {
		report_error(log, "restore UART settings");
		status = EXIT_FAILURE;
	}
	if (fd >= 0 && close(fd) < 0) {
		report_error(log, "close UART");
		status = EXIT_FAILURE;
	}
	if (log != NULL && fclose(log) == EOF) {
		perror("close log file");
		status = EXIT_FAILURE;
	}
	return status;
}