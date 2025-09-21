#include <linux/input.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#include <xdo.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdbool.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define MAX(x, y) ((x>y)?x:y)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

typedef enum taikohit {
    TAIKO_INVALID,
    TAIKO_LEFT_KA,
    TAIKO_LEFT_DON,
    TAIKO_RIGHT_DON,
    TAIKO_RIGHT_KA
} taikohit_t;

static int get_state(int fd, unsigned int type, unsigned long *array, size_t size)
{
	int rc;

	switch(type) {
	case EV_LED:
		rc = ioctl(fd, EVIOCGLED(size), array);
		break;
	case EV_SND:
		rc = ioctl(fd, EVIOCGSND(size), array);
		break;
	case EV_SW:
		rc = ioctl(fd, EVIOCGSW(size), array);
		break;
	case EV_KEY:
		/* intentionally not printing the value for EV_KEY, let the
		 * repeat handle this */
	default:
		return 1;
	}
	if (rc == -1)
		return 1;

	return 0;
}

bool stop = false;

void interrupt_handler(int sig) {
    stop = true;
}

/**
 * Print static device information (no events). This information includes
 * version numbers, device name and all bits supported by this device.
 *
 * @param fd The file descriptor to the device.
 * @return 0 on success or 1 otherwise.
 */
static int print_device_info(int fd)
{
	unsigned int type, code;
	int version;
	unsigned short id[4];
	char name[256] = "Unknown";
	unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
	unsigned long state[KEY_CNT] = {0};
#ifdef INPUT_PROP_SEMI_MT
	unsigned int prop;
	unsigned long propbits[INPUT_PROP_MAX];
#endif
	int stateval;
	int have_state;

	if (ioctl(fd, EVIOCGVERSION, &version)) {
		perror("evtest: can't get version");
		return 1;
	}

	printf("Input driver version is %d.%d.%d\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff);

	ioctl(fd, EVIOCGID, id);
	printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
		id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

	ioctl(fd, EVIOCGNAME(sizeof(name)), name);
	printf("Input device name: \"%s\"\n", name);

	memset(bit, 0, sizeof(bit));
	ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
	printf("Supported events:\n");

	for (type = 0; type < EV_MAX; type++) {
		if (test_bit(type, bit[0]) && type != EV_REP) {
			have_state = (get_state(fd, type, state, sizeof(state)) == 0);

			printf("  Event type %d\n", type);
			if (type == EV_SYN) continue;
			ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit[type]);
			for (code = 0; code < KEY_MAX; code++)
				if (test_bit(code, bit[type])) {
					if (have_state) {
						stateval = test_bit(code, state);
						printf("    Event code %d state %d\n",
						       code, stateval);
					} else {
						printf("    Event code %d\n", code);
					}
					if (type == EV_ABS) {
						//print_absdata(fd, code);
                    }
				}
		}
	}

	if (test_bit(EV_REP, bit[0])) {
		printf("Key repeat handling:\n");
		printf("  Repeat type %d\n", EV_REP);
		//print_repdata(fd);
	}
#ifdef INPUT_PROP_SEMI_MT
	memset(propbits, 0, sizeof(propbits));
	ioctl(fd, EVIOCGPROP(sizeof(propbits)), propbits);
	printf("Properties:\n");
	for (prop = 0; prop < INPUT_PROP_MAX; prop++) {
		if (test_bit(prop, propbits))
			printf("  Property type %d\n", prop);
	}
#endif

	return 0;
}

static xdo_t *xd;

static void handle_hit(int x, int y) {
	taikohit_t hit = TAIKO_INVALID;
	static const int WIDTH = 1280, HEIGHT = 720;
	if (x != -1 && y != -1) {
		if (x > WIDTH / 2) {
			if (y > (HEIGHT / 2) && x < (3 * WIDTH / 4)) {
				hit = TAIKO_RIGHT_DON;
			}
			else {
				hit = TAIKO_RIGHT_KA;
			}
		}
		else {
			if (y > (HEIGHT / 2) && x > (WIDTH / 4)) {
				hit = TAIKO_LEFT_DON;
			}
			else {
				hit = TAIKO_LEFT_KA;
			}
		}
	}
	switch (hit) {
		case TAIKO_LEFT_DON:
			puts("left don");
			xdo_send_keysequence_window(xd, CURRENTWINDOW, "f", 0);
			break;
		case TAIKO_RIGHT_DON:
			puts("right don");
			xdo_send_keysequence_window(xd, CURRENTWINDOW, "j", 0);
			break;
		case TAIKO_LEFT_KA:
			puts("left ka");
			xdo_send_keysequence_window(xd, CURRENTWINDOW, "d", 0);
			break;
		case TAIKO_RIGHT_KA:
			puts("right ka");
			xdo_send_keysequence_window(xd, CURRENTWINDOW, "k", 0);
			break;
		default:
			break;
	}
}

