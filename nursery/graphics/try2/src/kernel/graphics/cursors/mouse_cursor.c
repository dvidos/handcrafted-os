#include "mouse_cursor.h"

/*
    Cursor encoding with ASCII macros
    Using two macros, one for AND and one for XOR, each producing a 32-bit row.
    AND=1, XOR=0 -> transparent
    AND=0, XOR=0 -> black
    AND=0, XOR=1 -> white
    AND=1, XOR=1 -> invert (unused)

    ' ' = transparent
    '#' = white (foreground)
    '.' = black (background)

    +------+-----+-----+-------------+
    | char | AND | XOR | meaning     |
    +------+-----+-----+-------------+
    | ' '  |  1  |  0  | transparent |
    | '#'  |  0  |  1  | white pixel |
    | '.'  |  0  |  0  | black pixel |
    +------+-----+-----+-------------+

*/

#define CURSOR_AND_MASK(sym) ( \
    (((sym)[ 0] == ' ' ? 1 : 0) << 31) | \
    (((sym)[ 1] == ' ' ? 1 : 0) << 30) | \
    (((sym)[ 2] == ' ' ? 1 : 0) << 29) | \
    (((sym)[ 3] == ' ' ? 1 : 0) << 28) | \
    (((sym)[ 4] == ' ' ? 1 : 0) << 27) | \
    (((sym)[ 5] == ' ' ? 1 : 0) << 26) | \
    (((sym)[ 6] == ' ' ? 1 : 0) << 25) | \
    (((sym)[ 7] == ' ' ? 1 : 0) << 24) | \
    (((sym)[ 8] == ' ' ? 1 : 0) << 23) | \
    (((sym)[ 9] == ' ' ? 1 : 0) << 22) | \
    (((sym)[10] == ' ' ? 1 : 0) << 21) | \
    (((sym)[11] == ' ' ? 1 : 0) << 20) | \
    (((sym)[12] == ' ' ? 1 : 0) << 19) | \
    (((sym)[13] == ' ' ? 1 : 0) << 18) | \
    (((sym)[14] == ' ' ? 1 : 0) << 17) | \
    (((sym)[15] == ' ' ? 1 : 0) << 16) | \
    (((sym)[16] == ' ' ? 1 : 0) << 15) | \
    (((sym)[17] == ' ' ? 1 : 0) << 14) | \
    (((sym)[18] == ' ' ? 1 : 0) << 13) | \
    (((sym)[19] == ' ' ? 1 : 0) << 12) | \
    (((sym)[20] == ' ' ? 1 : 0) << 11) | \
    (((sym)[21] == ' ' ? 1 : 0) << 10) | \
    (((sym)[22] == ' ' ? 1 : 0) <<  9) | \
    (((sym)[23] == ' ' ? 1 : 0) <<  8) | \
    (((sym)[24] == ' ' ? 1 : 0) <<  7) | \
    (((sym)[25] == ' ' ? 1 : 0) <<  6) | \
    (((sym)[26] == ' ' ? 1 : 0) <<  5) | \
    (((sym)[27] == ' ' ? 1 : 0) <<  4) | \
    (((sym)[28] == ' ' ? 1 : 0) <<  3) | \
    (((sym)[29] == ' ' ? 1 : 0) <<  2) | \
    (((sym)[30] == ' ' ? 1 : 0) <<  1) | \
    (((sym)[31] == ' ' ? 1 : 0) <<  0))

#define CURSOR_XOR_MASK(sym) ( \
    (((sym)[ 0] == '#' ? 1 : 0) << 31) | \
    (((sym)[ 1] == '#' ? 1 : 0) << 30) | \
    (((sym)[ 2] == '#' ? 1 : 0) << 29) | \
    (((sym)[ 3] == '#' ? 1 : 0) << 28) | \
    (((sym)[ 4] == '#' ? 1 : 0) << 27) | \
    (((sym)[ 5] == '#' ? 1 : 0) << 26) | \
    (((sym)[ 6] == '#' ? 1 : 0) << 25) | \
    (((sym)[ 7] == '#' ? 1 : 0) << 24) | \
    (((sym)[ 8] == '#' ? 1 : 0) << 23) | \
    (((sym)[ 9] == '#' ? 1 : 0) << 22) | \
    (((sym)[10] == '#' ? 1 : 0) << 21) | \
    (((sym)[11] == '#' ? 1 : 0) << 20) | \
    (((sym)[12] == '#' ? 1 : 0) << 19) | \
    (((sym)[13] == '#' ? 1 : 0) << 18) | \
    (((sym)[14] == '#' ? 1 : 0) << 17) | \
    (((sym)[15] == '#' ? 1 : 0) << 16) | \
    (((sym)[16] == '#' ? 1 : 0) << 15) | \
    (((sym)[17] == '#' ? 1 : 0) << 14) | \
    (((sym)[18] == '#' ? 1 : 0) << 13) | \
    (((sym)[19] == '#' ? 1 : 0) << 12) | \
    (((sym)[20] == '#' ? 1 : 0) << 11) | \
    (((sym)[21] == '#' ? 1 : 0) << 10) | \
    (((sym)[22] == '#' ? 1 : 0) <<  9) | \
    (((sym)[23] == '#' ? 1 : 0) <<  8) | \
    (((sym)[24] == '#' ? 1 : 0) <<  7) | \
    (((sym)[25] == '#' ? 1 : 0) <<  6) | \
    (((sym)[26] == '#' ? 1 : 0) <<  5) | \
    (((sym)[27] == '#' ? 1 : 0) <<  4) | \
    (((sym)[28] == '#' ? 1 : 0) <<  3) | \
    (((sym)[29] == '#' ? 1 : 0) <<  2) | \
    (((sym)[30] == '#' ? 1 : 0) <<  1) | \
    (((sym)[31] == '#' ? 1 : 0) <<  0)) 

