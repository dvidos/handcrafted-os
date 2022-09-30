#include <va_list.h>
#include <klib/string.h>
#include <klib/strbuff.h>
#include <drivers/serial.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <klog.h>
#include <multitask/process.h>


static char *level_captions[] = {
    "NONE ",
    "CRIT ",
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG",
    "TRACE"
};

static struct {
    char buffer[8096];
    int len;
} memlog;

tty_t *tty_appender;
// should have a file appender as well...

typedef struct appender_info {
    log_level_t level;
    bool have_dumped_memory;
} appender_info_t;

static appender_info_t appenders[LOGAPP_SIZE];


typedef struct module_level {
    char *module_name;
    log_level_t level;
} module_level_t;

// static array to avoid dynamic mem allocation
#define MODULE_LEVELS_COUNT  64
static module_level_t module_levels[MODULE_LEVELS_COUNT];
static log_level_t default_module_level;



static void memlog_write(char *str);
static void memory_log_append(const char *module_name, log_level_t level, char *str);
static void screen_log_append(const char *module_name, log_level_t level, char *str, bool decorated);
static void serial_log_append(const char *module_name, log_level_t level, char *str, bool decorated);
static void file_log_append(const char *module_name, log_level_t level, char *str, bool decorated);
static void tty_log_append(const char *module_name, log_level_t level, char *str, bool decorated);
static char printable(char c);

void init_klog() {
    memset(memlog.buffer, '-', sizeof(memlog.buffer));
    memlog.len = 0;

    memset(&appenders, 0, sizeof(appenders));

    default_module_level = LOGLEV_WARN;
    memset(&module_levels, 0, sizeof(module_levels));
}

void klog_appender_level(log_appender_t appender, log_level_t level) {
    bool prev_level = appenders[appender].level;
    appenders[appender].level = level;

    // this is also used to enable an appender, when they are available
    // maybe dump memory contents to this newly opened appender
    if (appender != LOGAPP_MEMORY &&
        prev_level == LOGLEV_NONE && 
        level > LOGLEV_NONE &&
        !appenders[appender].have_dumped_memory
    ) {
        if (appender == LOGAPP_SERIAL)
            serial_log_append(NULL, LOGLEV_INFO, memlog.buffer, false);
        else if (appender == LOGAPP_FILE)
            file_log_append(NULL, LOGLEV_INFO, memlog.buffer, false);
        else if (appender == LOGAPP_TTY && tty_appender != NULL)
            tty_log_append(NULL, LOGLEV_INFO, memlog.buffer, false);
        appenders[appender].have_dumped_memory = true;
    }
}

void klog_default_module_level(log_level_t level) {
    default_module_level = level;
}

void klog_module_level(char *module_name, log_level_t level) {
    
    // see if module exists, we can update
    for (int i = 0; i < MODULE_LEVELS_COUNT; i++) {
        if (module_levels[i].module_name == NULL)
            break;
        if (strcmp(module_levels[i].module_name, module_name) == 0) {
            module_levels[i].level = level;
            return;
        }
    }

    // if we are here, module not already defined, we have to add
    for (int i = 0; i < MODULE_LEVELS_COUNT; i++) {
        if (module_levels[i].module_name == NULL) {
            module_levels[i].module_name = module_name;
            module_levels[i].level = level;
            return;
        }
    }

    // if we gotten here, no free slot was found to add
    panic("Not enough slots to set module log level, please increase");
}

static log_level_t get_module_log_level(const char *module_name) {
    if (module_name == NULL)
        return default_module_level;
    
    for (int i = 0; i < MODULE_LEVELS_COUNT; i++) {
        if (module_levels[i].module_name == NULL)
            // we reached the end of the defined names
            return default_module_level;
        
        if (strcmp(module_levels[i].module_name, module_name) == 0)
            return module_levels[i].level;
    }

    return default_module_level;
}

void klog_set_tty(tty_t *tty) {
    tty_appender = tty;
    tty_set_title_specific_tty(tty_appender, "Kernel Log Viewer");
}

void klog_user_syslog(int level, char *buffer) {
    klog_append("USER", level, "%s[%d] %s", running_process()->name, running_process()->pid, buffer);
}

void klog_append(const char *module_name, log_level_t level, const char *format, ...) {

    if (strlen(format) == 0)
        return;

    // should check the level of the module, if any
    if (module_name != NULL && level > get_module_log_level(module_name))
        return; 

    va_list args;
    va_start(args, format);
    
    char buffer[256];
    vsprintfn(buffer, sizeof(buffer), format, args);

    memory_log_append(module_name, level, buffer);
    screen_log_append(module_name, level, buffer, true);
    serial_log_append(module_name, level, buffer, true);
    file_log_append(module_name, level, buffer, true);
    tty_log_append(module_name, level, buffer, true);

    va_end(args);
}

