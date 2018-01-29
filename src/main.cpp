#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <poll.h>

void print_error(const char *tag, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "[%s] ", tag);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void panic(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "[panic] ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(-1);
}

void panic() {
	fprintf(stderr, "[panic]\n");
	exit(-1);
}

bool _assert(bool condition, const char *msg, const char *cond_str, const char *file, const char *func, size_t line) {
	if (!condition) {
		fprintf(stderr, "Assertion failed: %s\n", cond_str);
		if (msg) {
			fprintf(stderr, "%s\n", msg);
		}
		fprintf(stderr, "%s(%lu):%s\n", file, line, func);
		exit(-1);
	}
	return condition;
}

#define assertm(cond, msg) _assert(cond, msg, #cond, __FILE__, __func__, __LINE__)
#define assert(cond) _assert(cond, NULL, #cond, __FILE__, __func__, __LINE__)

struct vec2 {
	int x, y;
};

vec2 &operator +=(vec2 &lhs, const vec2 &rhs) {
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	return lhs;
}

vec2 &operator -=(vec2 &lhs, const vec2 &rhs) {
	lhs.x -= rhs.x;
	lhs.y -= rhs.y;
	return lhs;
}

vec2 operator +(const vec2 &lhs, const vec2 &rhs) {
	vec2 res = lhs;
	res += rhs;
	return res;
}

vec2 operator -(const vec2 &lhs, const vec2 &rhs) {
	vec2 res = lhs;
	res -= rhs;
	return res;
}

enum Direction {
	DIR_LEFT,
	DIR_RIGHT,
	DIR_UP,
	DIR_DOWN,
};

enum LpColor {
	COL_RED_OFF = 0x00,
	COL_RED_LOW = 0x01,
	COL_RED_MED = 0x02,
	COL_RED_FUL = 0x03,

	SET_PXL_CPY = 0x04,
	SET_PXL_CLR = 0x08,

	COL_GRN_OFF = 0x00,
	COL_GRN_LOW = 0x10,
	COL_GRN_MED = 0x20,
	COL_GRN_FUL = 0x30,
};

//      (name,  passable, symbol, color)
#define TILE_TYPES \
	TILE(WALL,  false,    '#',   COL_RED_LOW)				\
	TILE(FLOOR, true,     ' ',   0          )				\
	TILE(GOAL,  true,     'x',   COL_GRN_LOW)

//      (name,     symbol, color)
#define ENTITY_TYPES \
	ENTITY(NONE,   0,     0)						\
	ENTITY(PLAYER, '%',   COL_RED_FUL|COL_GRN_FUL)							\
	ENTITY(BOX,    'b',   COL_GRN_FUL)

enum TileType {
#define TILE(name, passable, symbol, color) TILE_##name,
	TILE_TYPES
#undef TILE

	TILE_LAST
};

struct TileDef {
	TileType type;
	const char *name;
	bool passable;
	char symbol;
	uint8_t color;
};

TileDef tile_definitions[] = {
#define TILE(name, passable, symbol, color) {TILE_##name, #name, passable, symbol, color},
	TILE_TYPES
#undef TILE
};

enum EntityType {
#define ENTITY(name, symbol, color) ENTITY_##name,
	ENTITY_TYPES
#undef ENTITY

	ENTITY_LAST
};


struct EntityDef {
	EntityType type;
	const char *name;
	char symbol;
	uint8_t color;
};

EntityDef entity_definitions[] = {
#define ENTITY(name, symbol, color) {ENTITY_##name, #name, symbol, color},
	ENTITY_TYPES
#undef ENTITY
};

struct Tile {
	TileType type;
	EntityType entity;
};

struct Board
{
	vec2 size;
	vec2 player_location;
	Tile *tiles;
};

const char *map =
	"########"
	"#%b   x#"
	"# b #  #"
	"#   #  #"
	"#   #  #"
	"#   #  #"
	"#     x#"
	"########";

Tile *get_tile(Board *board, vec2 pos) {
	assert(pos.x >= 0 && pos.x < board->size.x);
	assert(pos.y >= 0 && pos.y < board->size.y);
	return &board->tiles[pos.x + pos.y * board->size.x];
}

bool point_inside_board(Board *board, vec2 pos) {
	return (pos.x >= 0 && pos.x < board->size.x) &&
		(pos.y >= 0 && pos.y < board->size.y);
}

vec2 dir_to_vec(Direction direction) {
	vec2 dir = {};

	switch (direction) {
	case DIR_UP:   dir = { 0, -1}; break;
	case DIR_DOWN: dir = { 0,  1}; break;
	case DIR_LEFT: dir = {-1,  0}; break;
	case DIR_RIGHT: dir = { 1,  0}; break;
	default:
		print_error("game", "Unknown direction");
		break;
	}

	return dir;
}

bool has_won(Board *board) {
	for (int i = 0; i < board->size.x * board->size.y; ++i) {
		Tile *tile = &board->tiles[i];

		if (tile->type == TILE_GOAL && tile->entity != ENTITY_BOX) {
			return false;
		}
	}

	return true;
}

