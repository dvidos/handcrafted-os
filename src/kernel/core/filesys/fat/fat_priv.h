#ifndef _FAT_PRIV_H
#define _FAT_PRIV_H

#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <drivers/clock.h>
#include <lock.h>
#include <klog.h>

#define min(a, b)     ((a) <= (b) ? (a) : (b))
#define max(a, b)     ((a) >= (b) ? (a) : (b))



// used to lock when writing. should be per logical volume
extern lock_t fat_write_lock;

typedef struct fat_boot_sector_extension_12_16 // applies to FAT12 as well
{
	//extended fat12 and fat16 stuff
	unsigned char		bios_drive_num;
	unsigned char		reserved1;
	unsigned char		boot_signature;
	unsigned int		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
}__attribute__((packed)) fat_boot_sector_extension_12_16_t;

typedef struct fat_boot_sector_extension_32
{
	unsigned int		sectors_per_fat_32;
	unsigned short		extended_flags;
	unsigned short		fat_version;
	unsigned int		root_dir_cluster;
	unsigned short		fat_info;
	unsigned short		backup_BS_sector;
	unsigned char 		reserved_0[12];
	unsigned char		drive_number;
	unsigned char 		reserved_1;
	unsigned char		boot_signature;
	unsigned int 		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
}__attribute__((packed)) fat_boot_sector_extension_32_t;

typedef struct fat_boot_sector
{
	unsigned char 		bootjmp[3];
	unsigned char 		oem_name[8];
	unsigned short 	    bytes_per_sector;
	unsigned char		sectors_per_cluster;
	unsigned short		reserved_sector_count;
	unsigned char		number_of_fats;
	unsigned short		root_entry_count;
	unsigned short		total_sectors_16bits;
	unsigned char		media_type;
	unsigned short		sectors_per_fat_16; // for 12/16 only
	unsigned short		sectors_per_track;
	unsigned short		head_side_count;
	unsigned int 		hidden_sector_count;
	unsigned int 		total_sectors_32bits;

	// use the proper entry, once the driver actually knows what type of FAT this is.
    union {
        fat_boot_sector_extension_32_t fat_32;
        fat_boot_sector_extension_12_16_t fat_12_16;
    } types;

    char padding[420];
    uint16_t bootable_sector_signature;

}__attribute__((packed)) fat_boot_sector_t;

// for reading and writing fat sectors
typedef struct {
    uint8_t *buffer;
    uint32_t sector_no;
    bool loaded;
    bool dirty;
} sector_t;

// for reading/writing data clusters
typedef struct {
    uint8_t *buffer;
    uint32_t cluster_no;
    bool loaded;
    bool dirty;
} cluster_t;


enum FAT_TYPE { FAT32, FAT16, FAT12 };

// stored in the private data of the partition or logical volume
typedef struct {
    struct partition *partition;
    fat_boot_sector_t *boot_sector;
    enum FAT_TYPE fat_type;

    uint16_t bytes_per_sector;
    uint16_t bytes_per_cluster;
    uint8_t  sectors_per_cluster;
    uint32_t sectors_per_fat;
    uint32_t largest_cluster_no;
    uint32_t end_of_chain_value;

    // absolute addressing, relative to disk
    uint32_t fat_starting_lba;
    uint32_t root_dir_starting_lba;
    uint32_t data_clusters_starting_lba;
    uint32_t root_dir_sectors_count; // for FAT12/16 only

    struct fat_operations *ops;
} fat_info;


#define BYTES_PER_DIR_SLOT    32

// represents one file described inside a directory cluster
typedef struct {
    int short_entry_slot_no; // 32-bit slot no, for the short-name & data entry
    int long_name_slots_count;   // how many slots we have allocated for long name

    // these should be part of the struct, needing no further malloc()
    char short_name[12+1]; // 8 + dot + 3 + zero term
    char long_name_ucs2[512 + 2]; // UCS-2 format

    union {
        uint8_t value;
        struct {
            uint8_t read_only: 1;
            uint8_t hidden: 1;
            uint8_t system: 1;
            uint8_t volume_label: 1;
            uint8_t directory: 1;
            uint8_t archive: 1;
        }__attribute__((packed)) flags;
    } attributes;

    uint32_t first_cluster_no;
    uint32_t file_size;

    uint16_t created_year;
    uint8_t created_month;
    uint8_t created_day;
    uint8_t created_hour;
    uint8_t created_min;
    uint8_t created_sec;

    uint16_t modified_year;
    uint8_t modified_month;
    uint8_t modified_day;
    uint8_t modified_hour;
    uint8_t modified_min;
    uint8_t modified_sec;
} fat_dir_entry;


#define ATTR_READONLY   0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOL_LABEL  0x08
#define ATTR_SUB_DIR    0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  0x0F  // hack!


