#include <err.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#include "sway.h"

static bool change(const char *s, size_t n)
{
	switch (n) {
	case 4:
		return !memcmp(s, "move", 4);
	case 5:
		return !memcmp(s, "focus", 5);
	case 8:
		return !memcmp(s, "floating", 8);
	case 15:
		return !memcmp(s, "fullscreen_mode", 15);
	}
	return false;
}

// sway adds change first, so its value is the first string after the first :
static bool event(int fd)
{
	char b[64];
	uint32_t type, n = sway_recv(fd, &type), k = n < sizeof(b) ? n : sizeof(b);
	const char *s, *e;

	sway_read(fd, b, k);
	sway_skip(fd, n - k);
	if (type == SWAY_EV_MODE)
		return true;
	if (type != SWAY_EV_WINDOW)
		return false;
	s = memchr(b, ':', k);
	if (!s)
		return false;
	s = memchr(s, '"', (size_t)(b + k - s));
	if (!s)
		return false;
	s++;
	e = memchr(s, '"', (size_t)(b + k - s));
	return e && change(s, (size_t)(e - s));
}

static uint32_t command(char *c, uint64_t id, uint8_t want)
{
	char d[20];
	uint32_t i = sizeof(d), n = 8;

	do {
		d[--i] = (char)('0' + id % 10);
		id /= 10;
	} while (id);
	memcpy(c, "[con_id=", 8);
	memcpy(c + n, d + i, sizeof(d) - i);
	n += (uint32_t)sizeof(d) - i;
	memcpy(c + n, "] split", 7);
	n += 7;
	c[n++] = want == L_V ? 'v' : 'h';
	return n;
}

static void tile(int fd)
{
	struct focus f = {};
	char c[40];
	uint32_t type;
	uint8_t want;

	if (!sway_focus(fd, &f) || !f.con || f.fs || f.playout == L_NONE)
		return;
	want = f.h > f.w ? L_V : L_H;
	if (want == f.playout)
		return;
	sway_send(fd, SWAY_RUN_COMMAND, c, command(c, f.id, want));
	sway_skip(fd, sway_recv(fd, &type));
}

int main(void)
{
	static const char sub[] = "[\"window\",\"mode\"]";
	const char *path = getenv("SWAYSOCK");
	struct pollfd p = { .events = POLLIN };
	int cmd;

	if (!path)
		errx(1, "SWAYSOCK is not set");
	p.fd = sway_open(path);
	cmd = sway_open(path);
	sway_send(p.fd, SWAY_SUBSCRIBE, sub, sizeof(sub) - 1);
	for (;;) {
		bool act = false;

		do
			act |= event(p.fd);
		while (poll(&p, 1, 0) > 0);
		if (act)
			tile(cmd);
	}
}
