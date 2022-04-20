# STUIFM is a simple terminal user interface file manager
stuifm uses ncurses

stuifm shows the files on the middle of the terminal window (vertically) alligned to the right. the current file is that which has a '<' to the right of it. selected files are those that have a '>' as the first character of their line and are highlighted with a red background colour. directories have a '/' at the end of their name and are coloured with a blue foreground, all other files are colored with a white foreground

## key bindings
### movement
j - move down \
k - move up \
h - move to the previous directory ('..') \
l - move to the directory pointer to by the current file (it must be a directory; if it isn't a directory, the program will do nothing as i haven't programmed it to open normal files) \
Ctrl + d move down half a page \
Ctrl + u move up half a page \
g - go to the first element in the directory \
G - go to the last element in the directory \
Ctrl + e - move the top of the screen down with one line without moving the cursor (current file) \
Ctrl + y - mvoe the top of the screen up with one line without moving the cursor (current file)

### selection
v - add or remove from the selection \
V - clear the selection \
Ctrl + v - show all of the selection \
Ctrl + a - put all of the files in the current directory to the selection (doesn't reset the selection first)

the selection doesn't clear itself when you change directory (you can select multiple files from multiple directories)


### hidden files and sorting by directories first
\- switch from sorting with the directories being first and normal alphabetical order \
Ctrl + h - switch from showing and hiding hidden files (files that start with '.') \
. - switch from showing and hiding hidden files (files that start with '.')

### file management
y - copy all of the files from the selection to the current directory (selection must not be empty) \
d - move all of the files from the selection to the current directory (selection must not be empty) \
c - rename the current file \
b - bulk rename the files from the selection (selection must not be empty)

### searching
/ - will ask for you to input a pattern and will match the first ellement according to that pattern \
n - will redo the last search with the last pattern starting from the next element \
N - will redo the last search but in the opposite direction starting from the previous element

### executing a command
! - will ask for you to input a command and then will ask for confirmation if you want to execute it. put a % in the command to substitute it with the name of the current file, a %p to substitute it with the current working directory and a %s to substitute it with all of the elements in the selection - it does not clear the selection afterwars, even if the command got rid of them/renamed them/removed them

you can define your own commands to be executed like the examples in config.h

```c
static const char *command[] = {"your command here, only one element in this array (it can include spaces and the substitute characters)"}
```
and in the keys array:
```c
{chr,        executecommand,      {.v = command}},
```
where chr is your desired character. you can also make it so it works with the control key like this ```chr & CtrlMask```

## install:
before installation edit the config.h macros about path to ones that match your needs. also keep in mind if you want to use the program with multiple users, you'll need to give all of the users read and write access to those paths (at least the bulk rename paths) \
for bulk rename to work, you'll need to first create those files pointed to by the macros and chmod +x the script one

all of the macros should have pretty suggestive names

download one of the release or the latest master, cd into that directory and run:
```sudo make install```

### how to use it as a way visually and dynamically change directory
add the following to your .bashrc, .zsh etc
```sh
alias fm='stuifm; LASTDIR=`cat $HOME/.vcd`; cd "$LASTDIR"'
```
and start the program with ```fm```
