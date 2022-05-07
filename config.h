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

/* the draw ratios - similar to the ratios of ranger
 * the ratios themselves are defined like this:
 * {ratios, 0, currentposition} - where currentposition is the position of the column that contains the currentfile - 0 indexed
 */

static const int drawratios[][COLUMNS_MAX] = {
	//{1, 1, 0, 0},
	{1, 3, 4, 0, 1},
	//{1, 0, 0},
	{1, 1, 1, 1, 1, 0, 2}
};


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
 * %s -> the files in the selection with their full path -> echo %s becomes echo /path/to/file1 /path/to/file2
 * %% put a single %
 */
static const char *renamecommand[] = {"printf \"rename %%s: \" \"%c\"; read ans; mv -i -v %c $ans; echo $ans"};
static const char *yankcommand[] = {"printf \"yank selection[y/N]: \"; read ans; [ $ans = \"y\" ] && cp -r -i -v %s $(pwd); echo %c"};
static const char *movecommand[] = {"printf \"move selection[y/N]: \"; read ans; [ $ans = \"y\" ] && mv -i -v %s $(pwd); echo %c"};
static const char *trashputcommand[] = {"printf \"put to trash selection[y/N]: \"; read ans; [ $ans = \"y\" ] && trash-put %s; echo %c"};

/* TODO: make a list of commands to autostart, a list of commands to run before every event (not just defined events) and after every event. by event i mean keypress */
/* TODO: bookmark script */
/* TODO: add the last line output things */

/* WARNING:
 * 'n' & CtrlMask is the same thing as 'N' & CtrlMask
 * some combinations that don't work (that i've found):
 * 'm' & CtrlMask
 * 's' & CtrlMask
 * - i don't recommend you use the CtrlMask in your config unless it's 100% necessary
 */

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

	/* draw ratio */
	{'r'           , drawratiosscroll,      {.i = +1}},
	{'R'           , drawratiosscroll,      {.i = -1}},
    
    /* selection */
    {'v',            toggleselect,          {0}      },
    {'V',            clearselection,        {0}      },
    {'a' & CtrlMask, selectall,             {0}      },
    
    /* sorting */
    {'-',            directoriesfirst,      {0}      },
    {'h' & CtrlMask, hiddenfilesswitch,     {0}      },
    {'.',            hiddenfilesswitch,     {0}      },
                                  	     
	/* rename, bulkrename, yank, move and trash-put using commmands */
    {'c',            executecommand,        {.v = renamecommand,   .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'y',            executecommand,        {.v = yankcommand,     .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'y',            clearselection,        {0}      }, /* in order to clear the selection after yank - might consider making this a mask */
    {'d',            executecommand,        {.v = movecommand,     .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'d',            clearselection,        {0}      }, /* in order to clear the selection after copy - might consider making this a mask */
    {'D',            executecommand,        {.v = trashputcommand, .i=NoConfirmationMask|SearchLastLineMask|NoSaveSearchMask}},
    {'d',            clearselection,        {0}      }, /* in order to clear the selection after copy - might consider making this a mask */
    //{'b',            brename,               {0}      }, /* TODO: replace brename with a shellscript */
    
    /* searching */
    {'/',            search,                {.i = 0} },
    {'n',            search,                {.i = +1}},
    {'N',            search,                {.i = -1}},
    
    /* commands */
    {'!',            executecommand,        {.i = 0} }
};
