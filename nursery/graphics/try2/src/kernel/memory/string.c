
void *memset(void *dest, int c, int size) {
    void *d = dest;
    __asm__ volatile (
        "movl %0, %%edi\n\t"      // dest
        "movl %1, %%eax\n\t"      // value to fill
        "movl %2, %%ecx\n\t"      // size
        "rep stosb"               // fill byte by byte
        : /* no output */
        : "r"(d), "r"(c & 0xFF), "r"(size)
        : "edi","eax","ecx","memory"
    );
    return dest;
}

void *memcpy(void *dest, const void *src, int size) {
    void *d = dest;
    const void *s = src;
    __asm__ volatile (
        "movl %0, %%edi\n\t"      // dest
        "movl %1, %%esi\n\t"      // src
        "movl %2, %%ecx\n\t"      // size
        "rep movsb"               // copy byte by byte
        : /* no output */
        : "r"(d), "r"(s), "r"(size)
        : "edi","esi","ecx","memory"
    );
    return dest;
}

int memcmp(const void *s1, const void *s2, int size) {
    const unsigned char *p1 = s1, *p2 = s2;
    for (int i = 0; i < size; i++) {
        if (p1[i] != p2[i])
            return (int)p1[i] - (int)p2[i];
    }
    return 0;
}