const cursor32 triangle_cursor = {
    .and_mask = {
        CURSOR_AND_MASK("##                              "),
        CURSOR_AND_MASK("#.#                             "),
        CURSOR_AND_MASK("#..#                            "),
        CURSOR_AND_MASK("#...#                           "),
        CURSOR_AND_MASK("#....#                          "),
        CURSOR_AND_MASK("#.....#                         "),
        CURSOR_AND_MASK("#......#                        "),
        CURSOR_AND_MASK("#.......#                       "),
        CURSOR_AND_MASK("#........#                      "),
        CURSOR_AND_MASK("#.........#                     "),
        CURSOR_AND_MASK("#..........#                    "),
        CURSOR_AND_MASK("#...........#                   "),
        CURSOR_AND_MASK("#............#                  "),
        CURSOR_AND_MASK("#.............#                 "),
        CURSOR_AND_MASK("#..............#                "),
        CURSOR_AND_MASK("#...............#               "),
        CURSOR_AND_MASK("#................#              "),
        CURSOR_AND_MASK("#.............###               "),
        CURSOR_AND_MASK("#..........###                  "),
        CURSOR_AND_MASK("#.......###                     "),
        CURSOR_AND_MASK("#....###                        "),
        CURSOR_AND_MASK("#.###                           "),
        CURSOR_AND_MASK("##                              "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        /* rest blank */
    },
    .xor_mask = {
        CURSOR_XOR_MASK("##                              "),
        CURSOR_XOR_MASK("# #                             "),
        CURSOR_XOR_MASK("#  #                            "),
        CURSOR_XOR_MASK("#   #                           "),
        CURSOR_XOR_MASK("#    #                          "),
        CURSOR_XOR_MASK("#     #                         "),
        CURSOR_XOR_MASK("#      #                        "),
        CURSOR_XOR_MASK("#       #                       "),
        CURSOR_XOR_MASK("#        #                      "),
        CURSOR_XOR_MASK("#         #                     "),
        CURSOR_XOR_MASK("#          #                    "),
        CURSOR_XOR_MASK("#           #                   "),
        CURSOR_XOR_MASK("#            #                  "),
        CURSOR_XOR_MASK("#             #                 "),
        CURSOR_XOR_MASK("#              #                "),
        CURSOR_XOR_MASK("#               #               "),
        CURSOR_XOR_MASK("#                #              "),
        CURSOR_XOR_MASK("#             ###               "),
        CURSOR_XOR_MASK("#          ###                  "),
        CURSOR_XOR_MASK("#       ###                     "),
        CURSOR_XOR_MASK("#    ###                        "),
        CURSOR_XOR_MASK("# ###                           "),
        CURSOR_XOR_MASK("##                              "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
    },
    .hot_x = 0,
    .hot_y = 0
};

const cursor32 windows_cursor = {
    .and_mask = {
        CURSOR_AND_MASK(".                               "),
        CURSOR_AND_MASK("..                              "),
        CURSOR_AND_MASK(".#.                             "),
        CURSOR_AND_MASK(".##.                            "),
        CURSOR_AND_MASK(".###.                           "),
        CURSOR_AND_MASK(".####.                          "),
        CURSOR_AND_MASK(".#####.                         "),
        CURSOR_AND_MASK(".######.                        "),
        CURSOR_AND_MASK(".#######.                       "),
        CURSOR_AND_MASK(".########.                      "),
        CURSOR_AND_MASK(".#########.                     "),
        CURSOR_AND_MASK(".##########.                    "),
        CURSOR_AND_MASK(".###########.                   "),
        CURSOR_AND_MASK(".######......                   "),
        CURSOR_AND_MASK(".###.##.                        "),
        CURSOR_AND_MASK(".##. .##.                       "),
        CURSOR_AND_MASK(".#.  .##.                       "),
        CURSOR_AND_MASK("..    .##.                      "),
        CURSOR_AND_MASK("      .##.                      "),
        CURSOR_AND_MASK("       ..                       "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        CURSOR_AND_MASK("                                "),
        /* rest blank */
    },
    .xor_mask = {
        CURSOR_XOR_MASK(".                               "),
        CURSOR_XOR_MASK("..                              "),
        CURSOR_XOR_MASK(".#.                             "),
        CURSOR_XOR_MASK(".##.                            "),
        CURSOR_XOR_MASK(".###.                           "),
        CURSOR_XOR_MASK(".####.                          "),
        CURSOR_XOR_MASK(".#####.                         "),
        CURSOR_XOR_MASK(".######.                        "),
        CURSOR_XOR_MASK(".#######.                       "),
        CURSOR_XOR_MASK(".########.                      "),
        CURSOR_XOR_MASK(".#########.                     "),
        CURSOR_XOR_MASK(".##########.                    "),
        CURSOR_XOR_MASK(".###########.                   "),
        CURSOR_XOR_MASK(".######......                   "),
        CURSOR_XOR_MASK(".###.##.                        "),
        CURSOR_XOR_MASK(".##. .##.                       "),
        CURSOR_XOR_MASK(".#.  .##.                       "),
        CURSOR_XOR_MASK("..    .##.                      "),
        CURSOR_XOR_MASK("      .##.                      "),
        CURSOR_XOR_MASK("       ..                       "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
        CURSOR_XOR_MASK("                                "),
    },
    .hot_x = 0,
    .hot_y = 0
};

