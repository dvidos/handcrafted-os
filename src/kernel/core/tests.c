#include <drivers/screen.h>
#include <klib/string.h>
#include <klib/strbuff.h>
#include <klib/path.h>
#include <klog.h>
#include <errors.h>


#define assert(a)    \
    if (!(a)) {      \
        klog_error("Assertion failed: \"%s\", file %s, line %u", #a, __FILE__, __LINE__); \
    } else {         \
        printk("."); \
    }


void test_strings();
void test_printf();
void test_strbuff();
void test_paths();


void run_tests() {
    printk("\nstrings");
    test_strings();

    printk("\nprintf");
    test_printf();

    printk("\nstrbuff");
    test_strbuff();

    printk("\npaths");
    test_paths();
}


void test_strings() {
    char buffer[64];
    char *str;

    // strlen
    assert(strlen("") == 0);
    assert(strlen("Hello") == 5);

    // strcmp
    assert(strcmp("", "") == 0);
    assert(strcmp("", "a") < 0);
    assert(strcmp("a", "") > 0);
    assert(strcmp("abc", "def") < 0);
    assert(strcmp("abc", "123") > 0);
    assert(strcmp("abc", "abc") == 0);
    
    // strcpy & strcmp ?
    buffer[0] = 0;
    strcpy(buffer, "123");
    assert(strcmp(buffer, "123") == 0);

    // memset & memcmp ?
    memset(buffer, 0xff, sizeof(buffer));
    strcpy(buffer + 1, "xyz");
    assert(memcmp(buffer, "\xffxyz\0\xff", 6) == 0);

    // strtok is used to parse commands in konsole
    strcpy(buffer, "Hello there, this is-a test... phrase!");
    str = " ,.!";
    assert(strcmp(strtok(buffer, str), "Hello") == 0);
    assert(strcmp(strtok(NULL, str), "there") == 0);
    assert(strcmp(strtok(NULL, str), "this") == 0);
    assert(strcmp(strtok(NULL, str), "is-a") == 0);
    assert(strcmp(strtok(NULL, str), "test") == 0);
    assert(strcmp(strtok(NULL, str), "phrase") == 0);
    assert(strtok(NULL, str) == NULL);
    // remember, strtok() alters the string in place nulling out all delimiters
    assert(memcmp(buffer, "Hello\0there\0\0this\0is-a\0test\0\0\0\0phrase\0", 38) == 0);

    // strchr()
    strcpy(buffer, "Hello there!");
    assert(strchr(buffer, 'o') == &buffer[4]);
    assert(strchr(buffer, 'T') == NULL);

    // memmove()
    strcpy(buffer,        "0123456789abcdefghijklmnopqrstuvwxyz");
    memmove(&buffer[5], &buffer[10], 10);
    assert(strcmp(buffer, "01234abcdefghijfghijklmnopqrstuvwxyz") == 0);
    strcpy(buffer,        "0123456789abcdefghijklmnopqrstuvwxyz");
    memmove(&buffer[10], &buffer[5], 10);
    assert(strcmp(buffer, "012345678956789abcdeklmnopqrstuvwxyz") == 0);

    // prove atoi()
    assert(atoi(NULL) == 0);
    assert(atoi("") == 0);
    assert(atoi("100") == 100);
    assert(atoi("-100") == -100);
    assert(atoi("0xff") == 255);
    assert(atoi("0xFF") == 255);
    assert(atoi("077") == 63);
    assert(atoi("b1001") == 9);
    assert(atoi("2147483647") == 2147483647);  // max signed int
    assert(atoi("2147483649") == -2147483647); // wraparound

    // unsigned version
    assert(atoui("100") == 100);
    assert(atoui("-100") == 0); // minus is not recognized
    assert(atoui("2147483647") == 2147483647u);  // max signed int
    assert(atoui("2147483649") == 2147483649u);
    assert(atoui("4294967295") == 4294967295u);  // max unsigned int

    // ltoa should work with ant 32bit signed value
    ltoa(0, buffer, 10);
    assert(strcmp(buffer, "0") == 0);
    ltoa(123, buffer, 10);
    assert(strcmp(buffer, "123") == 0);
    ltoa(-123, buffer, 10);
    assert(strcmp(buffer, "-123") == 0);
    ltoa(32, buffer, 16);
    assert(strcmp(buffer, "20") == 0);
    ltoa(32, buffer, 8);
    assert(strcmp(buffer, "40") == 0);
    ltoa(9, buffer, 2);
    assert(strcmp(buffer, "1001") == 0);

    // prove ltoa() overflows for values over half, and that ultoa() works
    ltoa(3000000000, buffer, 10);
    assert(strcmp(buffer, "-1294967296") == 0);
    ultoa(3000000000, buffer, 10);
    assert(strcmp(buffer, "3000000000") == 0);
    ultoa(4294967295, buffer, 10);
    assert(strcmp(buffer, "4294967295") == 0);
    ultoa(4294967295, buffer, 16);
    assert(strcmp(buffer, "FFFFFFFF") == 0);

    str = "once upon a time, a kernel was written";
    assert(strstr(str, NULL) == str);
    assert(strstr(str, "") == str);
    assert(strstr(str, "z") == NULL);
    assert(strstr(str, "o") == str);
    assert(strstr(str, "on a time") != NULL);
    assert(strstr(str, "on a time") == str + 7);
    assert(strstr(str, "on a time in the future") == NULL);
    assert(strstr(str, "written") != NULL);
    assert(strstr(str, "written too") == NULL);

    str = "writing tests";
    assert(memcmp(str, "", 0) == 0);
    assert(memcmp(str, "writ", 0) == 0);
    assert(memcmp(str, "writ", 4) == 0);
    assert(memcmp(str, "wri", 4) != 0); // zero terminator should not match
    assert(memcmp(str, "wrie", 4) != 0);
    assert(memcmp(str, "writing tests", strlen(str)) == 0);
    assert(memcmp(str, "writing tests again", strlen(str)) == 0);
    assert(memcmp(str, "writing tests again", strlen(str) + 1) != 0); // zero term

    str = "copying tests";
    memset(buffer, 0, sizeof(buffer));
    assert(buffer[0] == '\0');
    assert(buffer[sizeof(buffer) - 1] == '\0');
    memcpy(buffer, str, 0);
    assert(buffer[0] == '\0');

    memcpy(buffer + 1, str, 4);
    assert(memcmp(buffer, "\0copy\0", 6) == 0)
}


void test_printf() {
    char buffer[64];

    sprintfn(buffer, sizeof(buffer), "");
    assert(strlen(buffer) == 0);

    sprintfn(buffer, sizeof(buffer), "Hello");
    assert(strcmp(buffer, "Hello") == 0);

    // respect buffer size (include zero terminator)

    sprintfn(buffer, 5, "aaaaaaaaaaaaaaa");
    assert(strlen(buffer) == 4);

    sprintfn(buffer, 5, "%s", "aaaaaaaaaaaaaaa");
    assert(strlen(buffer) == 4);

    // escape percent sign

    sprintfn(buffer, sizeof(buffer), "Hello 100%%");
    assert(strcmp(buffer, "Hello 100%") == 0);

    // chars

    sprintfn(buffer, sizeof(buffer), "%c+%c", 'a', 'b');
    assert(strcmp(buffer, "a+b") == 0);

    // basic strings 

    sprintfn(buffer, sizeof(buffer), "%s", "Hello");
    assert(strcmp(buffer, "Hello") == 0);

    sprintfn(buffer, sizeof(buffer), "%10s", "Hello");
    assert(strcmp(buffer, "     Hello") == 0);

    sprintfn(buffer, sizeof(buffer), "%-10s", "Hello");
    assert(strcmp(buffer, "Hello     ") == 0);

    // basic numbers

    sprintfn(buffer, sizeof(buffer), "%d", 0);
    assert(strcmp(buffer, "0") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", 5);
    assert(strcmp(buffer, "5") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", -5);
    assert(strcmp(buffer, "-5") == 0);

    // padding 

    sprintfn(buffer, sizeof(buffer), "%3d", 5);
    assert(strcmp(buffer, "  5") == 0);

    sprintfn(buffer, sizeof(buffer), "%-3d", 5);
    assert(strcmp(buffer, "5  ") == 0);

    sprintfn(buffer, sizeof(buffer), "%03d", 5);
    assert(strcmp(buffer, "005") == 0);

    sprintfn(buffer, sizeof(buffer), "%-03d", 5);
    assert(strcmp(buffer, "500") == 0);

    // signed, unsigned int

    sprintfn(buffer, sizeof(buffer), "%i", -123); // %i equiv to %d
    assert(strcmp(buffer, "-123") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", -123);
    assert(strcmp(buffer, "-123") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", 2147483647); // max signed int
    assert(strcmp(buffer, "2147483647") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", 4294967295); // should not be supported
    assert(strcmp(buffer, "-1") == 0);

    sprintfn(buffer, sizeof(buffer), "%u", 2147483647);
    assert(strcmp(buffer, "2147483647") == 0);

    sprintfn(buffer, sizeof(buffer), "%u", 4294967295); // max unsigned int
    assert(strcmp(buffer, "4294967295") == 0);

    // hex, octal, binary

    sprintfn(buffer, sizeof(buffer), "%x", 0x12345678);
    assert(strcmp(buffer, "12345678") == 0);

    sprintfn(buffer, sizeof(buffer), "%o", 0777);
    assert(strcmp(buffer, "777") == 0);

    sprintfn(buffer, sizeof(buffer), "%b", 0x99);
    assert(strcmp(buffer, "10011001") == 0);

    // pointers

    void *p = (void *)0x12345678;
    sprintfn(buffer, sizeof(buffer), "%p", p);
    assert(strcmp(buffer, "12345678") == 0);

    p = (void *)0xFFFFFFFF;  // full 4GB address
    sprintfn(buffer, sizeof(buffer), "%p", p); 
    assert(strcmp(buffer, "FFFFFFFF") == 0);
}

void test_strbuff() {

    // test a fixed allocation buffer
    strbuff_t *sb = create_sb(32, SB_FIXED);
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
    assert(sb_strcmp(sb, "teabcdefgjkl") == 0)
    // test insert in the middle, overflowing left
    sb_strcpy(sb, "123456789012");
    sb_insert(sb, 5, "abc");
    assert(sb_strcmp(sb, "12345abc6789") == 0)


    // check SB_SCROLL behavior
    sb_free(sb);
    sb = create_sb(12, SB_SCROLL);
    sb_append(sb, "Monday,");
    assert(sb_strcmp(sb, "Monday,") == 0)
    sb_append(sb, "Tuesday,");
    assert(sb_strcmp(sb, "day,Tuesday,") == 0)
    sb_append(sb, "Wednesday,");
    assert(sb_strcmp(sb, "y,Wednesday,") == 0)
    sb_append(sb, "Thursday,");
    assert(sb_strcmp(sb, "ay,Thursday,") == 0)
    sb_append(sb, "Friday!");
    assert(sb_strcmp(sb, "sday,Friday!") == 0)
    sb_append(sb, "Very big buffer trying to test overflow behavior");
    assert(sb_strcmp(sb, "low behavior") == 0)


    // check SB_EXPAND mode
    sb = create_sb(12, SB_EXPAND);
    sb_append(sb, "Monday,");
    assert(sb_strcmp(sb, "Monday,") == 0)
    sb_append(sb, "Tuesday,");
    assert(sb_strcmp(sb, "Monday,Tuesday,") == 0)
    sb_append(sb, "Wednesday,");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,") == 0)
    sb_append(sb, "Thursday,");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,Thursday,") == 0)
    sb_append(sb, "Friday!");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,Thursday,Friday!") == 0)
    sb_insert(sb, 25, "(insert day here),");
    assert(sb_strcmp(sb, "Monday,Tuesday,Wednesday,(insert day here),Thursday,Friday!") == 0)
    sb_free(sb);
}


void test_paths() {

    assert(count_path_parts("") == 0);
    assert(count_path_parts("/") == 0);

    assert(count_path_parts("/abc") == 1);
    assert(count_path_parts("/abc/") == 1);
    assert(count_path_parts("abc/") == 1);

    assert(count_path_parts("abc/def") == 2);
    assert(count_path_parts("/abc/def") == 2);
    assert(count_path_parts("abc/def/") == 2);

    assert(count_path_parts("/use/share/lib/kernel/klibc.a") == 5);
    assert(count_path_parts("/home/even with spaces") == 2);

    char *path = "/usr/share/lib/kernel/klibc.a";
    assert(count_path_parts(path) == 5);

    char buffer[32];
    int offset = 0;
    int err;
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "usr") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "share") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "lib") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "kernel") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "klibc.a") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == ERR_NO_MORE_CONTENT);

    // now, specific ones
    err = get_n_index_path_part(path, 5, buffer);
    assert(err == ERR_NOT_FOUND);

    err = get_n_index_path_part(path, 4, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "klibc.a") == 0);

    err = get_n_index_path_part(path, 3, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "kernel") == 0);

    err = get_n_index_path_part(path, 2, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "lib") == 0);

    err = get_n_index_path_part(path, 1, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "share") == 0);

    err = get_n_index_path_part(path, 0, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "usr") == 0);
}