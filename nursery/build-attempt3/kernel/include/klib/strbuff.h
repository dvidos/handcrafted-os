#ifndef _STRBUFF_H
#define _STRBUFF_H


#define SB_FIXED    0x00
#define SB_SCROLL   0x01
#define SB_EXPAND   0x02

typedef struct strbuff strbuff_t;


strbuff_t *create_sb(int capacity, int flags);
void sb_free(strbuff_t *sb);
int sb_length(strbuff_t *sb);
void sb_clear(strbuff_t *sb);
void sb_strcpy(strbuff_t *sb, char *str);
void sb_append(strbuff_t *sb, char *str);
void sb_insert(strbuff_t *sb, int pos, char *str);
void sb_delete(strbuff_t *sb, int pos, int len);
int sb_strcmp(strbuff_t *sb, char *str);
char *sb_charptr(strbuff_t *sb);

#endif
