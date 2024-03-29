#include <bits.h>
#include <klib/string.h>
#include <klog.h>
#include <errors.h>
#include <filesys/vfs.h>
#include <drivers/clock.h>
#include "fat_priv.h"


static inline bool is_dir_slot_long_name(uint8_t *buffer32) {
    return buffer32[0x0B] == ATTR_LONG_NAME;
}

static inline bool is_dir_slot_eof(uint8_t *buffer32) {
    return buffer32[0x0B] != ATTR_LONG_NAME && buffer32[0] == DIR_NAME_END_OF_LIST;
}

static inline bool is_dir_slot_deleted(uint8_t *buffer32) {
    return buffer32[0x0B] != ATTR_LONG_NAME && buffer32[0] == DIR_NAME_DELETED;
}

static inline void dir_slot_mark_deleted(uint8_t *buffer32) {
    buffer32[0] = DIR_NAME_DELETED;
}

static void dir_slot_to_entry(uint8_t *buffer, fat_dir_entry *entry) {

    entry->attributes.value = buffer[0x0B];

    uint16_t cluster_hi = *(uint16_t*)&buffer[0x14];
    uint16_t cluster_low = *(uint16_t*)&buffer[0x1a];
    entry->first_cluster_no = ((uint32_t)cluster_hi) << 16 | (uint32_t)cluster_low;
    entry->file_size = *(uint32_t *)&buffer[0x1c];

    uint16_t timestamp = *(uint16_t *)&buffer[0x0E];
    entry->created_hour = BIT_RANGE(timestamp, 15, 11);
    entry->created_min  = BIT_RANGE(timestamp, 10,  5);
    entry->created_sec  = BIT_RANGE(timestamp,  4,  0) * 2; // stores secs/2

    timestamp = *(uint16_t *)&buffer[0x10];
    entry->created_year = 1980 + BIT_RANGE(timestamp, 15, 9);
    entry->created_month = BIT_RANGE(timestamp, 8, 5);
    entry->created_day = BIT_RANGE(timestamp, 4, 0);

    timestamp = *(uint16_t *)&buffer[0x16];
    entry->modified_hour = BIT_RANGE(timestamp, 15, 11);
    entry->modified_min  = BIT_RANGE(timestamp, 10,  5);
    entry->modified_sec  = BIT_RANGE(timestamp,  4,  0) * 2; // stores secs/2

    timestamp = *(uint16_t *)&buffer[0x18];
    entry->modified_year = 1980 + BIT_RANGE(timestamp, 15, 9);
    entry->modified_month = BIT_RANGE(timestamp, 8, 5);
    entry->modified_day = BIT_RANGE(timestamp, 4, 0);

    // parse short name
    memset(entry->short_name, 0, sizeof(entry->short_name));
    for (int i = 0; i < 8; i++)
        if (buffer[i] != ' ')
            entry->short_name[i] = tolower(buffer[i]);
    if (buffer[8] != ' ')
        entry->short_name[strlen(entry->short_name)] = '.';
    for (int i = 8; i < 11; i++)
        if (buffer[i] != ' ')
            entry->short_name[strlen(entry->short_name)] = tolower(buffer[i]);
}

static void dir_entry_to_slot(fat_dir_entry *entry, uint8_t *buffer) {
    memset(buffer, 0, 32);

    buffer[0x0b] = entry->attributes.value;

    *(uint16_t *)&buffer[0x14] = HIGH_WORD(entry->first_cluster_no);
    *(uint16_t *)&buffer[0x1a] = LOW_WORD(entry->first_cluster_no);
    *(uint32_t *)&buffer[0x1c] = entry->file_size;

    *(uint16_t *)&buffer[0x0E] = (uint16_t)(
        SET_BIT_RANGE(entry->created_hour,    15, 11) |
        SET_BIT_RANGE(entry->created_min,     10,  5) |
        SET_BIT_RANGE(entry->created_sec / 2,  4,  0)  // 2 seconds precision
    );

    *(uint16_t *)&buffer[0x10] = (uint16_t)(
        SET_BIT_RANGE(entry->created_year - 1980, 15, 9) |
        SET_BIT_RANGE(entry->created_month,        8, 5) |
        SET_BIT_RANGE(entry->created_day,          4, 0)
    );

    *(uint16_t *)&buffer[0x16] = (uint16_t)(
        SET_BIT_RANGE(entry->modified_hour,    15, 11) |
        SET_BIT_RANGE(entry->modified_min,     10,  5) |
        SET_BIT_RANGE(entry->modified_sec / 2,  4,  0)  // 2 seconds precision
    );

    *(uint16_t *)&buffer[0x18] = (uint16_t)(
        SET_BIT_RANGE(entry->modified_year - 1980, 15, 9) |
        SET_BIT_RANGE(entry->modified_month,        8, 5) |
        SET_BIT_RANGE(entry->modified_day,          4, 0)
    );

    // name, we need better logic, to split name/extension
    memset(buffer, ' ', 11);
    int pos = 0;
    int max_chars = 8;
    int chars_so_far = 0;
    int len = strlen(entry->short_name);
    if (strcmp(entry->short_name, ".") == 0 || strcmp(entry->short_name, "..") == 0) {
        memcpy(buffer, entry->short_name, len);
    } else {
        for (int i = 0; i < len; i++) {
            if (entry->short_name[i] == '.') {
                pos = 8;
                max_chars = 3;
                chars_so_far = 0;
                continue;
            }
            if (chars_so_far < max_chars) {
                buffer[pos++] = toupper(entry->short_name[i]);
                chars_so_far++;
            }
        }
    }
}

