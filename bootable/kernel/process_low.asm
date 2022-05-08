
; somehow this method will allow context switching
; it takes two pointers to two structures:
; the old context and the new context.
; it saves the context it was called from into the old context
; it restores the new context onto the stack (including SS)
; therefore, return will look like far jumping to the new context.
; our call to this function will look like blocked, 
; until someone puts our old context as their new context
;
; vx6 maintains a context per process and a context for the scheduler,
; ping-ponging between them as needed
;
; for example, in an interrupt, we may yield() to schedule another process
; yield() calls schedule(), which calls switch_context(), to allow it to 
; save the context of the "yield" caller, and "return" to the scheduler's context
; inversely, the scheduler will "switch_context" between itself and the process it chose
; to execute.
; 
; essentially, from the perspective of a process, calling "switch_context"
; (through any syscall method) blocks until the result is returned.
; from the perspective of a scheduler, it calls "switch_context" to a process
; and it blocks until the process gives up control or is sleeping.
;
; see https://github.com/mit-pdos/xv6-public/blob/master/swtch.S as well
switch_context:
    ; do some magic
    ret




