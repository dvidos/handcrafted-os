#ifndef _FAT_PRIV_H
#define _FAT_PRIV_H


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

// for reading and writing fat sectors
typedef struct {
    uint8_t *buffer;
    uint32_t sector_no;
    bool dirty;
} sector_t;

// for reading/writing data clusters
typedef struct {
    uint8_t *buffer;
    uint32_t cluster_no;
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
    uint32_t root_dir_sectors_size; // for FAT12/16 only

    struct fat_operations *ops;
} fat_info;



// represents one file described inside a directory cluster
typedef struct {
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
    fat_dir_entry *dir_entry;  // general loading place for dir entries
    bool is_root_directory;           // check if the file is the root directory (as it is special in FAT16)

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
        sector_t *sector;
        uint32_t offset_in_sector;
    } fat16root;

    // for FAT32 and subdirs of FAT16,
    // we use file operations to manipulate the directory contents 
    fat_priv_file_info *pf;    
} fat_priv_dir_info;


// function pointers for better intellisense and encapsulation
struct fat_operations {
    int (*read_fat_sector)(fat_info *fat, uint32_t sector_no, sector_t *sector);
    int (*write_fat_sector)(fat_info *fat, sector_t *sector);
    int (*get_fat_entry_value)(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t *value);
    int (*set_fat_entry_value)(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t value);
    bool (*is_end_of_chain_entry_value)(fat_info *fat, uint32_t value);
    int (*find_a_free_cluster)(fat_info *fat, sector_t *sector, uint32_t *cluster_no);
    int (*get_n_index_cluster_no)(fat_info *fat, sector_t *sector, uint32_t first_cluster, uint32_t cluster_n_index, uint32_t *cluster_no);
    int (*read_data_cluster)(fat_info *fat, uint32_t cluster_no, cluster_t *cluster);
    int (*write_data_cluster)(fat_info *fat, cluster_t *cluster);

    // higher level functions to help top level functions
    int (*ensure_first_cluster_allocated)(fat_info *fat, fat_priv_file_info *pf);
    int (*move_to_next_data_cluster)(fat_info *fat, fat_priv_file_info *pf, bool create_if_needed);
    int (*move_to_n_index_data_cluster)(fat_info *fat, fat_priv_file_info *pf, uint32_t cluster_n_index);

    // high level functions, allow both files and dir contents read/write
    int (*priv_file_open)(fat_info *fat, uint32_t cluster_no, uint32_t file_size, fat_priv_file_info *pf);
    int (*priv_file_read)(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
    int (*priv_file_write)(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
    int (*priv_file_seek)(fat_info *fat, fat_priv_file_info *pf, int offset, enum seek_origin origin);
    int (*priv_file_close)(fat_info *fat, fat_priv_file_info *pf);


    // functions diff between FAT16 and FAT32 (to be deprecated)
    int (*root_dir_open)(file_t *file);
    int (*root_dir_read)(file_t *file, fat_dir_entry *entry);
    int (*root_dir_close)(file_t *file);
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

static int priv_file_open(fat_info *fat, uint32_t cluster_no, uint32_t file_size, fat_priv_file_info *pf);
static int priv_file_read(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
static int priv_file_write(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length);
static int priv_file_seek(fat_info *fat, fat_priv_file_info *pf, int offset, enum seek_origin origin);
static int priv_file_close(fat_info *fat, fat_priv_file_info *pf);


// debug
static void debug_fat_info(fat_info *fat);
static void debug_fat_dir_entry(bool title_line, fat_dir_entry *entry);

// dir entries
static int extract_dir_entry(uint8_t *buffer, uint32_t buffer_len, uint32_t *offset, fat_dir_entry *entry);

// fat vfs interaction
static int fat_probe(struct partition *partition);
static struct file_ops *fat_get_file_operations();

// fat dir operations
static int dir_in_data_clusters_open(file_t *file, uint32_t cluster_no);
static int dir_in_data_clusters_read(file_t *file, fat_dir_entry *entry);
static int dir_in_data_clusters_close(file_t *file);

static int fat16_root_dir_open(file_t *file);
static int fat16_root_dir_read(file_t *file, fat_dir_entry *entry);
static int fat16_root_dir_close(file_t *file);

static int fat32_root_dir_open(file_t *file);
static int fat32_root_dir_read(file_t *file, fat_dir_entry *entry);
static int fat32_root_dir_close(file_t *file);

static int find_entry_in_root_dir(file_t *file, char *target_name, fat_dir_entry *entry);
static int find_entry_in_sub_dir(file_t *file, uint32_t dir_cluster_no, char *target_name, fat_dir_entry *entry);
static int find_entry_for_path(file_t *file, char *path, fat_dir_entry *entry);

static int fat_opendir(char *path, file_t *file);
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
