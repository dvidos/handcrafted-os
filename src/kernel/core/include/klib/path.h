#ifndef _PATH_H
#define _PATH_H


// copies the next path component to buffer, advances offset.
int get_next_path_part(char *path, int *offset, char *buffer);

// returns how many parts in the path. leading and trailing '/' are ignored
int count_path_parts(char *path);

// gets the path part of index n
int get_n_index_path_part(char *path, int n, char *buffer);


#endif
