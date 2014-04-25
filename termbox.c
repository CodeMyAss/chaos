#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <stdarg.h>

#include "termbox.h"

struct bytebuffer {
	char *buf;
	int len;
	int cap;
};

static void bytebuffer_reserve(struct bytebuffer *b, int cap) {
	if (b->cap >= cap) {
		return;
	}

	// prefer doubling capacity
	if (b->cap * 2 >= cap) {
		cap = b->cap * 2;
	}

	char *newbuf = malloc(cap);
	if (b->len > 0) {
		// copy what was there, b->len > 0 assumes b->buf != null
		memcpy(newbuf, b->buf, b->len);
	}
	if (b->buf) {
		// in case there was an allocated buffer, free it
		free(b->buf);
	}
	b->buf = newbuf;
	b->cap = cap;
}

static void bytebuffer_init(struct bytebuffer *b, int cap) {
	b->cap = 0;
	b->len = 0;
	b->buf = 0;

	if (cap > 0) {
		b->cap = cap;
		b->buf = malloc(cap); // just assume malloc works always
	}
}

static void bytebuffer_free(struct bytebuffer *b) {
	if (b->buf)
		free(b->buf);
}

static void bytebuffer_clear(struct bytebuffer *b) {
	b->len = 0;
}

static void bytebuffer_append(struct bytebuffer *b, const char *data, int len) {
	bytebuffer_reserve(b, b->len + len);
	memcpy(b->buf + b->len, data, len);
	b->len += len;
}

static void bytebuffer_puts(struct bytebuffer *b, const char *str) {
	bytebuffer_append(b, str, strlen(str));
}

static void bytebuffer_resize(struct bytebuffer *b, int len) {
	bytebuffer_reserve(b, len);
	b->len = len;
}

static void bytebuffer_flush(struct bytebuffer *b, int fd) {
	write(fd, b->buf, b->len);
	bytebuffer_clear(b);
}

static void bytebuffer_truncate(struct bytebuffer *b, int n) {
	if (n <= 0)
		return;
	if (n > b->len)
		n = b->len;
	const int nmove = b->len - n;
	memmove(b->buf, b->buf+n, nmove);
	b->len -= n;
}

enum {
	T_ENTER_CA,
	T_EXIT_CA,
	T_SHOW_CURSOR,
	T_HIDE_CURSOR,
	T_CLEAR_SCREEN,
	T_SGR0,
	T_UNDERLINE,
	T_BOLD,
	T_BLINK,
	T_REVERSE,
	T_ENTER_KEYPAD,
	T_EXIT_KEYPAD,
	T_FUNCS_NUM,
};

#define EUNSUPPORTED_TERM -1

// rxvt-256color
static const char *rxvt_256color_keys[] = {
	"\033[11~","\033[12~","\033[13~","\033[14~","\033[15~","\033[17~","\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~","\033[2~","\033[3~","\033[7~","\033[8~","\033[5~","\033[6~","\033[A","\033[B","\033[D","\033[C", 0
};
static const char *rxvt_256color_funcs[] = {
	"\0337\033[?47h", "\033[2J\033[?47l\0338", "\033[?25h", "\033[?25l", "\033[H\033[2J", "\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "\033=", "\033>",
};

// Eterm
static const char *eterm_keys[] = {
	"\033[11~","\033[12~","\033[13~","\033[14~","\033[15~","\033[17~","\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~","\033[2~","\033[3~","\033[7~","\033[8~","\033[5~","\033[6~","\033[A","\033[B","\033[D","\033[C", 0
};
static const char *eterm_funcs[] = {
	"\0337\033[?47h", "\033[2J\033[?47l\0338", "\033[?25h", "\033[?25l", "\033[H\033[2J", "\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "", "",
};

