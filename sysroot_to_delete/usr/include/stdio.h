#ifndef _STDIO_H
#define _STDIO_H

// key codes and the key_event_t type
#include <keyboard.h>


#define EOF (-1)

// ----------------------------------
// console / screen / tty
// ----------------------------------

// prints to current tty
int printf(char* format, ...);

// prints a message on the tty. no new line is added
int puts(const char *message);

// prints one char on the tty
int putchar(int c);

// clears the screen of the tty
int clear_screen();

// returns cursor information
int where_xy(int *x, int *y);

// positions cursor
int goto_xy(int x, int y);

// returns screen dimensions
int screen_dimensions(int *cols, int *rows);

// returns current screen color
int get_screen_color();

// sets screen color (bg = high nibble)
int set_screen_color(int color);


#define COLOR_BLACK           0
#define COLOR_BLUE            1
#define COLOR_GREEN           2
#define COLOR_CYAN            3
#define COLOR_RED             4
#define COLOR_MAGENTA         5
#define COLOR_BROWN           6
#define COLOR_LIGHT_GREY      7
#define COLOR_DARK_GREY       8
#define COLOR_LIGHT_BLUE      9
#define COLOR_LIGHT_GREEN    10
#define COLOR_LIGHT_CYAN     11
#define COLOR_LIGHT_RED      12
#define COLOR_LIGHT_MAGENTA  13
#define COLOR_LIGHT_BROWN    14
#define COLOR_WHITE          15

void getkey(key_event_t *event);



// ----------------------------------
// files
// ----------------------------------

enum seek_origin {
    SEEK_START,
    SEEK_CURRENT,
    SEEK_END
};

struct file_timestamp {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

typedef struct dir_entry {
    char short_name[12+1];
    uint32_t file_size;
    struct {
        uint8_t label: 1;
        uint8_t dir: 1;
        uint8_t read_only: 1;
    } flags;
    struct file_timestamp created;
    struct file_timestamp modified;
} dir_entry_t;


// dirent.type, also see kernel vfs.h
#define DE_TYPE_FILE  1
#define DE_TYPE_DIR   2

typedef struct dirent {
    char name[256];
    uint8_t  type; // see DE_TYPE_* defines
    uint32_t size;
    uint32_t location;
} dirent_t;


int getcwd(char *buffer, int size);
int chdir(char *path);

int open(char *name);
int read(int handle, char *buffer, int length);
int write(int handle, char *buffer, int length);
int seek(int handle, int offset, enum seek_origin origin);
int close(int handle);

int opendir(char *name);
int rewinddir(int handle);
dirent_t *readdir(int handle);
int closedir(int handle);

int touch(char *path);
int unlink(char *path);

int mkdir(char *path);
int rmdir(char *path);


#endif