#define DIR_NAME_DELETED      0xE5
#define DIR_NAME_END_OF_LIST  0x00

// stored in the private data of a file_t pointer
typedef struct {
    uint32_t offset;                  // offset in bytes in file or directory contents
    uint32_t size;                    // current file size, in bytes, to detect EOF

    uint32_t first_cluster_no;        // first cluster of the file/dir (unless root dir in FAT16)
    uint32_t cluster_n_index;         // cluster incremental number, zero based (e.g. 3rd cluster in the chain)

    sector_t *sector;            // for maintaining FAT entries
    cluster_t *cluster;          // for the actual data
} fat_priv_file_info;


// stored in the private data of a file_t pointer
typedef struct {
    // we'll need some more things for fat16, e.g. load sectors etc.
    bool is_fat16_root;

    struct {
        uint8_t *sector_buffer;
        uint32_t sector_no;
        uint32_t offset_in_sector;
        bool     sector_dirty;
        bool     loaded;
    } fat16_root_data;

    // for FAT32 and subdirs of FAT16,
    // we use file operations to manipulate the directory contents 
    fat_priv_file_info *pf;    
} fat_priv_dir_info;


// function pointers for better intellisense and encapsulation
struct fat_operations {

    // working with the allocation table
    int (*read_fat_sector)(fat_info *fat, uint32_t sector_no, sector_t *sector);
    int (*write_fat_sector)(fat_info *fat, sector_t *sector);
    int (*get_fat_entry_value)(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t *value);
    int (*set_fat_entry_value)(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t value);
    bool (*is_end_of_chain_entry_value)(fat_info *fat, uint32_t value);
    int (*find_a_free_cluster)(fat_info *fat, sector_t *sector, uint32_t *cluster_no);
    int (*get_n_index_cluster_no)(fat_info *fat, sector_t *sector, uint32_t first_cluster, uint32_t cluster_n_index, uint32_t *cluster_no);

    // working with data clusters
    int (*read_data_cluster)(fat_info *fat, uint32_t cluster_no, cluster_t *cluster);
    int (*write_data_cluster)(fat_info *fat, cluster_t *cluster);

    // higher level functions to help top level functions
    int (*ensure_first_cluster_allocated)(fat_info *fat, fat_priv_file_info *pf);
    int (*move_to_next_data_cluster)(fat_info *fat, fat_priv_file_info *pf, bool create_if_needed);
    int (*move_to_n_index_data_cluster)(fat_info *fat, fat_priv_file_info *pf, uint32_t cluster_n_index);