// screen
static const char *screen_keys[] = {
	"\033OP","\033OQ","\033OR","\033OS","\033[15~","\033[17~","\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~","\033[2~","\033[3~","\033[1~","\033[4~","\033[5~","\033[6~","\033OA","\033OB","\033OD","\033OC", 0
};
static const char *screen_funcs[] = {
	"\033[?1049h", "\033[?1049l", "\033[34h\033[?25h", "\033[?25l", "\033[H\033[J", "\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "\033[?1h\033=", "\033[?1l\033>",
};

// rxvt-unicode
static const char *rxvt_unicode_keys[] = {
	"\033[11~","\033[12~","\033[13~","\033[14~","\033[15~","\033[17~","\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~","\033[2~","\033[3~","\033[7~","\033[8~","\033[5~","\033[6~","\033[A","\033[B","\033[D","\033[C", 0
};
static const char *rxvt_unicode_funcs[] = {
	"\033[?1049h", "\033[r\033[?1049l", "\033[?25h", "\033[?25l", "\033[H\033[2J", "\033[m\033(B", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "\033=", "\033>",
};

// linux
static const char *linux_keys[] = {
	"\033[[A","\033[[B","\033[[C","\033[[D","\033[[E","\033[17~","\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~","\033[2~","\033[3~","\033[1~","\033[4~","\033[5~","\033[6~","\033[A","\033[B","\033[D","\033[C", 0
};
static const char *linux_funcs[] = {
	"", "", "\033[?25h\033[?0c", "\033[?25l\033[?1c", "\033[H\033[J", "\033[0;10m", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "", "",
};

// xterm
static const char *xterm_keys[] = {
	"\033OP","\033OQ","\033OR","\033OS","\033[15~","\033[17~","\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~","\033[2~","\033[3~","\033OH","\033OF","\033[5~","\033[6~","\033OA","\033OB","\033OD","\033OC", 0
};
static const char *xterm_funcs[] = {
	"\033[?1049h", "\033[?1049l", "\033[?12l\033[?25h", "\033[?25l", "\033[H\033[2J", "\033(B\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "\033[?1h\033=", "\033[?1l\033>",
};

static struct term {
	const char *name;
	const char **keys;
	const char **funcs;
} terms[] = {
	{"rxvt-256color", rxvt_256color_keys, rxvt_256color_funcs},
	{"Eterm", eterm_keys, eterm_funcs},
	{"screen", screen_keys, screen_funcs},
	{"rxvt-unicode", rxvt_unicode_keys, rxvt_unicode_funcs},
	{"linux", linux_keys, linux_funcs},
	{"xterm", xterm_keys, xterm_funcs},
	{0, 0, 0},
};

static bool init_from_terminfo = false;
static const char **keys;
static const char **funcs;

static int try_compatible(const char *term, const char *name,
			  const char **tkeys, const char **tfuncs)
{
	if (strstr(term, name)) {
		keys = tkeys;
		funcs = tfuncs;
		return 0;
	}

	return EUNSUPPORTED_TERM;
}

static int init_term_builtin(void)
{
	int i;
	const char *term = getenv("TERM");

	if (term) {
		for (i = 0; terms[i].name; i++) {
			if (!strcmp(terms[i].name, term)) {
				keys = terms[i].keys;
				funcs = terms[i].funcs;
				return 0;
			}
		}

		/* let's do some heuristic, maybe it's a compatible terminal */
		if (try_compatible(term, "xterm", xterm_keys, xterm_funcs) == 0)
			return 0;
		if (try_compatible(term, "rxvt", rxvt_unicode_keys, rxvt_unicode_funcs) == 0)
			return 0;
		if (try_compatible(term, "linux", linux_keys, linux_funcs) == 0)
			return 0;
		if (try_compatible(term, "Eterm", eterm_keys, eterm_funcs) == 0)
			return 0;
		if (try_compatible(term, "screen", screen_keys, screen_funcs) == 0)
			return 0;
		/* let's assume that 'cygwin' is xterm compatible */
		if (try_compatible(term, "cygwin", xterm_keys, xterm_funcs) == 0)
			return 0;
	}

	return EUNSUPPORTED_TERM;
}

//----------------------------------------------------------------------
// terminfo
//----------------------------------------------------------------------

static char *read_file(const char *file) {
	FILE *f = fopen(file, "rb");
	if (!f)
		return 0;

	struct stat st;
	if (fstat(fileno(f), &st) != 0) {
		fclose(f);
		return 0;
	}

	char *data = malloc(st.st_size);
	if (!data) {
		fclose(f);
		return 0;
	}

	if (fread(data, 1, st.st_size, f) != (size_t)st.st_size) {
		fclose(f);
		free(data);
		return 0;
	}

	fclose(f);
	return data;
}

static char *terminfo_try_path(const char *path, const char *term) {
	char tmp[4096];
	snprintf(tmp, sizeof(tmp), "%s/%c/%s", path, term[0], term);
	tmp[sizeof(tmp)-1] = '\0';
	char *data = read_file(tmp);
	if (data) {
		return data;
	}

	// fallback to darwin specific dirs structure
	snprintf(tmp, sizeof(tmp), "%s/%x/%s", path, term[0], term);
	tmp[sizeof(tmp)-1] = '\0';
	return read_file(tmp);
}

static char *load_terminfo(void) {
	char tmp[4096];
	const char *term = getenv("TERM");
	if (!term) {
		return 0;
	}

	// if TERMINFO is set, no other directory should be searched
	const char *terminfo = getenv("TERMINFO");
	if (terminfo) {
		return terminfo_try_path(terminfo, term);
	}

	// next, consider ~/.terminfo
	const char *home = getenv("HOME");
	if (home) {
		snprintf(tmp, sizeof(tmp), "%s/.terminfo", home);
		tmp[sizeof(tmp)-1] = '\0';
		char *data = terminfo_try_path(tmp, term);
		if (data)
			return data;
	}

	// next, TERMINFO_DIRS
	const char *dirs = getenv("TERMINFO_DIRS");
	if (dirs) {
		snprintf(tmp, sizeof(tmp), "%s", dirs);
		tmp[sizeof(tmp)-1] = '\0';
		char *dir = strtok(tmp, ":");
		while (dir) {
			const char *cdir = dir;
			if (strcmp(cdir, "") == 0) {
				cdir = "/usr/share/terminfo";
			}
			char *data = terminfo_try_path(cdir, term);
			if (data)
				return data;
			dir = strtok(0, ":");
		}
	}

	// fallback to /usr/share/terminfo
	return terminfo_try_path("/usr/share/terminfo", term);
}

#define TI_MAGIC 0432
#define TI_HEADER_LENGTH 12
#define TB_KEYS_NUM 22

static const char *terminfo_copy_string(char *data, int str, int table) {
	const int16_t off = *(int16_t*)(data + str);
	const char *src = data + table + off;
	int len = strlen(src);
	char *dst = malloc(len+1);
	strcpy(dst, src);
	return dst;
}

static const int16_t ti_funcs[] = {
	28, 40, 16, 13, 5, 39, 36, 27, 26, 34, 89, 88,
};

static const int16_t ti_keys[] = {
	66, 68 /* apparently not a typo; 67 is F10 for whatever reason */, 69,
	70, 71, 72, 73, 74, 75, 67, 216, 217, 77, 59, 76, 164, 82, 81, 87, 61,
	79, 83,
};

static int init_term(void) {
	int i;
	char *data = load_terminfo();
	if (!data) {
		init_from_terminfo = false;
		return init_term_builtin();
	}

	int16_t *header = (int16_t*)data;
	if ((header[1] + header[2]) % 2) {
		// old quirk to align everything on word boundaries
		header[2] += 1;
	}

	const int str_offset = TI_HEADER_LENGTH +
		header[1] + header[2] +	2 * header[3];
	const int table_offset = str_offset + 2 * header[4];

	keys = malloc(sizeof(const char*) * (TB_KEYS_NUM+1));
	for (i = 0; i < TB_KEYS_NUM; i++) {
		keys[i] = terminfo_copy_string(data,
			str_offset + 2 * ti_keys[i], table_offset);
	}
	keys[TB_KEYS_NUM] = 0;

	funcs = malloc(sizeof(const char*) * T_FUNCS_NUM);
	for (i = 0; i < T_FUNCS_NUM; i++) {
		funcs[i] = terminfo_copy_string(data,
			str_offset + 2 * ti_funcs[i], table_offset);
	}

	init_from_terminfo = true;
	return 0;
}

static void shutdown_term(void) {
	if (init_from_terminfo) {
		int i;
		for (i = 0; i < TB_KEYS_NUM; i++) {
			free((void*)keys[i]);
		}
		for (i = 0; i < T_FUNCS_NUM; i++) {
			free((void*)funcs[i]);
		}
		free(keys);
		free(funcs);
	}
}

// if s1 starts with s2 returns true, else false
// len is the length of s1
// s2 should be null-terminated
static bool starts_with(const char *s1, int len, const char *s2)
{
	int n = 0;
	while (*s2 && n < len) {
		if (*s1++ != *s2++)
			return false;
		n++;
	}
	return *s2 == 0;
}

// convert escape sequence to event, and return consumed bytes on success (failure == 0)
static int parse_escape_seq(struct tb_event *event, const char *buf, int len)
{
	// it's pretty simple here, find 'starts_with' match and return
	// success, else return failure
	int i;
	for (i = 0; keys[i]; i++) {
		if (starts_with(buf, len, keys[i])) {
			event->ch = 0;
			event->key = 0xFFFF-i;
			return strlen(keys[i]);
		}
	}
	return 0;
}

static bool extract_event(struct tb_event *event, struct bytebuffer *inbuf, int inputmode)
{
	const char *buf = inbuf->buf;
	const int len = inbuf->len;
	if (len == 0)
		return false;

	if (buf[0] == '\033') {
		int n = parse_escape_seq(event, buf, len);
		if (n) {
			bytebuffer_truncate(inbuf, n);
			return true;
		} else {
			// it's not escape sequence, then it's ALT or ESC,
			// check inputmode
			switch (inputmode) {
			case TB_INPUT_ESC:
				// if we're in escape mode, fill ESC event, pop
				// buffer, return success
				event->ch = 0;
				event->key = TB_KEY_ESC;
				event->mod = 0;
				bytebuffer_truncate(inbuf, 1);
				return true;
			case TB_INPUT_ALT:
				// if we're in alt mode, set ALT modifier to
				// event and redo parsing
				event->mod = TB_MOD_ALT;
				bytebuffer_truncate(inbuf, 1);
				return extract_event(event, inbuf, inputmode);
			default:
				assert(!"never got here");
				break;
			}
		}
	}

	// if we're here, this is not an escape sequence and not an alt sequence
	// so, it's a FUNCTIONAL KEY or a UNICODE character

	// first of all check if it's a functional key
	if ((unsigned char)buf[0] <= TB_KEY_SPACE ||
	    (unsigned char)buf[0] == TB_KEY_BACKSPACE2)
	{
		// fill event, pop buffer, return success */
		event->ch = 0;
		event->key = (uint16_t)buf[0];
		bytebuffer_truncate(inbuf, 1);
		return true;
	}

	// feh... we got utf8 here

	// check if there is all bytes
	if (len >= tb_utf8_char_length(buf[0])) {
		/* everything ok, fill event, pop buffer, return success */
		tb_utf8_char_to_unicode(&event->ch, buf);
		event->key = 0;
		bytebuffer_truncate(inbuf, tb_utf8_char_length(buf[0]));
		return true;
	}

	// event isn't recognized, perhaps there is not enough bytes in utf8
	// sequence
	return false;
}

struct cellbuf {
	int width;
	int height;
	struct tb_cell *cells;
};

#define CELL(buf, x, y) (buf)->cells[(y) * (buf)->width + (x)]
#define IS_CURSOR_HIDDEN(cx, cy) (cx == -1 || cy == -1)
#define LAST_COORD_INIT -1

static struct termios orig_tios;

static struct cellbuf back_buffer;
static struct cellbuf front_buffer;
static struct bytebuffer output_buffer;
static struct bytebuffer input_buffer;

static int termw;
static int termh;

static int inputmode = TB_INPUT_ESC;
static int outputmode = TB_OUTPUT_NORMAL;

static int inout;
static int winch_fds[2];

static int lastx = LAST_COORD_INIT;
static int lasty = LAST_COORD_INIT;
static int cursor_x = -1;
static int cursor_y = -1;

static uint16_t background = TB_DEFAULT;
static uint16_t foreground = TB_DEFAULT;

static void write_cursor(int x, int y);
static void write_sgr_fg(uint16_t fg);
static void write_sgr_bg(uint16_t bg);
static void write_sgr(uint16_t fg, uint16_t bg);

static void cellbuf_init(struct cellbuf *buf, int width, int height);
static void cellbuf_resize(struct cellbuf *buf, int width, int height);
static void cellbuf_clear(struct cellbuf *buf);
static void cellbuf_free(struct cellbuf *buf);

static void update_size(void);
static void update_term_size(void);
static void send_attr(uint16_t fg, uint16_t bg);
static void send_char(int x, int y, uint32_t c);
static void send_clear(void);
static void sigwinch_handler(int xxx);
static int wait_fill_event(struct tb_event *event, struct timeval *timeout);

/* may happen in a different thread */
static volatile int buffer_size_change_request;

/* -------------------------------------------------------- */

int tb_init(void)
{
	inout = open("/dev/tty", O_RDWR);
	if (inout == -1) {
		return TB_EFAILED_TO_OPEN_TTY;
	}

	if (init_term() < 0) {
		close(inout);
		return TB_EUNSUPPORTED_TERMINAL;
	}

	if (pipe(winch_fds) < 0) {
		close(inout);
		return TB_EPIPE_TRAP_ERROR;
	}

	struct sigaction sa;
	sa.sa_handler = sigwinch_handler;
	sa.sa_flags = 0;
	sigaction(SIGWINCH, &sa, 0);

	tcgetattr(inout, &orig_tios);

	struct termios tios;
	memcpy(&tios, &orig_tios, sizeof(tios));

	tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                           | INLCR | IGNCR | ICRNL | IXON);
	tios.c_oflag &= ~OPOST;
	tios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tios.c_cflag &= ~(CSIZE | PARENB);
	tios.c_cflag |= CS8;
	tios.c_cc[VMIN] = 0;
	tios.c_cc[VTIME] = 0;
	tcsetattr(inout, TCSAFLUSH, &tios);

	bytebuffer_init(&input_buffer, 128);
	bytebuffer_init(&output_buffer, 32 * 1024);

	bytebuffer_puts(&output_buffer, funcs[T_ENTER_CA]);
	bytebuffer_puts(&output_buffer, funcs[T_ENTER_KEYPAD]);
	bytebuffer_puts(&output_buffer, funcs[T_HIDE_CURSOR]);
	send_clear();

	update_term_size();
	cellbuf_init(&back_buffer, termw, termh);
	cellbuf_init(&front_buffer, termw, termh);
	cellbuf_clear(&back_buffer);
	cellbuf_clear(&front_buffer);

	return 0;
}

void tb_shutdown(void)
{
	bytebuffer_puts(&output_buffer, funcs[T_SHOW_CURSOR]);
	bytebuffer_puts(&output_buffer, funcs[T_SGR0]);
	bytebuffer_puts(&output_buffer, funcs[T_CLEAR_SCREEN]);
	bytebuffer_puts(&output_buffer, funcs[T_EXIT_CA]);
	bytebuffer_puts(&output_buffer, funcs[T_EXIT_KEYPAD]);
	bytebuffer_flush(&output_buffer, inout);
	tcsetattr(inout, TCSAFLUSH, &orig_tios);

	shutdown_term();
	close(inout);
	close(winch_fds[0]);
	close(winch_fds[1]);

	cellbuf_free(&back_buffer);
	cellbuf_free(&front_buffer);
	bytebuffer_free(&output_buffer);
	bytebuffer_free(&input_buffer);
}

void tb_present(void)
{
	int x,y;
	struct tb_cell *back, *front;

	/* invalidate cursor position */
	lastx = LAST_COORD_INIT;
	lasty = LAST_COORD_INIT;

	if (buffer_size_change_request) {
		update_size();
		buffer_size_change_request = 0;
	}

	for (y = 0; y < front_buffer.height; ++y) {
		for (x = 0; x < front_buffer.width; ++x) {
			back = &CELL(&back_buffer, x, y);
			front = &CELL(&front_buffer, x, y);
			if (memcmp(back, front, sizeof(struct tb_cell)) == 0)
				continue;
			send_attr(back->fg, back->bg);
			send_char(x, y, back->ch);
			memcpy(front, back, sizeof(struct tb_cell));
		}
	}
	if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
		write_cursor(cursor_x, cursor_y);
	bytebuffer_flush(&output_buffer, inout);
}

void tb_set_cursor(int cx, int cy)
{
	if (IS_CURSOR_HIDDEN(cursor_x, cursor_y) && !IS_CURSOR_HIDDEN(cx, cy))
		bytebuffer_puts(&output_buffer, funcs[T_SHOW_CURSOR]);

	if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y) && IS_CURSOR_HIDDEN(cx, cy))
		bytebuffer_puts(&output_buffer, funcs[T_HIDE_CURSOR]);

	cursor_x = cx;
	cursor_y = cy;
	if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
		write_cursor(cursor_x, cursor_y);
}

void tb_put_cell(int x, int y, const struct tb_cell *cell)
{
	if ((unsigned)x >= (unsigned)back_buffer.width)
		return;
	if ((unsigned)y >= (unsigned)back_buffer.height)
		return;
	CELL(&back_buffer, x, y) = *cell;
}

void tb_change_cell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg)
{
	struct tb_cell c = {ch, fg, bg};
	tb_put_cell(x, y, &c);
}

