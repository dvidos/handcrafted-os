
struct circular_buffer {
    char *buffer;
    int total_size;
    int data_size; // tell totally full from totally empty
    int read_pos;
    int write_pos;
};

typedef struct circular_buffer circular_t;

// allocate and return a buffer
circular_t *circ_create(int size);

// frees both buffer and structure
void circ_free(circular_t *circ);

// clears the buffer from any data
void circ_clear_data(circular_t *circ);

// returns the bytes written in the buffer
int circ_data_size(circular_t *circ);

// returns the bytes available in the buffer
int circ_free_size(circular_t *circ);

// return number of bytes written, zero if none
int circ_write(circular_t *circ, char *buffer, int len);

// return number of bytes read, zero if none
int circ_read(circular_t *circ, char *buffer, int len);
