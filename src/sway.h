#pragma once

#include <stdint.h>

enum : uint32_t {
	SWAY_RUN_COMMAND = 0,
	SWAY_SUBSCRIBE = 2,
	SWAY_GET_TREE = 4,
	SWAY_EV_MODE = 0x80000002,
	SWAY_EV_WINDOW = 0x80000003,
};

enum : uint8_t {
	L_NONE,
	L_H,
	L_V,
};

struct focus {
	uint64_t id;
	uint32_t w;
	uint32_t h;
	uint8_t con;
	uint8_t fs;
	uint8_t playout;
};

int sway_open(const char *path);
void sway_send(int fd, uint32_t type, const char *p, uint32_t n);
uint32_t sway_recv(int fd, uint32_t *type);
void sway_read(int fd, void *b, uint32_t n);
void sway_skip(int fd, uint32_t n);
bool tree_parse(int fd, uint32_t len, struct focus *o);
bool sway_focus(int fd, struct focus *o);
