#include <stdio.h>
#include <stdlib.h>

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

//      (name,  passable, symbol)
#define TILE_TYPES \
	TILE(WALL,  false,    '#')				\
	TILE(FLOOR, true,     ' ')				\
	TILE(GOAL,  true,     'x')

//      (name,     symbol)
#define ENTITY_TYPES \
	ENTITY(NONE,   0)						\
	ENTITY(PLAYER, '%')							\
	ENTITY(BOX,    'b')

enum TileType {
#define TILE(name, passable, symbol) TILE_##name,
	TILE_TYPES
#undef TILE

	TILE_LAST
};

struct TileDef {
	TileType type;
	const char *name;
	bool passable;
	char symbol;
};

TileDef tile_definitions[] = {
#define TILE(name, passable, symbol) {TILE_##name, #name, passable, symbol},
	TILE_TYPES
#undef TILE
};

enum EntityType {
#define ENTITY(name, symbol) ENTITY_##name,
	ENTITY_TYPES
#undef ENTITY

	ENTITY_LAST
};


struct EntityDef {
	EntityType type;
	const char *name;
	char symbol;
};

EntityDef entity_definitions[] = {
#define ENTITY(name, symbol) {ENTITY_##name, #name, symbol},
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
			panic("at the disco!");
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

int main(int argc, char *argv[])
{
	Board board;
	if (!load_board(&board, map, {8, 8})) {
		return -1;
	}

	print_board(&board);
	while (true) {
		int r = fgetc(stdin);
		switch (r) {
		case 'w': move_player(&board, DIR_UP); break;
		case 'a': move_player(&board, DIR_LEFT); break;
		case 's': move_player(&board, DIR_DOWN); break;
		case 'd': move_player(&board, DIR_RIGHT); break;
		default: continue;
		}
		print_board(&board);

		if (has_won(&board)) {
			printf("You have won!\n");
			break;
		}
	}
}
