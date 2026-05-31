#include "process.h"
#include "../../utils/assert.h"
#include "../../klib/string.h"
#include "../../memory/kheap.h"
#include "../../filesys/vfs_api.h"
#include "../../filesys/vfs_objects/mount_table.h"

MODULE("PROC_IPC", LOG_LEVEL_WARN);

static inline bool message_queue_is_empty(process_t *proc) {
    return proc->ipc_messages_count == 0;
}

static inline bool message_queue_is_full(process_t *proc) {
    return proc->ipc_messages_count >= MAX_IPC_MESSAGES;
}

static inline void append_message_to_queue(process_t *proc, ipc_message_t *msg) {
    ASSERT(!message_queue_is_full(proc));
    memcpy(&proc->ipc_messages[proc->ipc_messages_count], msg, sizeof(ipc_message_t));
    proc->ipc_messages_count++;
}

static inline void extract_message_from_queue(process_t *proc, ipc_message_t *msg) {
    ASSERT(!message_queue_is_empty(proc));

    memcpy(&msg, &proc->ipc_messages[0], sizeof(ipc_message_t));
    proc->ipc_messages_count--;

    // shift the rest up
    memcpy(&proc->ipc_messages[0], &proc->ipc_messages[1], proc->ipc_messages_count * sizeof(ipc_message_t));
}


static process_t *find_proc_by_pid(pid_t pid) {
    // proclist_find(pid) for all proc lists.
    // but some time we'd need like a hash or tree of processes, to find it fast.
    // maybe built into the process_t itself.

    return NULL;
}

// ---------------------------------------------

error_t proc_send(process_t *proc, pid_t target_pid, ipc_message_t message) {
    // enqueue into target's message queue, block until they have space
    ASSERT(proc != NULL);
    ASSERT(target_pid > 0);

    process_t *recipient = find_proc_by_pid(target_pid);
    if (recipient == NULL)
        return ERR_NOT_FOUND;

    while (true) {
        if (!message_queue_is_full(recipient)) {
            append_message_to_queue(recipient, &message);
            return OK;
        }
        
        // sleep on the queue having space
        proc_block(proc, IPC_WAIT_SEND, recipient);
    }
}

error_t proc_receive(process_t *proc, pid_t *sender_pid, ipc_message_t *msg) {
    // return message from queue, block until there is a message
    ASSERT(proc != NULL);
    ASSERT(sender_pid != NULL);
    ASSERT(msg != NULL);

    while (true) {
        if (!message_queue_is_empty(proc)) {
            extract_message_from_queue(proc, msg);

            // wake any possible senders waiting to send to us
            unblock_process_that(IPC_WAIT_SEND, proc);
            return OK;
        }

        // sleep on anybody sending something
        proc_block(proc, IPC_RECEIVE, NULL);
    }
}

error_t proc_send_receive(process_t *proc, pid_t target_pid, ipc_message_t *message) {
    // in one syscall, send a message, sleep on reply, extract reply
    // callee assumed to use reply()
    // wake up from reply, return to caller

    ASSERT(proc != NULL);
    ASSERT(target_pid > 0);
    ASSERT(message != NULL);

    process_t *recipient = find_proc_by_pid(target_pid);
    if (recipient == NULL)
        return ERR_NOT_FOUND;

    // first send the message
    while (true) {
        if (!message_queue_is_full(recipient)) {
            append_message_to_queue(recipient, message);
            break;
        }

        // sleep on the queue having space
        proc_block(proc, IPC_WAIT_SEND, recipient);
    }

    // set message place holder and wait for reply
    proc->ipc_message_reply_ptr = message;
    proc_block(proc, IPC_WAIT_REPLY, recipient);
    proc->ipc_message_reply_ptr = NULL;
    return OK;
}

error_t proc_reply(process_t *proc, pid_t target_pid, ipc_message_t *response) {
    // works on send_receive() calls, allows server to immediately answer to sender, without send()
    // if the sender is not ready, an error is returned to the server

    ASSERT(proc != NULL);
    ASSERT(target_pid > 0);
    ASSERT(response != NULL);

    process_t *recipient = find_proc_by_pid(target_pid);
    if (recipient == NULL)
        return ERR_NOT_FOUND;

    bool waiting_reply_from_us = 
        recipient->state == BLOCKED && 
        recipient->block_reason == IPC_WAIT_REPLY && 
        recipient->block_channel == proc &&
        recipient->ipc_message_reply_ptr != NULL;
    if (!waiting_reply_from_us)
        return ERR_RECIPIENT_NOT_READY;

    memcpy(recipient->ipc_message_reply_ptr, response, sizeof(ipc_message_t));
    unblock_process_that(IPC_WAIT_REPLY, proc);
    proc_yield(proc);

    return OK;
}