void tb_blit(int x, int y, int w, int h, const struct tb_cell *cells)
{
	if ((unsigned)(x+w) > (unsigned)back_buffer.width)
		return;
	if ((unsigned)(y+h) > (unsigned)back_buffer.height)
		return;

	int sy;
	struct tb_cell *dst = &CELL(&back_buffer, x, y);
	size_t size = sizeof(struct tb_cell) * w;

	for (sy = 0; sy < h; ++sy) {
		memcpy(dst, cells, size);
		dst += back_buffer.width;
		cells += w;
	}
}

int tb_poll_event(struct tb_event *event)
{
	return wait_fill_event(event, 0);
}

int tb_peek_event(struct tb_event *event, int timeout)
{
	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout - (tv.tv_sec * 1000)) * 1000;
	return wait_fill_event(event, &tv);
}

int tb_width(void)
{
	return termw;
}

int tb_height(void)
{
	return termh;
}

void tb_clear(void)
{
	if (buffer_size_change_request) {
		update_size();
		buffer_size_change_request = 0;
	}
	cellbuf_clear(&back_buffer);
}

int tb_select_input_mode(int mode)
{
	if (mode)
		inputmode = mode;
	return inputmode;
}

int tb_select_output_mode(int mode)
{
	if (mode)
		outputmode = mode;
	return outputmode;
}

void tb_set_clear_attributes(uint16_t fg, uint16_t bg)
{
	foreground = fg;
	background = bg;
}

/* -------------------------------------------------------- */

static int convertnum(uint32_t num, char* buf) {
	int i, l = 0;
	int ch;
	do {
		buf[l++] = '0' + (num % 10);
		num /= 10;
	} while (num);
	for(i = 0; i < l / 2; i++) {
		ch = buf[i];
		buf[i] = buf[l - 1 - i];
		buf[l - 1 - i] = ch;
	}
	return l;
}

#define WRITE_LITERAL(X) bytebuffer_append(&output_buffer, (X), sizeof(X)-1)
#define WRITE_INT(X) bytebuffer_append(&output_buffer, buf, convertnum((X), buf))

static void write_cursor(int x, int y) {
	char buf[32];
	WRITE_LITERAL("\033[");
	WRITE_INT(y+1);
	WRITE_LITERAL(";");
	WRITE_INT(x+1);
	WRITE_LITERAL("H");
}

// can only be called in NORMAL output mode
static void write_sgr_fg(uint16_t fg) {
	char buf[32];

	WRITE_LITERAL("\033[3");
	WRITE_INT(fg-1);
	WRITE_LITERAL("m");
}

// can only be called in NORMAL output mode
static void write_sgr_bg(uint16_t bg) {
	char buf[32];

	WRITE_LITERAL("\033[4");
	WRITE_INT(bg-1);
	WRITE_LITERAL("m");
}

static void write_sgr(uint16_t fg, uint16_t bg) {
	char buf[32];

	switch (outputmode) {
	case TB_OUTPUT_256:
	case TB_OUTPUT_216:
	case TB_OUTPUT_GRAYSCALE:
		WRITE_LITERAL("\033[38;5;");
		WRITE_INT(fg);
		WRITE_LITERAL("m");
		WRITE_LITERAL("\033[48;5;");
		WRITE_INT(bg);
		WRITE_LITERAL("m");
		break;
	case TB_OUTPUT_NORMAL:
	default:
		WRITE_LITERAL("\033[3");
		WRITE_INT(fg-1);
		WRITE_LITERAL(";4");
		WRITE_INT(bg-1);
		WRITE_LITERAL("m");
	}
}

