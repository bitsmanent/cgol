/* Conway's Game of Life */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "arg.h"
char *argv0;

/* macros */
#define LENGTH(X)       (sizeof X / sizeof X[0])

/* VT100 escape sequences */
#define CLEARRIGHT      "\33[0K"
#define CURPOS          "\33[%d;%dH"
#define CURSON          "\33[?25h"
#define CURSOFF         "\33[?25l"

/* function declarations */
void *ecalloc(size_t nmemb, size_t size);
void die(const char *fmt, ...);
void draw(void);
int msleep(int ms);
int mvprintf(int x, int y, char *fmt, ...);
int neighbors(int pos);
int gridfile(char *file);
void gridrand(int w, int h);
void resize(int x, int y);
void setup(void);
void sigwinch(int unused);
void tick(void);
void usage(void);

/* variables */
int gw, gh, gs; /* grid, width, height, size */
int *grid, *diff;
int rows, cols;
int generation;

/* function implementations */
void *
ecalloc(size_t nmemb, size_t size) {
	void *p;

	if(!(p = calloc(nmemb, size)))
		die("Cannot allocate memory.");
	return p;
}

void
die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(0);
}

void
draw(void) {
	int r, c;

	printf(CURSOFF);
	mvprintf(1, 1, "Conway's Game of Life ⋅ #%d ⋅ %0dx%0d" CLEARRIGHT, generation, rows, cols);
	for(r = 0; r < rows - 1 && r < gh; ++r)
		for(c = 0; c < cols && c < gw; ++c)
			mvprintf(1+c, 2+r, "%s", grid[r * gw + c] ? "\033[07m \033[m" : " ");
	printf(CURPOS CURSON, rows, cols);
}

int
gridfile(char *file) {
	FILE *fp;
	char *line, *p;
	size_t len = 0, nc;
	int r, c;

	fp = fopen(file, "r");
	if(!fp)
		return -1;
	r = 0;
	while((nc = getline(&line, &len, fp)) != -1) {
		p = line;
		c = 0;
		while(*p && *p != '\n') {
			grid[r * gw + c] = (*p == '1') ? 1 : 0;
			++c;
			p += 2;
		}
		++r;
	}
	free(line);
	fclose(fp);
	return 0;
}

void
gridrand(int w, int h) {
	int r, c;

	for(r = 0; r < h && r < gh; ++r)
		for(c = 0; c < w && c < gw; ++c)
			grid[r * gw + c] = random() % 4 ? 0 : 1;
}

int
msleep(int ms) {
	struct timespec req = {0}, rem;
	int r = ms / 1000;

	if(r >= 1) {
		req.tv_sec = r;
		ms -= r * 1000;
	}
	if(ms)
		req.tv_nsec = ms * 1000000;

	while((r = nanosleep(&req, &rem)) == -1 && errno == EINTR)
		req = rem;
	return r;
}

int
mvprintf(int x, int y, char *fmt, ...) {
	va_list ap;
	int len;

	printf(CURPOS, y, x);
	va_start(ap, fmt);
	len = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return len;
}

int
neighbors(int pos) {
	int r = pos / gw;
	int c = pos % gw;
	int sides[] = {
		r - 1 >= 0 && c - 1 >= 0 ? (r - 1) * gw + (c - 1) : -1, /* top-left */
		r - 1 >= 0 ? (r - 1) * gw + c : -1,                     /* top */
		r - 1 >= 0 && c + 1 < gw ? (r - 1) * gw + (c + 1) : -1, /* top-right */
		c + 1 < gw ? r * gw + (c + 1) : -1,                     /* right */
		r + 1 < gh && c + 1 < gw ? (r + 1) * gw + (c + 1) : -1, /* bottom-right */
		r + 1 < gh ? (r + 1) * gw + c : -1,                     /* bottom */
		r + 1 < gh && c - 1 >= 0 ? (r + 1) * gw + (c - 1) : -1, /* bottom-left */
		c - 1 >= 0 ? r * gw + (c - 1) : -1,                     /* left */
	};
	int n = 0, i;

	for(i = 0; i < LENGTH(sides); ++i)
		n += sides[i] >= 0 ? grid[sides[i]] : 0;
	return n;
}

void
resize(int wsrow, int wscol) {
	rows = wsrow;
	cols = wscol;
}

void
setup(void) {
	struct sigaction sa;
	struct winsize ws;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigwinch;
	sigaction(SIGWINCH, &sa, NULL);
	ioctl(0, TIOCGWINSZ, &ws);
	resize(ws.ws_row, ws.ws_col);
	setbuf(stdout, NULL);
}

void
sigwinch(int unused) {
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);
	resize(ws.ws_row, ws.ws_col);
	draw();
}

void
tick(void) {
	int i, n;

	for(i = 0; i < gs; ++i) {
		n = neighbors(i);
		if(grid[i] == 1 && (n < 2 || n > 3))
			diff[i] = 0;
		else if(grid[i] == 0 && n == 3)
			diff[i] = 1;
		else
			diff[i] = grid[i];
	}
	for(i = 0; i < gs; ++i)
		if(grid[i] != diff[i])
			grid[i] = diff[i];
	++generation;
}

void
usage(void) {
	die("Usage: %s [-v] [-dgn <arg>] [file]", argv0);
}

int
main(int argc, char *argv[]) {
	int delay = 250, gen = 0, ticks = 0;
	char *file = NULL;

	ARGBEGIN {
	case 'd': delay = atoi(EARGF(usage())); break;
	case 'g': gen = atoi(EARGF(usage())); break;
	case 'n': ticks = atoi(EARGF(usage())); break;
	case 'v': die("cgol-"VERSION);
	default: usage();
	} ARGEND;

	if(argc) file = argv[0];
	setup();

	if(!gw) gw = 256;
	if(!gh) gh = 256;
	gs = gw * gw;
	grid = ecalloc(gs, sizeof(int));
	diff = ecalloc(gs, sizeof(int));

	if(file) {
		if(gridfile(file))
			die("%s: %s:", argv0, file);
	}
	else {
		srandom(time(NULL));
		gridrand(cols, rows);
	}

	while(gen-- > 0)
		tick();
	while(1) {
		draw();
		tick();
		if(!--ticks)
			break;
		msleep(delay);
	}
	/* NOTREACHED */
	free(grid);
	free(diff);
	return 0;
}
