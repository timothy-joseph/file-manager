#define QUIT_CHAR 'q'
/* TODO: use /tmp for these */
#define BRENAME_SCRIPT_PATH "/home/joseph/.cache/stuifm_brename.sh"
#define BRENAME_TXT_PATH "/home/joseph/.cache/stuifm_brename.txt"
#define VCD_PATH "/home/joseph/.vcd"

#define SELECTEDCOLOR  COLOR_RED
#define DIRECTORYCOLOR COLOR_BLUE

/* sort with directories being first ? */
#define DIRECTORIESFIRST 1
/* show hidden files ? */
#define HIDDENFILES 1

/* example commands
 * the array must have one element (a string)
 */


/* NoEndWin -> don't end the curses section when executing commands
 * NoReloadMask -> don't update the elements in the current file list
 * CdLastLineMask -> chdir to the last line of the output of the command
 * SearchLastLineMask -> searches using the pattern showed on the last line of the output of the command
 * NoSaveSearchMask -> doesn't save the pattern used with the SearchLastLineMask
 *
 * if the NoEndWin mask includes the NoConfirmationMask because you can't ask for input
 * i'm considering removing the confirmation part of the code becuase you can make your scripts ask for the confirmation
 *
 * in order to use multiple just put a bitwise or between them (eg: .i = NoConfirmationMask|NoReloadMask)
 * 
 * if you want to use none of the masks, initialize .i to 0, otherwise it will be undefined and it might be filled with junk
 * ----
 * this is how commands are defined - don't use ~ or $HOME, just pot /home/user/
 * ----
 * the format characters are:
 * %c -> the current file -> echo %c becomes echo currentname
 * %C -> the current file with its full path (TODO: coming soon) -> echo %C becomes echo /path/to/current
 * %s -> the files in the selection with their full path -> echo %s becomes echo /path/to/file1 /path/to/file2
 * %S -> the files in the selection with their full path, but it executes separate commands -> echo %S becomes echo /path/to/file1 and then echo/path/to/file2
 * %% put a single %
 */
static const char *renamecommand[] = {"printf \"rename %%s: \" \"%c\"; read ans; mv -i -v %c $ans; echo $ans"};
static const char *yankcommand[] = {"printf \"yank selection[y/N]: \"; read ans; [ $ans = \"y\" ] && cp -r -i -v %s $(pwd); echo %c"};
static const char *movecommand[] = {"printf \"move selection[y/N]: \"; read ans; [ $ans = \"y\" ] && mv -i -v %s $(pwd); echo %c"};

/* TODO: make a list of commands to autostart, a list of commands to run before every event (not just defined events) and after every event. by event i mean keypress */
/* TODO: columns array of ratios */
/* TODO: bookmark script */
/* TODO: add the last line output things */

static Key keys[] = {
    /* movement */
    {'j',            movev,                 {.i = +1}},
    {'k',            movev,                 {.i = -1}},
    {'h',            moveh,                 {.i = -1}},
    {'l',            moveh,                 {.i = +1}},
    {'d' & CtrlMask, movev,                 {.i = +2}},
    {'u' & CtrlMask, movev,                 {.i = -2}},
    {'g',            first,                 {0}      },
    {'G',            last,                  {0}      },
    {'e' & CtrlMask, topofscreenscroll,     {.i = +1}},
    {'y' & CtrlMask, topofscreenscroll,     {.i = -1}},
    
    /* selection */
    {'v',            selection,             {0}      },
    {'V',            clearselection,        {0}      },
    {'a' & CtrlMask, selectall,             {0}      },
    
    /* sorting */
    {'-',            directoriesfirst,      {0}      },
    {'h' & CtrlMask, hiddenfilesswitch,     {0}      },
    {'.',            hiddenfilesswitch,     {0}      },
                                  	     
	/* rename, bulkrename, yank, move and trash-put using commmands */
    {'c',            executecommand,        {.v = renamecommand, .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'y',            executecommand,        {.v = yankcommand, .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'y',            clearselection,        {0}      }, /* in order to clear the selection after yank - might consider making this a mask */
    {'d',            executecommand,        {.v = movecommand, .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'d',            clearselection,        {0}      }, /* in order to clear the selection after copy - might consider making this a mask */
    {'b',            brename,               {0}      }, /* TODO: replace brename with a shellscript */
    
    /* searching */
    {'/',            search,                {.i = 0} },
    {'n',            search,                {.i = +1}},
    {'N',            search,                {.i = -1}},
    
    /* commands */
    {'!',            executecommand,        {0}      },
    /* example commands */
};
