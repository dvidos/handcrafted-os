# Kernel Logging Subsystem

This document describes the usage and architecture of the kernel's logging subsystem.

## Features

*   **Flexible Log Levels**: Supports various log levels from `CRITICAL` to `TRACE` (defined by `log_level_t`).
*   **Global and Module-Specific Filtering**:
    *   A global minimum log level can be set using `logger_set_global_minimum_log_level()`.
    *   Individual modules can define their own minimum log level using the `MODULE(module_name, log_level)` macro.
    *   Log entries are passed through if *either* the global log level *or* the module's log level meets or exceeds the entry's level. This ensures that the "most relaxed" (lowest numerical value, meaning more verbose) setting between global and module-specific configuration is applied.
*   **Pluggable Appenders**: Log entries are dispatched to registered "appenders." Each appender has its own log level filter; it will only process log entries that are at or above its configured level.
*   **Ring Buffer for Early Logs**: From the kernel's very first lines of execution, log messages are buffered into an internal ring buffer. This ensures no log data is lost before any appenders are initialized.
*   **Seamless Appender Integration**: When a new appender is added via `logger_add_appender()`, the entire contents of the ring buffer are immediately "dumped" (sent) to that new appender. This guarantees that all logs, even those from the earliest boot stages, are eventually routed to active appenders.

## Current Appenders

Currently, the logging subsystem supports dispatching log messages to:
*   **Memory**: The initial ring buffer itself serves as a temporary in-memory log.
*   **Screen**: Direct output to the console/screen.
*   **Serial Port**: Useful for debugging with external tools like QEMU, outputting logs over a serial connection.

## Extending Appenders

The design is modular, making it straightforward to implement new appenders. For example, a **File Appender** could easily be integrated to write logs to a persistent storage device.

## Usage Example

To enable logging for a module and define its level:

```c
// In your module's .c file
#include <logger.h> // Assuming logger.h is included via a common header or directly

MODULE("MY_MODULE", LOG_LEVEL_DEBUG); // Set module-specific log level to DEBUG

void my_function() {
    log_info("Doing something important.");
    log_debug("Variable value: %d", some_variable);
    log_warn("Something unexpected happened!");
}
```

To initialize the logger and add appenders (typically in a kernel initialization routine):

```c
#include <logger.h>
#include <serial.h> // For serial appender

extern void serial_appender_func(void *context, const char *timing, const char *module_name, const char *level_str, const char *message, bool raw_dump);
extern void screen_appender_func(void *context, const char *timing, const char *module_name, const char *level_str, const char *message, bool raw_dump);

void kernel_main() {
    init_logger(); // Initialize the ring buffer

    // Add a serial port appender that logs INFO and above
    logger_add_appender(serial_appender_func, (void*)SERIAL_COM1_BASE, LOG_LEVEL_INFO);

    // Add a screen appender that logs WARN and above
    logger_add_appender(screen_appender_func, NULL, LOG_LEVEL_WARN);

    logger_set_global_minimum_log_level(LOG_LEVEL_DEBUG); // Set global minimum to DEBUG
    // ... rest of kernel initialization ...
}
```
