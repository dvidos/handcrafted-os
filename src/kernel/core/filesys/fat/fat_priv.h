#ifndef _FAT_PRIV_H
#define _FAT_PRIV_H


#include <stdint.h>
#include <filesys/vfs.h>
#include <filesys/partition.h>


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



enum FAT_TYPE { FAT32, FAT16, FAT12 };

// stored in the private data of the partition or logical volume
struct fat_info {
    struct partition *partition;
    fat_boot_sector_t *boot_sector;
    enum FAT_TYPE fat_type;

    uint16_t bytes_per_sector;
    uint16_t bytes_per_cluster;
    uint8_t  sectors_per_cluster;
    uint32_t sectors_per_fat;
    uint32_t largest_cluster_no;

    // absolute addressing, relative to disk
    uint32_t fat_starting_lba;
    uint32_t root_dir_starting_lba;
    uint32_t data_clusters_starting_lba;
    uint32_t root_dir_sectors_size; // for FAT12/16 only

    // here's an idea: strategy pattern
    // int (*root_dir_open)(struct fat_info *info);
    // int (*root_dir_read)(struct fat_info *info, struct fat_dir_entry *entry);
    // int (*root_dir_close)(struct fat_info *info);
    // int (*get_next_cluster_no)(struct fat_info *info, uint32_t cluster_no, uint32_t *next_cluster_no);
};



// represents one file described inside a directory cluster
struct fat_dir_entry {
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
};

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
struct fat_priv_file_info {
    struct fat_dir_entry *dir_entry;  // general loading place
    bool is_root_directory;           // check if root directory (as it is special in FAT16)
    uint32_t first_cluster_no;        // first cluster of the file/dir (unless root dir in FAT16)
    uint32_t curr_cluster_no;         // tracks current cluster, can be equal to the first
    char *cluster_buffer;             // buffer for reading / writing the cluster
    uint32_t offset;                  // offset in file or directory contents
    uint32_t size;                    // current file size, to detect EOF
};


// clusters
static int get_next_cluster_no(struct fat_info *info, uint32_t current_cluster_no, uint32_t *next_cluster_no, char *sector_buffer);
static int read_cluster(struct fat_info *info, uint32_t cluster_no, uint8_t *buffer);
static int write_cluster(struct fat_info *info, uint32_t cluster_no, uint8_t *buffer);

// debug
static void debug_fat_info(struct fat_info *info);
static void debug_fat_dir_entry(bool title_line, struct fat_dir_entry *entry);

// dir entries
static int extract_next_dir_entry(uint8_t *buffer, uint32_t buffer_len, uint32_t *offset, struct fat_dir_entry *entry);

// fat vfs interaction
static int fat_probe(struct partition *partition);
static struct file_ops *fat_get_file_operations();

// fat dir operations
static int fat16_root_dir_open(file_t *file);
static int fat16_root_dir_read(file_t *file, struct fat_dir_entry *entry);
static int fat16_root_dir_close(file_t *file);

static int dir_in_data_clusters_open(file_t *file, uint32_t cluster_no);
static int dir_in_data_clusters_read(file_t *file, struct fat_dir_entry *entry);
static int dir_in_data_clusters_close(file_t *file);

static int find_entry_in_root_dir(file_t *file, char *target_name, struct fat_dir_entry *entry);
static int find_entry_in_sub_dir(file_t *file, uint32_t dir_cluster_no, char *target_name, struct fat_dir_entry *entry);
static int find_entry_for_path(file_t *file, char *path, struct fat_dir_entry *entry);

static int fat_opendir(char *path, file_t *file);
static int fat_readdir(file_t *file, struct dir_entry *dir_entry);
static int fat_closedir(file_t *file);

// file operations
static int fat_open(char *path, file_t *file);
static int fat_read(file_t *file, char *buffer, int length);
static int fat_seek(file_t *file, int position, enum seek_origin origin);
static int fat_close(file_t *file);



#endif