bool load_board(Board *board, const char *mapdata, vec2 board_size) {
	int board_length = board_size.x * board_size.y;

	board->size = board_size;
	board->player_location.x = 0;
	board->player_location.y = 0;
	board->tiles = (Tile *)calloc(board_length, sizeof(Tile));

	for (int i = 0; i < board_length; i += 1) {
		char c = mapdata[i];
		vec2 p;
		p.x = i % board_size.x;
		p.y = i / board_size.x;

		Tile tile = {};

		switch (c) {
		case '#':
			tile.type = TILE_WALL;
			break;
		case ' ':
			tile.type = TILE_FLOOR;
			break;
		case '%':
			tile.type = TILE_FLOOR;
			tile.entity = ENTITY_PLAYER;
			board->player_location = p;
			break;
		case 'b':
			tile.type = TILE_FLOOR;
			tile.entity = ENTITY_BOX;
			break;
		case 'x':
			tile.type = TILE_GOAL;
			break;

		case 0:
			print_error("load map", "End of map data before the entire board is filled.");
			free(board->tiles);
			return false;
		default:
			print_error("load map", "Unrecognised tile symbol %c at (%i,%i)", c, p.x, p.y);
			break;
		}

		board->tiles[i] = tile;
	}

	return true;
}

bool move_player(Board *board, Direction direction)
{
	Tile *player_tile = get_tile(board, board->player_location);
	assert(player_tile->entity == ENTITY_PLAYER);

	vec2 dir = dir_to_vec(direction);

	vec2 new_player_location = board->player_location + dir;

	if (!point_inside_board(board, new_player_location)) {
		return false;
	}

	Tile *player_target = get_tile(board, new_player_location);

	if (!tile_definitions[player_target->type].passable) {
		return false;
	}

	if (player_target->entity == ENTITY_BOX) {
		vec2 new_box_location = new_player_location + dir;

		if (!point_inside_board(board, new_box_location)) {
			return false;
		}

		Tile *box_target = get_tile(board, new_box_location);

		if (!tile_definitions[box_target->type].passable) {
			return false;
		}

		if (box_target->entity != ENTITY_NONE) {
			return false;
		}

		box_target->entity = ENTITY_BOX;
		player_target->entity = ENTITY_NONE;
	}

	if (player_target->entity != ENTITY_NONE) {
		return false;
	}

	player_tile->entity = ENTITY_NONE;
	player_target->entity = ENTITY_PLAYER;

	board->player_location = new_player_location;

	return true;
}

void print_board(Board *board)
{
	for (int y = 0; y < board->size.y; y += 1) {
		for (int x = 0; x < board->size.x; x += 1) {
			Tile *tile;
			char out;

			tile = get_tile(board, {x, y});

			assert(tile->type < TILE_LAST);
			assert(tile->entity < ENTITY_LAST);

			out = tile_definitions[tile->type].symbol;

			if (entity_definitions[tile->entity].symbol) {
				out = entity_definitions[tile->entity].symbol;
			}

			printf("%c", out);
		}
		printf("\n");
	}
}

int lp_set_pixel(int dev, uint8_t x, uint8_t y, uint8_t color, uint8_t flags) {
	uint8_t key = x + y * 16;
	uint8_t vel;
	vel  = color;
	vel |= flags;
	uint8_t packet[4] = {0x90, key, vel};
	return write(dev, packet, sizeof(packet));
}

void lp_print_board(int dev, Board *board)
{
	for (int y = 0; y < board->size.y; y += 1) {
		for (int x = 0; x < board->size.x; x += 1) {
			Tile *tile;
			uint8_t out;

			tile = get_tile(board, {x, y});

			assert(tile->type < TILE_LAST);
			assert(tile->entity < ENTITY_LAST);

			out = tile_definitions[tile->type].color;

			if (entity_definitions[tile->entity].color) {
				out = entity_definitions[tile->entity].color;
			}

			lp_set_pixel(dev, x, y, out, SET_PXL_CPY|SET_PXL_CLR);
		}
	}
}

void lp_clear_input(int dev) {
	uint8_t buffer[3];

	struct pollfd fds = {
		.fd = dev,
		.events = POLLIN,
		.revents = 0,
	};

	while (poll(&fds, 1, 0)) {
		read(dev, buffer, sizeof(buffer));
	}
}

void lp_transition_out(int dev, uint8_t color) {
	for (int i = 0; i < 16; ++i) {
		for (int j = 0; j < i; ++j) {
			int x = j;
			int y = i - j;
			if (x > 7 || y > 7)
				continue;
			lp_set_pixel(dev, x, y, color, 0);
		}
		usleep(100000);
	}
	usleep(2000000);
}

int main(int argc, char *argv[])
{
	const char *midi_device = "/dev/midi1";
	int fd = open(midi_device, O_RDWR, 0);
	if (fd < 0) {
		print_error("midi", "Could not open %s", midi_device);
		return -1;
	}

	Board board;
	if (!load_board(&board, map, {8, 8})) {
		return -1;
	}

	print_board(&board);
	lp_print_board(fd, &board);
	while (true) {
		uint8_t buffer[3];

		read(fd, buffer, sizeof(buffer));

		if (buffer[0] == 0x90 && buffer[2] == 0x7f) {
			uint8_t x, y;
			x = buffer[1] % 8;
			y = buffer[1] / 16;

			if (x == 0) {
				move_player(&board, DIR_LEFT);
			} else if (x == 7) {
				move_player(&board, DIR_RIGHT);
			} else if (y == 0) {
				move_player(&board, DIR_UP);
			} else if (y == 7) {
				move_player(&board, DIR_DOWN);
			}
			lp_set_pixel(fd, x, y, COL_RED_FUL|COL_GRN_FUL, 0);
		} else if (buffer[0] == 0xb0) {
			switch (buffer[1] - 104) {
			case 0: {
				if (!load_board(&board, map, {8, 8})) {
					return -1;
				}
			} break;
			}
		}

		if (has_won(&board)) {
			printf("You have won!\n");
			lp_transition_out(fd, COL_GRN_FUL);
			return 0;
		}

		print_board(&board);
		lp_print_board(fd, &board);
	}

	close(fd);
	return 0;
}