static void cellbuf_init(struct cellbuf *buf, int width, int height)
{
	buf->cells = (struct tb_cell*)malloc(sizeof(struct tb_cell) * width * height);
	assert(buf->cells);
	buf->width = width;
	buf->height = height;
}

static void cellbuf_resize(struct cellbuf *buf, int width, int height)
{
	if (buf->width == width && buf->height == height)
		return;

	int oldw = buf->width;
	int oldh = buf->height;
	struct tb_cell *oldcells = buf->cells;

	cellbuf_init(buf, width, height);
	cellbuf_clear(buf);

	int minw = (width < oldw) ? width : oldw;
	int minh = (height < oldh) ? height : oldh;
	int i;

	for (i = 0; i < minh; ++i) {
		struct tb_cell *csrc = oldcells + (i * oldw);
		struct tb_cell *cdst = buf->cells + (i * width);
		memcpy(cdst, csrc, sizeof(struct tb_cell) * minw);
	}

	free(oldcells);
}

static void cellbuf_clear(struct cellbuf *buf)
{
	int i;
	int ncells = buf->width * buf->height;

	for (i = 0; i < ncells; ++i) {
		buf->cells[i].ch = ' ';
		buf->cells[i].fg = foreground;
		buf->cells[i].bg = background;
	}
}

static void cellbuf_free(struct cellbuf *buf)
{
	free(buf->cells);
}

static void get_term_size(int *w, int *h)
{
	struct winsize sz;
	memset(&sz, 0, sizeof(sz));

	ioctl(inout, TIOCGWINSZ, &sz);

	if (w) *w = sz.ws_col;
	if (h) *h = sz.ws_row;
}

static void update_term_size(void)
{
	struct winsize sz;
	memset(&sz, 0, sizeof(sz));

	ioctl(inout, TIOCGWINSZ, &sz);

	termw = sz.ws_col;
	termh = sz.ws_row;
}

static void send_attr(uint16_t fg, uint16_t bg)
{
#define LAST_ATTR_INIT 0xFFFF
	static uint16_t lastfg = LAST_ATTR_INIT, lastbg = LAST_ATTR_INIT;
	if (fg != lastfg || bg != lastbg) {
		bytebuffer_puts(&output_buffer, funcs[T_SGR0]);

		uint16_t fgcol;
		uint16_t bgcol;

		switch (outputmode) {
		case TB_OUTPUT_256:
			fgcol = fg & 0xFF;
			bgcol = bg & 0xFF;
			break;

		case TB_OUTPUT_216:
			fgcol = fg & 0xFF; if(fgcol > 215) fgcol = 7;
			bgcol = bg & 0xFF; if(bgcol > 215) bgcol = 0;
			fgcol += 0x10;
			bgcol += 0x10;
			break;

		case TB_OUTPUT_GRAYSCALE:
			fgcol = fg & 0xFF; if(fgcol > 23) fg = 23;
			bgcol = bg & 0xFF; if(bgcol > 23) bg = 0;
			fgcol += 0xe8;
			bgcol += 0xe8;
			break;

		case TB_OUTPUT_NORMAL:
		default:
			fgcol = fg & 0x0F;
			bgcol = bg & 0x0F;
		}

		if (fg & TB_BOLD)
			bytebuffer_puts(&output_buffer, funcs[T_BOLD]);
		if (bg & TB_BOLD)
			bytebuffer_puts(&output_buffer, funcs[T_BLINK]);
		if (fg & TB_UNDERLINE)
			bytebuffer_puts(&output_buffer, funcs[T_UNDERLINE]);
		if ((fg & TB_REVERSE) || (bg & TB_REVERSE))
			bytebuffer_puts(&output_buffer, funcs[T_REVERSE]);

		switch (outputmode) {
		case TB_OUTPUT_256:
		case TB_OUTPUT_216:
		case TB_OUTPUT_GRAYSCALE:
			write_sgr(fgcol, bgcol);
			break;

		case TB_OUTPUT_NORMAL:
		default:
			if (fgcol != TB_DEFAULT) {
				if (bgcol != TB_DEFAULT)
					write_sgr(fgcol, bgcol);
				else
					write_sgr_fg(fgcol);
			} else if (bgcol != TB_DEFAULT) {
				write_sgr_bg(bgcol);
			}
		}

		lastfg = fg;
		lastbg = bg;
	}
}

static void send_char(int x, int y, uint32_t c)
{
	char buf[7];
	int bw = tb_utf8_unicode_to_char(buf, c);
	buf[bw] = '\0';
	if (x-1 != lastx || y != lasty)
		write_cursor(x, y);
	lastx = x; lasty = y;
	if(!c) buf[0] = ' '; // replace 0 with whitespace
	bytebuffer_puts(&output_buffer, buf);
}

static void send_clear(void)
{
	send_attr(foreground, background);
	bytebuffer_puts(&output_buffer, funcs[T_CLEAR_SCREEN]);
	if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
		write_cursor(cursor_x, cursor_y);
	bytebuffer_flush(&output_buffer, inout);

	/* we need to invalidate cursor position too and these two vars are
	 * used only for simple cursor positioning optimization, cursor
	 * actually may be in the correct place, but we simply discard
	 * optimization once and it gives us simple solution for the case when
	 * cursor moved */
	lastx = LAST_COORD_INIT;
	lasty = LAST_COORD_INIT;
}

static void sigwinch_handler(int xxx)
{
	(void) xxx;
	const int zzz = 1;
	write(winch_fds[1], &zzz, sizeof(int));
}

static void update_size(void)
{
	update_term_size();
	cellbuf_resize(&back_buffer, termw, termh);
	cellbuf_resize(&front_buffer, termw, termh);
	cellbuf_clear(&front_buffer);
	send_clear();
}

static int wait_fill_event(struct tb_event *event, struct timeval *timeout)
{
	// ;-)
#define ENOUGH_DATA_FOR_PARSING 64
	fd_set events;
	memset(event, 0, sizeof(struct tb_event));

	// try to extract event from input buffer, return on success
	event->type = TB_EVENT_KEY;
	if (extract_event(event, &input_buffer, inputmode))
		return TB_EVENT_KEY;

	// it looks like input buffer is incomplete, let's try the short path,
	// but first make sure there is enough space
	int prevlen = input_buffer.len;
	bytebuffer_resize(&input_buffer, prevlen + ENOUGH_DATA_FOR_PARSING);
	ssize_t r = read(inout,	input_buffer.buf + prevlen,
		ENOUGH_DATA_FOR_PARSING);
	if (r < 0) {
		// EAGAIN / EWOULDBLOCK shouldn't occur here
		assert(errno != EAGAIN && errno != EWOULDBLOCK);
		return -1;
	} else if (r > 0) {
		bytebuffer_resize(&input_buffer, prevlen + r);
		if (extract_event(event, &input_buffer, inputmode))
			return TB_EVENT_KEY;
	} else {
		bytebuffer_resize(&input_buffer, prevlen);
	}

	// r == 0, or not enough data, let's go to select
	while (1) {
		FD_ZERO(&events);
		FD_SET(inout, &events);
		FD_SET(winch_fds[0], &events);
		int maxfd = (winch_fds[0] > inout) ? winch_fds[0] : inout;
		int result = select(maxfd+1, &events, 0, 0, timeout);
		if (!result)
			return 0;

		if (FD_ISSET(inout, &events)) {
			event->type = TB_EVENT_KEY;
			prevlen = input_buffer.len;
			bytebuffer_resize(&input_buffer,
				prevlen + ENOUGH_DATA_FOR_PARSING);
			r = read(inout, input_buffer.buf + prevlen,
				ENOUGH_DATA_FOR_PARSING);
			if (r < 0) {
				// EAGAIN / EWOULDBLOCK shouldn't occur here
				assert(errno != EAGAIN && errno != EWOULDBLOCK);
				return -1;
			}
			bytebuffer_resize(&input_buffer, prevlen + r);
			if (r == 0)
				continue;
			if (extract_event(event, &input_buffer, inputmode))
				return TB_EVENT_KEY;
		}
		if (FD_ISSET(winch_fds[0], &events)) {
			event->type = TB_EVENT_RESIZE;
			int zzz = 0;
			read(winch_fds[0], &zzz, sizeof(int));
			buffer_size_change_request = 1;
			get_term_size(&event->w, &event->h);
			return TB_EVENT_RESIZE;
		}
	}
}




