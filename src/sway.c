#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "sway.h"

#define HDR	14
#define DEPTH	128

enum : uint8_t {
	K_NONE,
	K_ID,
	K_TYPE,
	K_LAYOUT,
	K_FOCUSED,
	K_RECT,
	K_WIDTH,
	K_HEIGHT,
	K_FS,
};

struct frame {
	uint64_t id;
	uint32_t w;
	uint32_t h;
	uint8_t key;
	uint8_t inkey;
	uint8_t con;
	uint8_t layout;
	uint8_t focused;
	uint8_t fs;
	uint8_t child;
};

static char buf[8192];
static struct frame st[DEPTH];

int sway_open(const char *path)
{
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	size_t n = strlen(path);
	int fd;

	if (n >= sizeof(sa.sun_path))
		errx(1, "%s: is %zu bytes, the maximum is %zu", path, n, sizeof(sa.sun_path) - 1);
	memcpy(sa.sun_path, path, n);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		err(1, "socket");
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		err(1, "connect %s", path);
	return fd;
}

void sway_send(int fd, uint32_t type, const char *p, uint32_t n)
{
	char m[HDR + 64];

	memcpy(m, "i3-ipc", 6);
	memcpy(m + 6, &n, 4);
	memcpy(m + 10, &type, 4);
	memcpy(m + HDR, p, n);
	if (send(fd, m, HDR + n, MSG_NOSIGNAL) < 0)
		err(1, "send");
}

void sway_read(int fd, void *b, uint32_t n)
{
	char *p = b;

	while (n) {
		ssize_t r = read(fd, p, n);

		if (r <= 0) {
			if (!r)
				exit(0);
			if (errno == EINTR)
				continue;
			err(1, "read");
		}
		p += r;
		n -= (uint32_t)r;
	}
}

uint32_t sway_recv(int fd, uint32_t *type)
{
	char h[HDR];
	uint32_t n;

	sway_read(fd, h, HDR);
	memcpy(&n, h + 6, 4);
	memcpy(type, h + 10, 4);
	return n;
}

void sway_skip(int fd, uint32_t n)
{
	while (n) {
		uint32_t k = n < sizeof(buf) ? n : sizeof(buf);

		sway_read(fd, buf, k);
		n -= k;
	}
}

static uint8_t keyof(const char *t, uint32_t n)
{
	switch (n) {
	case 2:
		return memcmp(t, "id", 2) ? K_NONE : K_ID;
	case 4:
		if (!memcmp(t, "type", 4))
			return K_TYPE;
		return memcmp(t, "rect", 4) ? K_NONE : K_RECT;
	case 5:
		return memcmp(t, "width", 5) ? K_NONE : K_WIDTH;
	case 6:
		if (!memcmp(t, "layout", 6))
			return K_LAYOUT;
		return memcmp(t, "height", 6) ? K_NONE : K_HEIGHT;
	case 7:
		return memcmp(t, "focused", 7) ? K_NONE : K_FOCUSED;
	case 15:
		return memcmp(t, "fullscreen_mode", 15) ? K_NONE : K_FS;
	}
	return K_NONE;
}

static uint64_t num(const char *t, uint32_t n)
{
	uint64_t v = 0;

	if (n > 16)
		n = 16;
	for (uint32_t i = 0; i < n; i++)
		v = v * 10 + (uint64_t)(t[i] - '0');
	return v;
}

static void value(struct frame *f, const char *t, uint32_t n)
{
	switch (f->key) {
	case K_ID:
		f->id = num(t, n);
		break;
	case K_WIDTH:
		f->w = (uint32_t)num(t, n);
		break;
	case K_HEIGHT:
		f->h = (uint32_t)num(t, n);
		break;
	case K_FS:
		f->fs = t[0] != '0';
		break;
	case K_FOCUSED:
		f->focused = t[0] == 't';
		break;
	case K_TYPE:
		f->con = n == 3 && !memcmp(t, "con", 3);
		break;
	case K_LAYOUT:
		f->layout = L_NONE;
		if (n == 6 && !memcmp(t, "split", 5))
			f->layout = t[5] == 'h' ? L_H : t[5] == 'v' ? L_V : L_NONE;
		break;
	}
}

// the parent closes after the focused child, so its layout is known regardless of key order
static bool pop(const struct frame *f, struct frame *p, struct focus *o)
{
	if (f->inkey == K_RECT) {
		p->w = f->w;
		p->h = f->h;
	} else if (f->focused) {
		o->id = f->id;
		o->w = f->w;
		o->h = f->h;
		o->con = f->con;
		o->fs = f->fs;
		p->child = 1;
	} else if (f->child) {
		o->playout = f->layout;
		return true;
	}
	return false;
}

// a token followed by : is a key, followed by , ] or } a value
bool tree_parse(int fd, uint32_t len, struct focus *o)
{
	char tok[16] = {};
	uint32_t d = 0, n = 0;
	bool str = false, esc = false, pend = false, ret = false;

	st[0] = (struct frame){};
	while (len) {
		uint32_t k = len < sizeof(buf) ? len : sizeof(buf);

		sway_read(fd, buf, k);
		len -= k;
		for (uint32_t i = 0; i < k; i++) {
			struct frame *t = &st[d];
			char c = buf[i];

			if (str) {
				if (esc)
					esc = false;
				else if (c == '\\')
					esc = true;
				else if (c == '"')
					str = false;
				else if (n++ < sizeof(tok))
					tok[n - 1] = c;
				continue;
			}
			switch (c) {
			case '"':
				str = pend = true;
				n = 0;
				break;
			case ':':
				t->key = keyof(tok, n);
				pend = false;
				break;
			case '{':
				if (++d == DEPTH)
					goto out;
				st[d] = (struct frame){ .inkey = t->key };
				break;
			case '}':
				if (pend)
					value(t, tok, n);
				pend = false;
				if (!d)
					goto out;
				if (pop(t, &st[d - 1], o)) {
					ret = true;
					goto out;
				}
				d--;
				break;
			case ',':
			case ']':
				if (pend)
					value(t, tok, n);
				pend = false;
				break;
			case '[':
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				break;
			default:
				if (!pend) {
					pend = true;
					n = 0;
				}
				if (n++ < sizeof(tok))
					tok[n - 1] = c;
			}
		}
	}
out:
	sway_skip(fd, len);
	return ret;
}

bool sway_focus(int fd, struct focus *o)
{
	uint32_t type;

	sway_send(fd, SWAY_GET_TREE, "", 0);
	return tree_parse(fd, sway_recv(fd, &type), o);
}
