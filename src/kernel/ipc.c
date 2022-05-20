#include <stddef.h>

struct ipc_message {
    int call_no;
    union {
        struct {
            int p1;
            int p2;
            int p3;
        } a;
        struct {
            int operation;
            void *buffer;
            size_t len;
        } b;
    };
};
typedef struct ipc_message ipc_message_t;

void send(int receiver_pid, ipc_message_t *message) {
    // send message, block if receiver cannot receive it
}
void receive(int sender_pid, ipc_message_t *message) {
    // block until there is a message from target
    // unblock when message received
}
void sendrec(int receiver_pid, ipc_message_t *message) {
    // send and receive
    // block until answer is given
}
void notify(int receiver_pid, ipc_message_t *message) {
    // notify sender, do not block
}

