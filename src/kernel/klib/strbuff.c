#include <memory/kheap.h>
#include <klib/string.h>
#include <klib/strbuff.h>
#include <klog.h>

MODULE("SBUFF");


#define SB_FIXED    0x00
#define SB_SCROLL   0x01
#define SB_EXPAND   0x02


typedef struct strbuff {
    int capacity; // includes the null terminator
    int length;
    int flags;
    char *buffer;
} strbuff_t;

strbuff_t *create_sb(int capacity, int flags) {

    strbuff_t *sb = kmalloc(sizeof(strbuff_t));
    sb->capacity = capacity + 1; // include the null terminator
    sb->length = 0;
    sb->flags = flags;
    sb->buffer = kmalloc(sb->capacity);

    return sb;
}

void sb_free(strbuff_t *sb) {
    kfree(sb->buffer);
    kfree(sb);
}

int sb_length(strbuff_t *sb) {
    return sb->length;
}

void sb_clear(strbuff_t *sb) {
    sb->length = 0;
    sb->buffer[0] = '\0';
}

void sb_strcpy(strbuff_t *sb, char *str) {
    sb_clear(sb);
    sb_append(sb, str);
}

void sb_append(strbuff_t *sb, char *str) {
    sb_insert(sb, sb->length, str);
}

void sb_insert(strbuff_t *sb, int pos, char *str) {
    int len = strlen(str);

    // out of bounds
    if (pos < 0 || pos > sb->length || len == 0)
        return;

    // now, depending on settings, we have different behavior
    if (sb->flags == SB_FIXED) {

        // if exceeds to the right, trim
        if (pos + len > sb->capacity - 1) {
            len = sb->capacity - pos - 1;
            klog_debug("trimming from right, len=%d", len);
        }

        // if current buffer will overflow, trim it
        if (len + sb->length > sb->capacity - 1) {
            sb->length = sb->capacity - 1 - len;
            klog_debug("trimming from buffer, sb->length=%d", sb->length);
        }

    } else if (sb->flags == SB_SCROLL) {

        // make sure it fits
        if (len > sb->capacity - 1) {
            int diff = len - (sb->capacity - 1);
            len -= diff;
            str += diff;
            pos = 0;
        }

        // if there is space, blindly lose the first bytes to make room at the end
        if (sb->capacity - 1 - sb->length < len) {
            int needed = len - (sb->capacity - 1 - sb->length);
            memmove(sb->buffer, sb->buffer + needed, sb->capacity - needed + 1);
            sb->length -= needed;
            pos = sb->length;
        }

    } else if (sb->flags == SB_EXPAND) {

        // we'll just reallocate to make room
        if (sb->capacity < sb->length + len + 1) {
            while (sb->capacity < sb->length + len + 1)
                sb->capacity *= 2;
            char *new_ptr = kmalloc(sb->capacity);
            memcpy(new_ptr, sb->buffer, sb->length + 1);
            kfree(sb->buffer);
            sb->buffer = new_ptr;
        }
    }

    // in case there is nothing to inser...
    if (len == 0)
        return;
    
    // we should now be in position to insert
    if (len > sb->capacity - sb->length - 1) {
        klog_error("oops, there should be space for str of %d bytes in buffer[%d], with length=%d", len, sb->capacity, sb->length);
        return;
    }
    
    // length = 16, one zero terminator, capacity = 31 (30 + zero)
    //  0123456789012345678901234567890
    // [once upon a time0______________]
    //             "lost "
    
    memmove(sb->buffer + pos + len, sb->buffer + pos, sb->length - pos + 1);
    memcpy(sb->buffer + pos, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0'; // add terminator even in cases we do partial copy
}

void sb_delete(strbuff_t *sb, int pos, int len) {

    if (pos < 0 || pos >= sb->length || len <= 0)
        return;
    
    if (sb->length - pos < len)
        len = sb->length - pos;
    if (len <= 0)
        return;
    
    // length = 16, one zero terminator, capacity = 31 (30 + zero)
    // [once upon a time0______________]
    //      

    // +1 for the zero terminator
    memmove(sb->buffer + pos, sb->buffer + pos + len, sb->length - pos - len + 1);
    sb->length -= len;
}

int sb_strcmp(strbuff_t *sb, char *str) {
    return strcmp(sb->buffer, str);
}

char *sb_charptr(strbuff_t *sb) {
    return sb->buffer;
}


