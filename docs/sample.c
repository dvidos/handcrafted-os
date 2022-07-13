
// memory management
void *malloc(int size);
void free(void *ptr);
void *sbrk(int diff); // allocate virtual space at the end of the process

// file manipulation
int open(char *file, int flags);
int read(int h, char *buffer, int size);
int write(int h, char *buffer, int size);
int close(int h);
int seek(int h, int pos, int origin);

// dir manipulation
struct direntry;
int opendir(char *path);
int readdir(int h, struct direntry *entry);
int closedir(int h);

// file info, creation, deletion
struct stat;
int stat(char *path, struct stat *stat);
int touch(char *path);
int mkdir(char *path);
int unlink(char *path);

// process manipulation
int getpid();
int getppid();
int fork();
int exec(char *file);
int wait(int pid);
int sleep(int msecs);
int exit(int err);

// inter-process communication
typedef struct msg msg;
int send(int to, msg *msg);
int receive(int from, msg *msg);
int sendreceive(int to, msg *msg);
int notify(int who, msg *msg);

// console (per process tty) manipulation
int printf(char *format, ...);
int getchar();
int putchar(int c);
int wherexy();
int gotoxy(int xy);
int textcolor(int color);
int clrscr();

// clock information
typedef struct dtime dtime;
int datetime(dtime *dtime);
unsigned long long uptime();

// socket manipulation?



/* possible data structures and algorithms in library
    - good, safe string class, 
    - good, multibyte string class (ucs2 / utf16)
    - list / collection functionality
    - hashtable functionality
    - sorting functionality
    - json manipulating functionality
*/
 


// how about beyond just terminal apps...
// what about setting programs to run at intervals, or in response to events or ... 
// how about instead of manipulating files, we manipulate message processors,
// for example filters and routers and splitters and aggregators.
// then pipe them to a web stream (say... twitter, or email)
// something as universal as copy/paste, allow automatic transfer of data
// what about easy ways of writing graphical games or mathematical equations to test them?
// why should we have a file system, but not a 3D engine with surround sound?
// why should console be the main UI and not... a book, or a joystick?
// why should we write imperative programs and not... statements?
// we could have libraries for exploiting various things
// data structures: dictionaries, hash sets, lists, etc
// mathematics, geographic or astronomic calculations...
// creating an editor and a compiler is fun
// i'd like to have the simplicity of early unix...



int main() {

    // what can a program do?

    // manipulate stdin / stdout (not just bytes or lines, but maybe json objects)
    // manipulate processes (fork, wait, change priority, etc)
    // manipulate files (how about databases?)
    // manipulate terminal or graphical window
    // manipulate inter-process messages (e.g. send or wait for messages)
    // manipulate network?

    // how to make an OS useful today? similar to what Unix did, back in the day
    // allow communication?
}
