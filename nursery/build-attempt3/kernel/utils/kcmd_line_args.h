#pragma once
#include <ctypes.h>


/*
    Usual kernel arguments:
    root= (root filesystem device)
    rootfstype= (type)
    rw / ro  (mount rw / ro)
    init= (path to init program)
    initrd= / initramfs=
    loglevel= (0-7 or semantic)
    quiet  (reduce console output)
    debug  (enable extra diagnostics)
    console=  (ttyX, ttySX the device to use)
    acpi= (enable / disable ACPI)
    apic/noapic (apic control)
    pci=  (PCI subsystem options)
    irqpoll (polling fallback)
    mem=  (limit availabile RAM)
    iomem= (io memory handling)
    vmalloc=.. (kernel virtual memory limit)
    maxcpus=x (limit number of cpus)
    nosmp (disable smp)
    isolcpus=... (cpu isolation options)
    rootdelay=  (wait before mounting root)
    rootwait    (wait indefinitely before mounting root)

    (custom example keys include)
    vfs.debug
    vmem.debug
    ...
*/

typedef struct {
    const char *key;
    const char *value;   // NULL if no "=value"
} kcmd_arg_t;

typedef struct {
    kcmd_arg_t *args;
    size_t count;
} kcmdline_t;


void kcmd_parse(char *cmdline);
const char *kcmd_get(const char *key);
bool kcmd_has(const char *key);