static const unsigned char utf8_length[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

static const unsigned char utf8_mask[6] = {
	0x7F,
	0x1F,
	0x0F,
	0x07,
	0x03,
	0x01
};

int tb_utf8_char_length(char c)
{
	return utf8_length[(unsigned char)c];
}

int tb_utf8_char_to_unicode(uint32_t *out, const char *c)
{
	if (*c == 0)
		return TB_EOF;

	int i;
	unsigned char len = tb_utf8_char_length(*c);
	unsigned char mask = utf8_mask[len-1];
	uint32_t result = c[0] & mask;
	for (i = 1; i < len; ++i) {
		result <<= 6;
		result |= c[i] & 0x3f;
	}

	*out = result;
	return (int)len;
}

int tb_utf8_unicode_to_char(char *out, uint32_t c)
{
	int len = 0;
	int first;
	int i;

	if (c < 0x80) {
		first = 0;
		len = 1;
	} else if (c < 0x800) {
		first = 0xc0;
		len = 2;
	} else if (c < 0x10000) {
		first = 0xe0;
		len = 3;
	} else if (c < 0x200000) {
		first = 0xf0;
		len = 4;
	} else if (c < 0x4000000) {
		first = 0xf8;
		len = 5;
	} else {
		first = 0xfc;
		len = 6;
	}

	for (i = len - 1; i > 0; --i) {
		out[i] = (c & 0x3f) | 0x80;
		c >>= 6;
	}
	out[0] = c | first;

	return len;
}










/* struct key { */
/* 	unsigned char x; */
/* 	unsigned char y; */
/* 	uint32_t ch; */
/* }; */

/* #define STOP {0,0,0} */
/* struct key K_ESC[] = {{1,1,'E'},{2,1,'S'},{3,1,'C'},STOP}; */
/* struct key K_F1[] = {{6,1,'F'},{7,1,'1'},STOP}; */
/* struct key K_F2[] = {{9,1,'F'},{10,1,'2'},STOP}; */
/* struct key K_F3[] = {{12,1,'F'},{13,1,'3'},STOP}; */
/* struct key K_F4[] = {{15,1,'F'},{16,1,'4'},STOP}; */
/* struct key K_F5[] = {{19,1,'F'},{20,1,'5'},STOP}; */
/* struct key K_F6[] = {{22,1,'F'},{23,1,'6'},STOP}; */
/* struct key K_F7[] = {{25,1,'F'},{26,1,'7'},STOP}; */
/* struct key K_F8[] = {{28,1,'F'},{29,1,'8'},STOP}; */
/* struct key K_F9[] = {{33,1,'F'},{34,1,'9'},STOP}; */
/* struct key K_F10[] = {{36,1,'F'},{37,1,'1'},{38,1,'0'},STOP}; */
/* struct key K_F11[] = {{40,1,'F'},{41,1,'1'},{42,1,'1'},STOP}; */
/* struct key K_F12[] = {{44,1,'F'},{45,1,'1'},{46,1,'2'},STOP}; */
/* struct key K_PRN[] = {{50,1,'P'},{51,1,'R'},{52,1,'N'},STOP}; */
/* struct key K_SCR[] = {{54,1,'S'},{55,1,'C'},{56,1,'R'},STOP}; */
/* struct key K_BRK[] = {{58,1,'B'},{59,1,'R'},{60,1,'K'},STOP}; */
/* struct key K_LED1[] = {{66,1,'-'},STOP}; */
/* struct key K_LED2[] = {{70,1,'-'},STOP}; */
/* struct key K_LED3[] = {{74,1,'-'},STOP}; */

/* struct key K_TILDE[] = {{1,4,'`'},STOP}; */
/* struct key K_TILDE_SHIFT[] = {{1,4,'~'},STOP}; */
/* struct key K_1[] = {{4,4,'1'},STOP}; */
/* struct key K_1_SHIFT[] = {{4,4,'!'},STOP}; */
/* struct key K_2[] = {{7,4,'2'},STOP}; */
/* struct key K_2_SHIFT[] = {{7,4,'@'},STOP}; */
/* struct key K_3[] = {{10,4,'3'},STOP}; */
/* struct key K_3_SHIFT[] = {{10,4,'#'},STOP}; */
/* struct key K_4[] = {{13,4,'4'},STOP}; */
/* struct key K_4_SHIFT[] = {{13,4,'$'},STOP}; */
/* struct key K_5[] = {{16,4,'5'},STOP}; */
/* struct key K_5_SHIFT[] = {{16,4,'%'},STOP}; */
/* struct key K_6[] = {{19,4,'6'},STOP}; */
/* struct key K_6_SHIFT[] = {{19,4,'^'},STOP}; */
/* struct key K_7[] = {{22,4,'7'},STOP}; */
/* struct key K_7_SHIFT[] = {{22,4,'&'},STOP}; */
/* struct key K_8[] = {{25,4,'8'},STOP}; */
/* struct key K_8_SHIFT[] = {{25,4,'*'},STOP}; */
/* struct key K_9[] = {{28,4,'9'},STOP}; */
/* struct key K_9_SHIFT[] = {{28,4,'('},STOP}; */
/* struct key K_0[] = {{31,4,'0'},STOP}; */
/* struct key K_0_SHIFT[] = {{31,4,')'},STOP}; */
/* struct key K_MINUS[] = {{34,4,'-'},STOP}; */
/* struct key K_MINUS_SHIFT[] = {{34,4,'_'},STOP}; */
/* struct key K_EQUALS[] = {{37,4,'='},STOP}; */
/* struct key K_EQUALS_SHIFT[] = {{37,4,'+'},STOP}; */
/* struct key K_BACKSLASH[] = {{40,4,'\\'},STOP}; */
/* struct key K_BACKSLASH_SHIFT[] = {{40,4,'|'},STOP}; */
/* struct key K_BACKSPACE[] = {{44,4,0x2190},{45,4,0x2500},{46,4,0x2500},STOP}; */
/* struct key K_INS[] = {{50,4,'I'},{51,4,'N'},{52,4,'S'},STOP}; */
/* struct key K_HOM[] = {{54,4,'H'},{55,4,'O'},{56,4,'M'},STOP}; */
/* struct key K_PGU[] = {{58,4,'P'},{59,4,'G'},{60,4,'U'},STOP}; */
/* struct key K_K_NUMLOCK[] = {{65,4,'N'},STOP}; */
/* struct key K_K_SLASH[] = {{68,4,'/'},STOP}; */
/* struct key K_K_STAR[] = {{71,4,'*'},STOP}; */
/* struct key K_K_MINUS[] = {{74,4,'-'},STOP}; */

/* struct key K_TAB[] = {{1,6,'T'},{2,6,'A'},{3,6,'B'},STOP}; */
/* struct key K_q[] = {{6,6,'q'},STOP}; */
/* struct key K_Q[] = {{6,6,'Q'},STOP}; */
/* struct key K_w[] = {{9,6,'w'},STOP}; */
/* struct key K_W[] = {{9,6,'W'},STOP}; */
/* struct key K_e[] = {{12,6,'e'},STOP}; */
/* struct key K_E[] = {{12,6,'E'},STOP}; */
/* struct key K_r[] = {{15,6,'r'},STOP}; */
/* struct key K_R[] = {{15,6,'R'},STOP}; */
/* struct key K_t[] = {{18,6,'t'},STOP}; */
/* struct key K_T[] = {{18,6,'T'},STOP}; */
/* struct key K_y[] = {{21,6,'y'},STOP}; */
/* struct key K_Y[] = {{21,6,'Y'},STOP}; */
/* struct key K_u[] = {{24,6,'u'},STOP}; */
/* struct key K_U[] = {{24,6,'U'},STOP}; */
/* struct key K_i[] = {{27,6,'i'},STOP}; */
/* struct key K_I[] = {{27,6,'I'},STOP}; */
/* struct key K_o[] = {{30,6,'o'},STOP}; */
/* struct key K_O[] = {{30,6,'O'},STOP}; */
/* struct key K_p[] = {{33,6,'p'},STOP}; */
/* struct key K_P[] = {{33,6,'P'},STOP}; */
/* struct key K_LSQB[] = {{36,6,'['},STOP}; */
/* struct key K_LCUB[] = {{36,6,'{'},STOP}; */
/* struct key K_RSQB[] = {{39,6,']'},STOP}; */
/* struct key K_RCUB[] = {{39,6,'}'},STOP}; */
/* struct key K_ENTER[] = { */
/* 	{43,6,0x2591},{44,6,0x2591},{45,6,0x2591},{46,6,0x2591}, */
/* 	{43,7,0x2591},{44,7,0x2591},{45,7,0x21B5},{46,7,0x2591}, */
/* 	{41,8,0x2591},{42,8,0x2591},{43,8,0x2591},{44,8,0x2591}, */
/* 	{45,8,0x2591},{46,8,0x2591},STOP */
/* }; */
/* struct key K_DEL[] = {{50,6,'D'},{51,6,'E'},{52,6,'L'},STOP}; */
/* struct key K_END[] = {{54,6,'E'},{55,6,'N'},{56,6,'D'},STOP}; */
/* struct key K_PGD[] = {{58,6,'P'},{59,6,'G'},{60,6,'D'},STOP}; */
/* struct key K_K_7[] = {{65,6,'7'},STOP}; */
/* struct key K_K_8[] = {{68,6,'8'},STOP}; */
/* struct key K_K_9[] = {{71,6,'9'},STOP}; */
/* struct key K_K_PLUS[] = {{74,6,' '},{74,7,'+'},{74,8,' '},STOP}; */

/* struct key K_CAPS[] = {{1,8,'C'},{2,8,'A'},{3,8,'P'},{4,8,'S'},STOP}; */
/* struct key K_a[] = {{7,8,'a'},STOP}; */
/* struct key K_A[] = {{7,8,'A'},STOP}; */
/* struct key K_s[] = {{10,8,'s'},STOP}; */
/* struct key K_S[] = {{10,8,'S'},STOP}; */
/* struct key K_d[] = {{13,8,'d'},STOP}; */
/* struct key K_D[] = {{13,8,'D'},STOP}; */
/* struct key K_f[] = {{16,8,'f'},STOP}; */
/* struct key K_F[] = {{16,8,'F'},STOP}; */
/* struct key K_g[] = {{19,8,'g'},STOP}; */
/* struct key K_G[] = {{19,8,'G'},STOP}; */
/* struct key K_h[] = {{22,8,'h'},STOP}; */
/* struct key K_H[] = {{22,8,'H'},STOP}; */
/* struct key K_j[] = {{25,8,'j'},STOP}; */
/* struct key K_J[] = {{25,8,'J'},STOP}; */
/* struct key K_k[] = {{28,8,'k'},STOP}; */
/* struct key K_K[] = {{28,8,'K'},STOP}; */
/* struct key K_l[] = {{31,8,'l'},STOP}; */
/* struct key K_L[] = {{31,8,'L'},STOP}; */
/* struct key K_SEMICOLON[] = {{34,8,';'},STOP}; */
/* struct key K_PARENTHESIS[] = {{34,8,':'},STOP}; */
/* struct key K_QUOTE[] = {{37,8,'\''},STOP}; */
/* struct key K_DOUBLEQUOTE[] = {{37,8,'"'},STOP}; */
/* struct key K_K_4[] = {{65,8,'4'},STOP}; */
/* struct key K_K_5[] = {{68,8,'5'},STOP}; */
/* struct key K_K_6[] = {{71,8,'6'},STOP}; */

/* struct key K_LSHIFT[] = {{1,10,'S'},{2,10,'H'},{3,10,'I'},{4,10,'F'},{5,10,'T'},STOP}; */
/* struct key K_z[] = {{9,10,'z'},STOP}; */
/* struct key K_Z[] = {{9,10,'Z'},STOP}; */
/* struct key K_x[] = {{12,10,'x'},STOP}; */
/* struct key K_X[] = {{12,10,'X'},STOP}; */
/* struct key K_c[] = {{15,10,'c'},STOP}; */
/* struct key K_C[] = {{15,10,'C'},STOP}; */
/* struct key K_v[] = {{18,10,'v'},STOP}; */
/* struct key K_V[] = {{18,10,'V'},STOP}; */
/* struct key K_b[] = {{21,10,'b'},STOP}; */
/* struct key K_B[] = {{21,10,'B'},STOP}; */
/* struct key K_n[] = {{24,10,'n'},STOP}; */
/* struct key K_N[] = {{24,10,'N'},STOP}; */
/* struct key K_m[] = {{27,10,'m'},STOP}; */
/* struct key K_M[] = {{27,10,'M'},STOP}; */
/* struct key K_COMMA[] = {{30,10,','},STOP}; */
/* struct key K_LANB[] = {{30,10,'<'},STOP}; */
/* struct key K_PERIOD[] = {{33,10,'.'},STOP}; */
/* struct key K_RANB[] = {{33,10,'>'},STOP}; */
/* struct key K_SLASH[] = {{36,10,'/'},STOP}; */
/* struct key K_QUESTION[] = {{36,10,'?'},STOP}; */
/* struct key K_RSHIFT[] = {{42,10,'S'},{43,10,'H'},{44,10,'I'},{45,10,'F'},{46,10,'T'},STOP}; */
/* struct key K_ARROW_UP[] = {{54,10,'('},{55,10,0x2191},{56,10,')'},STOP}; */
/* struct key K_K_1[] = {{65,10,'1'},STOP}; */
/* struct key K_K_2[] = {{68,10,'2'},STOP}; */
/* struct key K_K_3[] = {{71,10,'3'},STOP}; */
/* struct key K_K_ENTER[] = {{74,10,0x2591},{74,11,0x2591},{74,12,0x2591},STOP}; */

/* struct key K_LCTRL[] = {{1,12,'C'},{2,12,'T'},{3,12,'R'},{4,12,'L'},STOP}; */
/* struct key K_LWIN[] = {{6,12,'W'},{7,12,'I'},{8,12,'N'},STOP}; */
/* struct key K_LALT[] = {{10,12,'A'},{11,12,'L'},{12,12,'T'},STOP}; */
/* struct key K_SPACE[] = { */
/* 	{14,12,' '},{15,12,' '},{16,12,' '},{17,12,' '},{18,12,' '}, */
/* 	{19,12,'S'},{20,12,'P'},{21,12,'A'},{22,12,'C'},{23,12,'E'}, */
/* 	{24,12,' '},{25,12,' '},{26,12,' '},{27,12,' '},{28,12,' '}, */
/* 	STOP */
/* }; */
/* struct key K_RALT[] = {{30,12,'A'},{31,12,'L'},{32,12,'T'},STOP}; */
/* struct key K_RWIN[] = {{34,12,'W'},{35,12,'I'},{36,12,'N'},STOP}; */
/* struct key K_RPROP[] = {{38,12,'P'},{39,12,'R'},{40,12,'O'},{41,12,'P'},STOP}; */
/* struct key K_RCTRL[] = {{43,12,'C'},{44,12,'T'},{45,12,'R'},{46,12,'L'},STOP}; */
/* struct key K_ARROW_LEFT[] = {{50,12,'('},{51,12,0x2190},{52,12,')'},STOP}; */
/* struct key K_ARROW_DOWN[] = {{54,12,'('},{55,12,0x2193},{56,12,')'},STOP}; */
/* struct key K_ARROW_RIGHT[] = {{58,12,'('},{59,12,0x2192},{60,12,')'},STOP}; */
/* struct key K_K_0[] = {{65,12,' '},{66,12,'0'},{67,12,' '},{68,12,' '},STOP}; */
/* struct key K_K_PERIOD[] = {{71,12,'.'},STOP}; */

/* struct combo { */
/* 	struct key *keys[6]; */
/* }; */

/* struct combo combos[] = { */
/* 	{{K_TILDE, K_2, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_A, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_B, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_C, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_D, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_E, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_F, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_G, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_H, K_BACKSPACE, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_I, K_TAB, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_J, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_K, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_L, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_M, K_ENTER, K_K_ENTER, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_N, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_O, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_P, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_Q, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_R, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_S, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_T, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_U, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_V, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_W, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_X, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_Y, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_Z, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_LSQB, K_ESC, K_3, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_4, K_BACKSLASH, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_RSQB, K_5, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_6, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_7, K_SLASH, K_MINUS_SHIFT, K_LCTRL, K_RCTRL, 0}}, */
/* 	{{K_SPACE,0}}, */
/* 	{{K_1_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_DOUBLEQUOTE,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_3_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_4_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_5_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_7_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_QUOTE,0}}, */
/* 	{{K_9_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_0_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_8_SHIFT,K_K_STAR,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_EQUALS_SHIFT,K_K_PLUS,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_COMMA,0}}, */
/* 	{{K_MINUS,K_K_MINUS,0}}, */
/* 	{{K_PERIOD,K_K_PERIOD,0}}, */
/* 	{{K_SLASH,K_K_SLASH,0}}, */
/* 	{{K_0,K_K_0,0}}, */
/* 	{{K_1,K_K_1,0}}, */
/* 	{{K_2,K_K_2,0}}, */
/* 	{{K_3,K_K_3,0}}, */
/* 	{{K_4,K_K_4,0}}, */
/* 	{{K_5,K_K_5,0}}, */
/* 	{{K_6,K_K_6,0}}, */
/* 	{{K_7,K_K_7,0}}, */
/* 	{{K_8,K_K_8,0}}, */
/* 	{{K_9,K_K_9,0}}, */
/* 	{{K_PARENTHESIS,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_SEMICOLON,0}}, */
/* 	{{K_LANB,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_EQUALS,0}}, */
/* 	{{K_RANB,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_QUESTION,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_2_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_A,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_B,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_C,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_D,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_E,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_F,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_G,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_H,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_I,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_J,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_K,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_L,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_M,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_N,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_O,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_P,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_Q,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_R,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_S,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_T,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_U,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_V,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_W,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_X,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_Y,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_Z,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_LSQB,0}}, */
/* 	{{K_BACKSLASH,0}}, */
/* 	{{K_RSQB,0}}, */
/* 	{{K_6_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_MINUS_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_TILDE,0}}, */
/* 	{{K_a,0}}, */
/* 	{{K_b,0}}, */
/* 	{{K_c,0}}, */
/* 	{{K_d,0}}, */
/* 	{{K_e,0}}, */
/* 	{{K_f,0}}, */
/* 	{{K_g,0}}, */
/* 	{{K_h,0}}, */
/* 	{{K_i,0}}, */
/* 	{{K_j,0}}, */
/* 	{{K_k,0}}, */
/* 	{{K_l,0}}, */
/* 	{{K_m,0}}, */
/* 	{{K_n,0}}, */
/* 	{{K_o,0}}, */
/* 	{{K_p,0}}, */
/* 	{{K_q,0}}, */
/* 	{{K_r,0}}, */
/* 	{{K_s,0}}, */
/* 	{{K_t,0}}, */
/* 	{{K_u,0}}, */
/* 	{{K_v,0}}, */
/* 	{{K_w,0}}, */
/* 	{{K_x,0}}, */
/* 	{{K_y,0}}, */
/* 	{{K_z,0}}, */
/* 	{{K_LCUB,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_BACKSLASH_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_RCUB,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_TILDE_SHIFT,K_LSHIFT,K_RSHIFT,0}}, */
/* 	{{K_8, K_BACKSPACE, K_LCTRL, K_RCTRL, 0}} */
/* }; */

/* struct combo func_combos[] = { */
/* 	{{K_F1,0}}, */
/* 	{{K_F2,0}}, */
/* 	{{K_F3,0}}, */
/* 	{{K_F4,0}}, */
/* 	{{K_F5,0}}, */
/* 	{{K_F6,0}}, */
/* 	{{K_F7,0}}, */
/* 	{{K_F8,0}}, */
/* 	{{K_F9,0}}, */
/* 	{{K_F10,0}}, */
/* 	{{K_F11,0}}, */
/* 	{{K_F12,0}}, */
/* 	{{K_INS,0}}, */
/* 	{{K_DEL,0}}, */
/* 	{{K_HOM,0}}, */
/* 	{{K_END,0}}, */
/* 	{{K_PGU,0}}, */
/* 	{{K_PGD,0}}, */
/* 	{{K_ARROW_UP,0}}, */
/* 	{{K_ARROW_DOWN,0}}, */
/* 	{{K_ARROW_LEFT,0}}, */
/* 	{{K_ARROW_RIGHT,0}} */
/* }; */

/* void print_tb(const char *str, int x, int y, uint16_t fg, uint16_t bg) */
/* { */
/* 	while (*str) { */
/* 		uint32_t uni; */
/* 		str += tb_utf8_char_to_unicode(&uni, str); */
/* 		tb_change_cell(x, y, uni, fg, bg); */
/* 		x++; */
/* 	} */
/* } */

/* void printf_tb(int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...) */
/* { */
/* 	char buf[4096]; */
/* 	va_list vl; */
/* 	va_start(vl, fmt); */
/* 	vsnprintf(buf, sizeof(buf), fmt, vl); */
/* 	va_end(vl); */
/* 	print_tb(buf, x, y, fg, bg); */
/* } */

/* void draw_key(struct key *k, uint16_t fg, uint16_t bg) */
/* { */
/* 	while (k->x) { */
/* 		tb_change_cell(k->x+2, k->y+4, k->ch, fg, bg); */
/* 		k++; */
/* 	} */
/* } */

/* void draw_keyboard() */
/* { */
/* 	int i; */
/* 	tb_change_cell(0, 0, 0x250C, TB_WHITE, TB_DEFAULT); */
/* 	tb_change_cell(79, 0, 0x2510, TB_WHITE, TB_DEFAULT); */
/* 	tb_change_cell(0, 23, 0x2514, TB_WHITE, TB_DEFAULT); */
/* 	tb_change_cell(79, 23, 0x2518, TB_WHITE, TB_DEFAULT); */

/* 	for (i = 1; i < 79; ++i) { */
/* 		tb_change_cell(i, 0, 0x2500, TB_WHITE, TB_DEFAULT); */
/* 		tb_change_cell(i, 23, 0x2500, TB_WHITE, TB_DEFAULT); */
/* 		tb_change_cell(i, 17, 0x2500, TB_WHITE, TB_DEFAULT); */
/* 		tb_change_cell(i, 4, 0x2500, TB_WHITE, TB_DEFAULT); */
/* 	} */
/* 	for (i = 1; i < 23; ++i) { */
/* 		tb_change_cell(0, i, 0x2502, TB_WHITE, TB_DEFAULT); */
/* 		tb_change_cell(79, i, 0x2502, TB_WHITE, TB_DEFAULT); */
/* 	} */
/* 	tb_change_cell(0, 17, 0x251C, TB_WHITE, TB_DEFAULT); */
/* 	tb_change_cell(79, 17, 0x2524, TB_WHITE, TB_DEFAULT); */
/* 	tb_change_cell(0, 4, 0x251C, TB_WHITE, TB_DEFAULT); */
/* 	tb_change_cell(79, 4, 0x2524, TB_WHITE, TB_DEFAULT); */
/* 	for (i = 5; i < 17; ++i) { */
/* 		tb_change_cell(1, i, 0x2588, TB_YELLOW, TB_YELLOW); */
/* 		tb_change_cell(78, i, 0x2588, TB_YELLOW, TB_YELLOW); */
/* 	} */

/* 	draw_key(K_ESC, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F1, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F2, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F3, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F4, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F5, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F6, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F7, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F8, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F9, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F10, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F11, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_F12, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_PRN, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_SCR, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_BRK, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_LED1, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_LED2, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_LED3, TB_WHITE, TB_BLUE); */

/* 	draw_key(K_TILDE, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_1, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_2, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_3, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_4, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_5, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_6, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_7, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_8, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_9, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_0, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_MINUS, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_EQUALS, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_BACKSLASH, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_BACKSPACE, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_INS, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_HOM, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_PGU, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_NUMLOCK, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_SLASH, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_STAR, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_MINUS, TB_WHITE, TB_BLUE); */

/* 	draw_key(K_TAB, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_q, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_w, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_e, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_r, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_t, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_y, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_u, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_i, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_o, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_p, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_LSQB, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_RSQB, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_ENTER, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_DEL, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_END, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_PGD, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_7, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_8, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_9, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_PLUS, TB_WHITE, TB_BLUE); */

/* 	draw_key(K_CAPS, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_a, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_s, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_d, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_f, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_g, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_h, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_j, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_k, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_l, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_SEMICOLON, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_QUOTE, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_4, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_5, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_6, TB_WHITE, TB_BLUE); */

/* 	draw_key(K_LSHIFT, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_z, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_x, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_c, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_v, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_b, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_n, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_m, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_COMMA, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_PERIOD, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_SLASH, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_RSHIFT, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_ARROW_UP, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_1, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_2, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_3, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_ENTER, TB_WHITE, TB_BLUE); */

/* 	draw_key(K_LCTRL, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_LWIN, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_LALT, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_SPACE, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_RCTRL, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_RPROP, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_RWIN, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_RALT, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_ARROW_LEFT, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_ARROW_DOWN, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_ARROW_RIGHT, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_0, TB_WHITE, TB_BLUE); */
/* 	draw_key(K_K_PERIOD, TB_WHITE, TB_BLUE); */

/* 	printf_tb(33, 1, TB_MAGENTA | TB_BOLD, TB_DEFAULT, "Keyboard demo!"); */
/* 	printf_tb(21, 2, TB_MAGENTA, TB_DEFAULT, "(press CTRL+X and then CTRL+Q to exit)"); */
/* 	printf_tb(15, 3, TB_MAGENTA, TB_DEFAULT, "(press CTRL+X and then CTRL+C to change input mode)"); */

/* 	static const char *inputmodemap[] = { */
/* 		0, */
/* 		"TB_INPUT_ESC", */
/* 		"TB_INPUT_ALT" */
/* 	}; */
/* 	printf_tb(3, 18, TB_WHITE, TB_DEFAULT, "Input mode: %s", */
/* 			inputmodemap[tb_select_input_mode(0)]); */
/* } */

/* const char *funckeymap(int k) */
/* { */
/* 	static const char *fcmap[] = { */
/* 		"CTRL+2, CTRL+~", */
/* 		"CTRL+A", */
/* 		"CTRL+B", */
/* 		"CTRL+C", */
/* 		"CTRL+D", */
/* 		"CTRL+E", */
/* 		"CTRL+F", */
/* 		"CTRL+G", */
/* 		"CTRL+H, BACKSPACE", */
/* 		"CTRL+I, TAB", */
/* 		"CTRL+J", */
/* 		"CTRL+K", */
/* 		"CTRL+L", */
/* 		"CTRL+M, ENTER", */
/* 		"CTRL+N", */
/* 		"CTRL+O", */
/* 		"CTRL+P", */
/* 		"CTRL+Q", */
/* 		"CTRL+R", */
/* 		"CTRL+S", */
/* 		"CTRL+T", */
/* 		"CTRL+U", */
/* 		"CTRL+V", */
/* 		"CTRL+W", */
/* 		"CTRL+X", */
/* 		"CTRL+Y", */
/* 		"CTRL+Z", */
/* 		"CTRL+3, ESC, CTRL+[", */
/* 		"CTRL+4, CTRL+\\", */
/* 		"CTRL+5, CTRL+]", */
/* 		"CTRL+6", */
/* 		"CTRL+7, CTRL+/, CTRL+_", */
/* 		"SPACE" */
/* 	}; */
/* 	static const char *fkmap[] = { */
/* 		"F1", */
/* 		"F2", */
/* 		"F3", */
/* 		"F4", */
/* 		"F5", */
/* 		"F6", */
/* 		"F7", */
/* 		"F8", */
/* 		"F9", */
/* 		"F10", */
/* 		"F11", */
/* 		"F12", */
/* 		"INSERT", */
/* 		"DELETE", */
/* 		"HOME", */
/* 		"END", */
/* 		"PGUP", */
/* 		"PGDN", */
/* 		"ARROW UP", */
/* 		"ARROW DOWN", */
/* 		"ARROW LEFT", */
/* 		"ARROW RIGHT" */
/* 	}; */

/* 	if (k == TB_KEY_CTRL_8) */
/* 		return "CTRL+8, BACKSPACE 2"; /\* 0x7F *\/ */
/* 	else if (k >= TB_KEY_ARROW_RIGHT && k <= 0xFFFF) */
/* 		return fkmap[0xFFFF-k]; */
/* 	else if (k <= TB_KEY_SPACE) */
/* 		return fcmap[k]; */
/* 	return "UNKNOWN"; */
/* } */

/* void pretty_print_press(struct tb_event *ev) */
/* { */
/* 	char buf[7]; */
/* 	buf[tb_utf8_unicode_to_char(buf, ev->ch)] = '\0'; */
/* 	printf_tb(3, 19, TB_WHITE , TB_DEFAULT, "Key: "); */
/* 	printf_tb(8, 19, TB_YELLOW, TB_DEFAULT, "decimal: %d", ev->key); */
/* 	printf_tb(8, 20, TB_GREEN , TB_DEFAULT, "hex:     0x%X", ev->key); */
/* 	printf_tb(8, 21, TB_CYAN  , TB_DEFAULT, "octal:   0%o", ev->key); */
/* 	printf_tb(8, 22, TB_RED   , TB_DEFAULT, "string:  %s", funckeymap(ev->key)); */

/* 	printf_tb(43, 19, TB_WHITE , TB_DEFAULT, "Char: "); */
/* 	printf_tb(49, 19, TB_YELLOW, TB_DEFAULT, "decimal: %d", ev->ch); */
/* 	printf_tb(49, 20, TB_GREEN , TB_DEFAULT, "hex:     0x%X", ev->ch); */
/* 	printf_tb(49, 21, TB_CYAN  , TB_DEFAULT, "octal:   0%o", ev->ch); */
/* 	printf_tb(49, 22, TB_RED   , TB_DEFAULT, "string:  %s", buf); */

/* 	printf_tb(43, 18, TB_WHITE, TB_DEFAULT, "Modifier: %s", */
/* 			(ev->mod) ? "TB_MOD_ALT" : "none"); */

/* } */

/* void pretty_print_resize(struct tb_event *ev) */
/* { */
/* 	printf_tb(3, 19, TB_WHITE, TB_DEFAULT, "Resize event: %d x %d", ev->w, ev->h); */
/* } */

/* void dispatch_press(struct tb_event *ev) */
/* { */
/* 	if (ev->mod & TB_MOD_ALT) { */
/* 		draw_key(K_LALT, TB_WHITE, TB_RED); */
/* 		draw_key(K_RALT, TB_WHITE, TB_RED); */
/* 	} */

/* 	struct combo *k = 0; */
/* 	if (ev->key >= TB_KEY_ARROW_RIGHT) */
/* 		k = &func_combos[0xFFFF-ev->key]; */
/* 	else if (ev->ch < 128) { */
/* 		if (ev->ch == 0 && ev->key < 128) */
/* 			k = &combos[ev->key]; */
/* 		else */
/* 			k = &combos[ev->ch]; */
/* 	} */
/* 	if (!k) */
/* 		return; */

/* 	struct key **keys = k->keys; */
/* 	while (*keys) { */
/* 		draw_key(*keys, TB_WHITE, TB_RED); */
/* 		keys++; */
/* 	} */
/* } */

int main(int argc, char **argv) {
  (void) argc; (void) argv;
  int ret;

  ret = tb_init();
  if (ret) {
    fprintf(stderr, "tb_init() failed with error code %d\n", ret);
    return 1;
  }

  tb_select_input_mode(TB_INPUT_ESC);
  struct tb_event ev;

  tb_clear();
  tb_change_cell(2, 4, 'a', TB_WHITE, TB_BLUE);
  /* draw_keyboard(); */
  tb_present();
  int ctrlxpressed = 0;

  while (tb_poll_event(&ev)) {
    switch (ev.type) {
    case TB_EVENT_KEY: {
      tb_shutdown();
      return 0;
      break;
    }
    case TB_EVENT_RESIZE:
      tb_clear();
      /* draw_keyboard(); */
      /* pretty_print_resize(&ev); */
      tb_present();
      break;
    default:
      break;
    }
  }
  tb_shutdown();
  return 0;
}
