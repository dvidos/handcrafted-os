/**
 * When we execute a file,
 * 
 * we shall load and parse the ELF header,
 * we shall allocate physical memory for the code, data, bss, heap, stack areas
 * we shall load the various areas into memory,
 * we shall create a stack entry, by pushing the argv[] and maybe the environment
 * (as if someone had called the main function)
 * we shall put this process into the ready queue, for next scheduling.
 * 
 * Notice, when we go to load the file, there's an opportunity for a shebang.
 * If the first two bytes are "#!" and a valid program follows (e.g. /bin/python)
 * then we can execute the program, passing the filename as the first argument.
 * 
 * Also notice that exec() does not allocate a new process, this is done via fork() only.
 * I do not remember why this is, but I read the reason in one of the pdfs.
 */