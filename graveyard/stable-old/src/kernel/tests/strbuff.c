#include <ctypes.h>
#include <klib/string.h>
#include <klib/strbuff.h>
#include "framework.h"


void test_strbuff() {
    strbuff_t *sb;

    // test a fixed allocation buffer
    sb = create_sb(32, SB_FIXED);
    assert(sb_length(sb) == 0);

    // in order to use it with printf() and other methods
    sb_strcpy(sb, "coding");
    assert(sb_charptr(sb) != NULL);
    assert(strlen(sb_charptr(sb)) == 6);
    assert(strcmp(sb_charptr(sb), "coding") == 0);
    sb_clear(sb);
    assert(sb_length(sb) == 0);

    // try deleting some content
    sb_strcpy(sb, "Hello there");
    assert(sb_length(sb) == 11);
    assert(sb_strcmp(sb, "Hello there") == 0);
    sb_delete(sb, 4, 1);
    assert(sb_length(sb) == 10);
    assert(sb_strcmp(sb, "Hell there") == 0);
    sb_strcpy(sb, "Hello there");
    sb_delete(sb, 4, 100);
    assert(sb_strcmp(sb, "Hell") == 0);
    sb_strcpy(sb, "Hello there");
    sb_delete(sb, -5, 100);
    assert(sb_strcmp(sb, "Hello there") == 0);
    sb_strcpy(sb, "Hello there");
    sb_delete(sb, 35, 100);
    assert(sb_strcmp(sb, "Hello there") == 0);

    sb_free(sb);

    // insert simple case
    sb = create_sb(12, SB_FIXED);
    sb_strcpy(sb, "hello");
    sb_append(sb, " there");
    assert(sb_strcmp(sb, "hello there") == 0);
    // beyond on the right
    sb_strcpy(sb, "hello");
    sb_insert(sb, sb_length(sb), " and goodbye");
    assert(sb_strcmp(sb, "hello and go") == 0);
    // beyond on the left
    sb_strcpy(sb, "hello");
    sb_insert(sb, 2, " and goodbye");
    assert(sb_strcmp(sb, "he and goodb") == 0);
    // make sure further append is ignored
    sb_append(sb, "further");
    assert(sb_strcmp(sb, "he and goodb") == 0);

    // insert: test out of boundaries
    sb_strcpy(sb, "hello");
    sb_insert(sb, -5, "bye bye!");
    assert(sb_strcmp(sb, "hello") == 0);
    sb_strcpy(sb, "hello");
    sb_insert(sb, 8, "bye bye!");
    assert(sb_strcmp(sb, "hello") == 0);
    // test insert huge string
    sb_strcpy(sb, "test");
    sb_insert(sb, 2, "abcdefgjklmnopqrstuvwxyz");
    assert(sb_strcmp(sb, "teabcdefgjkl") == 0);
    // test insert in the middle, overflowing left
    sb_strcpy(sb, "123456789012");
    sb_insert(sb, 5, "abc");
    assert(sb_strcmp(sb, "12345abc6789") == 0);

    sb_free(sb);

    // check SB_SCROLL behavior
    sb = create_sb(12, SB_SCROLL);
    sb_append(sb, "Monday,");
    assert(sb_strcmp(sb, "Monday,") == 0);
    sb_append(sb, "Tuesday,");
    assert(sb_strcmp(sb, "day,Tuesday,") == 0);
    sb_append(sb, "Wednesday,");
    assert(sb_strcmp(sb, "y,Wednesday,") == 0);
    sb_append(sb, "Thursday,");
    assert(sb_strcmp(sb, "ay,Thursday,") == 0);
    sb_append(sb, "Friday!");
    assert(sb_strcmp(sb, "sday,Friday!") == 0);
    sb_append(sb, "Very big buffer trying to test overflow behavior");
    assert(sb_strcmp(sb, "low behavior") == 0);

    sb_free(sb);

    // check SB_EXPAND mode
    sb = create_sb(12, SB_EXPAND);
    sb_append(sb, "Monday,");
    assert(sb_strcmp(sb, "Monday,") == 0);
    sb_append(sb, "Tuesday,");
    assert(sb_strcmp(sb, "Monday,Tuesday,") == 0);
    sb_append(sb, "Wednesday,");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,") == 0);
    sb_append(sb, "Thursday,");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,Thursday,") == 0);
    sb_append(sb, "Friday!");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,Thursday,Friday!") == 0);
    sb_insert(sb, 25, "(insert day here),");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,(insert day here),Thursday,Friday!") == 0);
    sb_free(sb);
}