static bool enabled = true;
static void process_keyboard_event(struct input_event *ev) {
	if (ev->type == EV_KEY && ev->code == KEY_LEFTSHIFT && ev->value == 1) {
		// Backspace down
		enabled = !enabled;
		if (enabled) {
			printf("enabled\n");
		}
		else {
			printf("disabled\n");
		}
	}
}

static void process_touchpad_event(struct input_event *ev) {
	static int x = -1, y = -1, touch = 0;

	int type = ev->type;
	int code = ev->code;
	if (type == EV_SYN) {
		if (touch == 1 && enabled) {
			handle_hit(x, y);
		}
		x = -1;
		y = -1;
		touch = 0;
	}
	else if (type == EV_ABS) {
		if (code == ABS_X) {
			x = ev->value;
		}
		else if (code == ABS_Y) {
			y = ev->value;
		}
	}
	else if (type == EV_KEY && code == BTN_TOUCH) {
		touch = ev->value;
	}
}

/**
 * Print device events as they come in.
 *
 * @param fd The file descriptor to the device.
 * @return 0 on success or 1 otherwise.
 */
static int listen_events(int touchpad_fd, int keyboard_fd)
{
    int fail = 0;
	struct input_event ev[64];
	int i, rd;

	static void (*process_cbs[2])(struct input_event *) = {
		process_touchpad_event,
		process_keyboard_event
	};
	struct pollfd fds[2] = {
		{ .fd = touchpad_fd, .events = POLLIN },
		{ .fd = keyboard_fd, .events = POLLIN }
	};
    
    int x = -1, y = -1, touch = 0;

	while (!stop) {
		int ret = poll(fds, 2, -1);
		if (ret == -1) {
			perror("poll");
			fail = 1;
			break;
		}
		if (stop) {
			break;
		}

		for (int i=0; i<(sizeof(fds) / sizeof(*fds)); ++i) {
			if (!(fds[i].revents & POLLIN)) {
				continue;
			}
            rd = read(fds[i].fd, ev, sizeof(ev));

            if (rd < (int)sizeof(struct input_event)) {
                printf("expected %d bytes, got %d\n", (int)sizeof(struct input_event), rd);
                perror("\nevtest: error reading");
                return 1;
            }
            for (int j = 0; j < rd / sizeof(struct input_event); ++j) {
				(process_cbs[i])(&ev[j]);
			}
		}
	}

	ioctl(touchpad_fd, EVIOCGRAB, (void*)0);
	ioctl(keyboard_fd, EVIOCGRAB, (void*)0);
	return fail;
}

/**
 * Grab and immediately ungrab the device.
 *
 * @param fd The file descriptor to the device.
 * @return 0 if the grab was successful, or 1 otherwise.
 */
static int test_grab(int fd, int grab_flag)
{
	int rc;

	rc = ioctl(fd, EVIOCGRAB, (void*)1);

	if (rc == 0 && !grab_flag)
		ioctl(fd, EVIOCGRAB, (void*)0);

	return rc;
}

/**
 * Enter capture mode. The requested event device will be monitored, and any
 * captured events will be decoded and printed on the console.
 *
 * @param device The device to monitor, or NULL if the user should be prompted.
 * @return 0 on success, non-zero on error.
 */
static int open_device(const char *filename, int grab_flag)
{
	int fd;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror("evtest");
		if (errno == EACCES && getuid() != 0)
			fprintf(stderr, "You do not have access to %s. Try "
					"running as root instead.\n",
					filename);
		goto error;
	}

	if (!isatty(fileno(stdout)))
		setbuf(stdout, NULL);

	if (print_device_info(fd))
		goto error;

	printf("Testing ... (interrupt to exit)\n");

	if (test_grab(fd, grab_flag))
	{
		printf("***********************************************\n");
		printf("  This device is grabbed by another process.\n");
		printf("  No events are available to evtest while the\n"
		       "  other grab is active.\n");
		printf("  In most cases, this is caused by an X driver,\n"
		       "  try VT-switching and re-run evtest again.\n");
		printf("  Run the following command to see processes with\n"
		       "  an open fd on this device\n"
		       " \"fuser -v %s\"\n", filename);
		printf("***********************************************\n");
	}

	if (grab_flag) {
		signal(SIGINT, interrupt_handler);
		signal(SIGTERM, interrupt_handler);
	}
    
    return fd;

error:
	return -1;
}

int main (int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <touchpad> <keyboard>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int touchpad = open_device(argv[1], 0);
    if (touchpad == -1) {
        fprintf(stderr, "failed to open touchpad\n");
        return EXIT_FAILURE;
    }
    int keyboard = open_device(argv[2], 0);
    if (keyboard == -1) {
        fprintf(stderr, "failed to open keyboard\n");
        return EXIT_FAILURE;
    }
	xd = xdo_new(":0.0");
	int ret = listen_events(touchpad, keyboard);
	xdo_free(xd);
	return ret;
}