    // high level file functions, allow both files and dir contents read/write
    int (*priv_file_open)(fat_info *fat, uint32_t cluster_no, uint32_t file_size, fat_priv_file_info **ppf);
    int (*priv_file_read)(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
    int (*priv_file_write)(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
    int (*priv_file_seek)(fat_info *fat, fat_priv_file_info *pf, int offset, enum seek_origin origin);
    int (*priv_file_close)(fat_info *fat, fat_priv_file_info *pf);

    // mid level dir functions, abstract away FAT16/32 differences
    int (*priv_dir_open_root)(fat_info *fat, fat_priv_dir_info **ppd);
    int (*priv_dir_open_cluster)(fat_info *fat, uint32_t cluster_no, fat_priv_dir_info **ppd);
    int (*priv_dir_find_and_open)(fat_info *fat,  uint8_t *path, bool containing_dir, fat_priv_dir_info **ppd);
    int (*priv_dir_read_slot)(fat_info *fat, fat_priv_dir_info *pd, uint8_t *buffer32);
    int (*priv_dir_write_slot)(fat_info *fat, fat_priv_dir_info *pd, uint8_t *buffer32);
    int (*priv_dir_get_slot_no)(fat_info *fat, fat_priv_dir_info *pd);
    int (*priv_dir_seek_slot)(fat_info *fat, fat_priv_dir_info *pd, int slot_no);
    int (*priv_dir_close)(fat_info *fat, fat_priv_dir_info *pd);

    // high level dir functions
    int (*priv_dir_read_one_entry)(fat_info *fat, fat_priv_dir_info *pd, fat_dir_entry *entry);
    int (*priv_dir_create_entry)(fat_info *fat, fat_priv_dir_info *pd, char *name, bool directory);
    int (*priv_dir_entry_invalidate)(fat_info *fat, fat_priv_dir_info *pd, fat_dir_entry *entry);

    // facilitate resolving paths to priv dirs and files
    int (*find_entry_in_dir)(fat_info *fat, fat_priv_dir_info *pd, uint8_t *name, fat_dir_entry *entry);
    int (*find_path_dir_entry)(fat_info *fat,  uint8_t *path, bool containing_dir, fat_dir_entry *entry);
};


// clusters low level work
static int read_fat_sector(fat_info *fat, uint32_t sector_no, sector_t *sector);
static int write_fat_sector(fat_info *fat, sector_t *sector);
static int get_fat_entry_value(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t *value);
static int set_fat_entry_value(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t value);
static bool is_end_of_chain_entry_value(fat_info *fat, uint32_t value);
static int find_a_free_cluster(fat_info *fat, sector_t *sector, uint32_t *cluster_no);
static int get_n_index_cluster_no(fat_info *fat, sector_t *sector, uint32_t first_cluster, uint32_t cluster_n_index, uint32_t *cluster_no);
static int read_data_cluster(fat_info *fat, uint32_t cluster_no, cluster_t *cluster);
static int write_data_cluster(fat_info *fat, cluster_t *cluster);

static int ensure_first_cluster_allocated(fat_info *fat, fat_priv_file_info *pf);
static int move_to_next_data_cluster(fat_info *fat, fat_priv_file_info *pf, bool create_if_needed);
static int move_to_n_index_data_cluster(fat_info *fat, fat_priv_file_info *pf, uint32_t cluster_n_index);

static int priv_file_open(fat_info *fat, uint32_t cluster_no, uint32_t file_size, fat_priv_file_info **ppf);
static int priv_file_read(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
static int priv_file_write(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
static int priv_file_seek(fat_info *fat, fat_priv_file_info *pf, int offset, enum seek_origin origin);
static int priv_file_close(fat_info *fat, fat_priv_file_info *pf);

// opening and closing directories
static int priv_dir_open_root(fat_info *fat, fat_priv_dir_info **ppd);
static int priv_dir_open_cluster(fat_info *fat, uint32_t cluster_no, fat_priv_dir_info **ppd);
static int priv_dir_find_and_open(fat_info *fat,  uint8_t *path, bool containing_dir, fat_priv_dir_info **ppd);
static int priv_dir_read_slot(fat_info *fat, fat_priv_dir_info *pd, uint8_t *buffer32);
static int priv_dir_write_slot(fat_info *fat, fat_priv_dir_info *pd, uint8_t *buffer32);
static int priv_dir_get_slot_no(fat_info *fat, fat_priv_dir_info *pd);
static int priv_dir_seek_slot(fat_info *fat, fat_priv_dir_info *pd, int slot_no);
static int priv_dir_close(fat_info *fat, fat_priv_dir_info *pd);

static int priv_dir_read_one_entry(fat_info *fat, fat_priv_dir_info *pd, fat_dir_entry *entry);
static int priv_dir_create_entry(fat_info *fat, fat_priv_dir_info *pd, char *name, bool directory);
static int priv_dir_entry_invalidate(fat_info *fat, fat_priv_dir_info *pd, fat_dir_entry *entry);

static int find_entry_in_dir(fat_info *fat, fat_priv_dir_info *pd, uint8_t *name, fat_dir_entry *entry);
static int find_path_dir_entry(fat_info *fat,  uint8_t *path, bool containing_dir, fat_dir_entry *entry);

// debug
static void debug_fat_info(fat_info *fat);
static void debug_fat_dir_entry(bool title_line, fat_dir_entry *entry);

// dir entries
static inline bool is_dir_slot_long_name(uint8_t *buffer32);
static inline bool is_dir_slot_eof(uint8_t *buffer32);
static inline bool is_dir_slot_deleted(uint8_t *buffer32);
static inline void dir_slot_mark_deleted(uint8_t *buffer32);
static void dir_slot_to_entry(uint8_t *buffer, fat_dir_entry *entry);
static void dir_entry_to_slot(fat_dir_entry *entry, uint8_t *buffer);
static void dir_entry_to_long_name(uint8_t *buffer, fat_dir_entry *entry);
static void dir_long_name_to_slot(fat_dir_entry *entry, int seq_no, uint8_t *buffer);
static void fat_dir_entry_to_vfs_dir_entry(fat_dir_entry *fat_entry, dir_entry_t *vfs_entry);
static void dir_entry_set_created_time(fat_dir_entry *entry, real_time_clock_info_t *time);
static void dir_entry_set_modified_time(fat_dir_entry *entry, real_time_clock_info_t *time);


// fat vfs interaction
static int fat_probe(struct partition *partition);
static struct file_ops *fat_get_file_operations();

static int fat_opendir(char *path, file_t *file);
static int fat_rewinddir(file_t *file);
static int fat_readdir(file_t *file, struct dir_entry *dir_entry);
static int fat_closedir(file_t *file);

// file operations
static uint32_t calculate_new_file_offset(uint32_t old_position, uint32_t size, int offset, enum seek_origin origin);
static int fat_open(char *path, file_t *file);
static int fat_read(file_t *file, char *buffer, int length);
static int fat_write(file_t *file, char *buffer, int length);
static int fat_seek(file_t *file, int offset, enum seek_origin origin);
static int fat_close(file_t *file);


#endif