void klog_append_hex(const char *module_name, log_level_t level,  uint8_t *buffer, size_t length, uint32_t start_address) {

    if (length == 0)
        return;

    // should check the level of the module, if any
    if (module_name != NULL && level > get_module_log_level(module_name))
        return; 

    while (length > 0) {
        // using xxd's format, seems nice
        klog_append(module_name, level,
            "%08x: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c",
            start_address,
            buffer[0], buffer[1], buffer[2], buffer[3], 
            buffer[4], buffer[5], buffer[6], buffer[7],
            buffer[8], buffer[9], buffer[10], buffer[11], 
            buffer[12], buffer[13], buffer[14], buffer[15],
            printable(buffer[0]), printable(buffer[1]), printable(buffer[2]), printable(buffer[3]),
            printable(buffer[4]), printable(buffer[5]), printable(buffer[6]), printable(buffer[7]),
            printable(buffer[8]), printable(buffer[9]), printable(buffer[10]), printable(buffer[11]),
            printable(buffer[12]), printable(buffer[13]), printable(buffer[14]), printable(buffer[15])
        );
        buffer += 16;
        length -= length > 16 ? 16 : length;
        start_address += 16;
    }
}



// write to memory log, losing older data if needed
// would go with a circular buffer, but too complex to code right now
static void memlog_write(char *str) {
    int slen = strlen(str) + 1; // include the zero terminator

    // if it does not fit, make just enough room for it
    if (memlog.len + slen >= (int)sizeof(memlog.buffer)) {
        memmove(memlog.buffer, memlog.buffer + slen, sizeof(memlog.buffer) - slen);
        memlog.len -= slen;
    }

    // now copy it.
    memcpy(&memlog.buffer[memlog.len], str, slen);
    memlog.len = strlen(memlog.buffer);
}

static void memory_log_append(const char *module_name, log_level_t level, char *str) {
    if (level > appenders[LOGAPP_MEMORY].level)
        return;

    // first write preamble
    char buff[128];
    uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
    sprintfn(buff, sizeof(buff), "[%u.%03u] %s %s: ", msecs / 1000, msecs % 1000, module_name, level_captions[level]);

    memlog_write(buff);
    memlog_write(str);
    memlog_write("\n");
}

static void screen_log_append(const char *module_name, log_level_t level, char *str, bool decorated) {
    if (level > appenders[LOGAPP_SCREEN].level)
        return;

    uint8_t old_color = screen_get_color();
    if (decorated) {
        if (level < LOGLEV_ERROR)
            screen_set_color(VGA_COLOR_LIGHT_RED);
        else if (level == LOGLEV_WARN)
            screen_set_color(VGA_COLOR_LIGHT_BROWN);    

    }
    screen_write(str);
    if (decorated) {
        screen_write("\n");
        screen_set_color(old_color);
    }
}

static void serial_log_append(const char *module_name, log_level_t level, char *str, bool decorated) {
    if (level > appenders[LOGAPP_SERIAL].level)
        return;

    if (decorated) {
        char buff[128];
        uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
        sprintfn(buff, sizeof(buff), "[%u.%03u] %s %s: ", msecs / 1000, msecs % 1000, module_name, level_captions[level]);
        serial_write(buff);
    }
    serial_write(str);
    if (decorated) {
        serial_write("\n");
    }
}

static void file_log_append(const char *module_name, log_level_t level, char *str, bool decorated) {
    ; // not implemented yet. hopefully /var/log/kernel.log
}

static void tty_log_append(const char *module_name, log_level_t level, char *str, bool decorated) {
    if (tty_appender == NULL)
        return;
    if (level > appenders[LOGAPP_TTY].level)
        return;

    if (decorated) {
        char buff[128];
        uint32_t msecs = (uint32_t)timer_get_uptime_msecs();
        sprintfn(buff, sizeof(buff), "[%u.%03u] %s %s: ", msecs / 1000, msecs % 1000, module_name, level_captions[level]);
        tty_write_specific_tty(tty_appender, buff);
    }
    tty_write_specific_tty(tty_appender, str);
    if (decorated) {
        tty_write_specific_tty(tty_appender, "\n");
    }
}

static inline char printable(char c) {
    return (c >= ' ' && 'c' <= '~' ? c : '.');
}

