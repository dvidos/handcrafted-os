#include "types.h"
#include "bios.h"
#include "funcs.h"
#include "memory.h"


void *heap_start;
void *heap_end;
void *heap_free_block_start;

void init_heap(word start, word end) {
    // casting to different size
    heap_start = (void *)(int)start;
    heap_end = (void *)(int)end;

    memset(heap_start, 0, heap_end - heap_start);
    heap_free_block_start = heap_start;
}

word heap_free_space() {
    return (heap_end - heap_free_block_start);
}

void *malloc(int size) {
    if (heap_free_block_start + size > heap_end) {
        printf("Heap free block start 0x%x, Heap end 0x%x, free heap size %d KB, %d bytes requested\n",
            heap_free_block_start,
            heap_end,
            (heap_end - heap_free_block_start) / 1024,
            size
        );
        bios_print_str("\nPanic: heap space exhausted\n");
        freeze();
    }
    void *ptr = heap_free_block_start;
    heap_free_block_start += size;
    return ptr;
}



void print_mem_map_entry(struct mem_map_entry_24 *entry) {
    char addr_buff[16+1];
    char size_buff[16+1];
    if (entry->address_high > 0)
        strcpy(addr_buff, ">4GB");
    else
        itosize(entry->address_low, addr_buff);
    if (entry->size_high > 0)
        strcpy(size_buff, ">4GB");
    else
        itosize(entry->size_low, size_buff);

    printf("  addr %08x %5s   len %08x %5s   type %s\n",
        entry->address_low,
        addr_buff,
        entry->size_low,
        size_buff,
        entry->type == 1 ? "Usable" : (entry->type == 2 ? "Reserved" : "?")
    );
}

void detect_memory() {
    int err;

    word low_mem_kb = bios_get_low_memory_in_kb();
    printf("Low memory: %u KB\n", low_mem_kb);
    
    word kb_above_1mb = 0;
    word pg_64kb_above_16mb = 0;
    err = bios_detect_memory_map_e801(&kb_above_1mb, &pg_64kb_above_16mb);
    if (err) {
        printf("Error getting high memory\n");
    } else {
        printf("Memory above 1MB: %u KB (%u MB)\n", kb_above_1mb, (kb_above_1mb / 1024));
        printf("Memory above 16MB: %u pages of 64KB (%u MB)\n", pg_64kb_above_16mb, pg_64kb_above_16mb / 16);
    }

    int memory_map_entries = 0;
    char *mmap_addr = malloc(sizeof(struct mem_map_entry_24) * 16);
    err = bios_detect_memory_map_e820(mmap_addr, &memory_map_entries);
    if (err) {
        bios_print_str("Error getting memory map\n");
    } else {
        printf("Memory map from E820:\n");
        struct mem_map_entry_24 *entry = (struct mem_map_entry_24 *)mmap_addr;
        for (int i = 0; i < memory_map_entries; i++) {
            print_mem_map_entry(entry);
            entry++;
        }
    }
}


