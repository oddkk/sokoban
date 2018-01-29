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


int main(int argc, char *argv[])
{
	print_error("foo", "Hello, World!");
	panic("asdf");
}