static void dir_entry_to_long_name(uint8_t *buffer, fat_dir_entry *entry) {
    // uint8_t seq_no = buffer[0] & 0x3F; // starts with 1
    // uint8_t is_last = buffer[0] & 0x40;

    // // each sequence entry holds 13 2-byte characters
    // int long_offset = (seq_no - 1) * (13 * 2);
    // memcpy(entry->long_name_ucs2 + long_offset +  0, buffer + 0x01, 10);
    // memcpy(entry->long_name_ucs2 + long_offset + 10, buffer + 0x0E, 12);
    // memcpy(entry->long_name_ucs2 + long_offset + 22, buffer + 0x1C,  4);
}

static void dir_long_name_to_slot(fat_dir_entry *entry, int seq_no, uint8_t *buffer) {
}

static void fat_dir_entry_to_vfs_dir_entry(fat_dir_entry *fat_entry, dir_entry_t *vfs_entry) {

    strncpy(vfs_entry->short_name, fat_entry->short_name, sizeof(vfs_entry->short_name));
    vfs_entry->file_size = fat_entry->file_size;
    vfs_entry->location_in_dev = fat_entry->first_cluster_no;

    vfs_entry->flags.label = fat_entry->attributes.flags.volume_label;
    vfs_entry->flags.dir = fat_entry->attributes.flags.directory;
    vfs_entry->flags.read_only = fat_entry->attributes.flags.read_only;

    vfs_entry->created.year = fat_entry->created_year;
    vfs_entry->created.month = fat_entry->created_month;
    vfs_entry->created.day = fat_entry->created_day;
    vfs_entry->created.hours = fat_entry->created_hour;
    vfs_entry->created.minutes = fat_entry->created_min;
    vfs_entry->created.seconds = fat_entry->created_sec;
    
    vfs_entry->modified.year = fat_entry->modified_year;
    vfs_entry->modified.month = fat_entry->modified_month;
    vfs_entry->modified.day = fat_entry->modified_day;
    vfs_entry->modified.hours = fat_entry->modified_hour;
    vfs_entry->modified.minutes = fat_entry->modified_min;
    vfs_entry->modified.seconds = fat_entry->modified_sec;
}

static void fat_dir_entry_to_file_descriptor(file_descriptor_t *parent_dir, fat_dir_entry *fat_entry, file_descriptor_t **fd) {
    (*fd) = create_file_descriptor(
        parent_dir->superblock, 
        fat_entry->short_name, 
        fat_entry->first_cluster_no,
        parent_dir);
    (*fd)->size = fat_entry->file_size;

    if (fat_entry->attributes.flags.directory)
        (*fd)->flags = FD_DIR;
    else if (!fat_entry->attributes.flags.volume_label)
        (*fd)->flags = FD_FILE;
}

static void dir_entry_set_created_time(fat_dir_entry *entry, real_time_clock_info_t *time) {
    entry->created_year = time->years;
    entry->created_month = time->months;
    entry->created_day = time->days;
    entry->created_hour = time->hours;
    entry->created_min = time->minutes;
    entry->created_sec = time->seconds;
}

static void dir_entry_set_modified_time(fat_dir_entry *entry, real_time_clock_info_t *time) {
    entry->modified_year = time->years;
    entry->modified_month = time->months;
    entry->modified_day = time->days;
    entry->modified_hour = time->hours;
    entry->modified_min = time->minutes;
    entry->modified_sec = time->seconds;
}


