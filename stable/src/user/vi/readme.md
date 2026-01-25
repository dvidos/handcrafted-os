
vi editor, written from scratch (is that even useful?)

Overview of some ideas about this:

* We'll create and work on a `buffer` object for all text editing functionality
* We'll create and work on a `viewport` object, for all screen drawing functionality (for the buffer part, not status lines)

Not to be supported:

* regex
* unicode
* settings
* syntax highlighting

Things to consider:

* one function to translate keys to commands
* command parser to invoke the relevant methods in the relevant objects
* file opening, saving, etc
* we want to support find, visual, yank, paste, number of commands
* we will have limited size of file to load (e.g. up to 1 MB)
* one unified way of editing the command line (for various commands)

Success definition:

* Being able to edit a file quickly, be it code or configuration


