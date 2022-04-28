#define QUIT_CHAR 'q'
#define BRENAME_SCRIPT_PATH "/home/joseph/.cache/stuifm_brename.sh"
#define BRENAME_TXT_PATH "/home/joseph/.cache/stuifm_brename.txt"
#define VCD_PATH "/home/joseph/.vcd"


#define SELECTEDCOLOR  COLOR_RED
#define DIRECTORYCOLOR COLOR_BLUE

/* sort with directories being first ? */
#define DIRECTORIESFIRST 1
/* show hidden files ? */
#define HIDDENFILES 1
/* draw mode 0 is midnight commmander-like and draw mode 1 is ranger-like
 * the ratio is hard-coded to 1,1,2 and can't be changed unless you change the source code
 */
#define DRAWMODE 0
/* for drawmode 0, if you want to draw the elements in the middle or from the line beginning */
#define MIDDLE 1
/* if you are in draw mode 1, and preview is set to 1, then there will be a file
 * preview - right now it only works with text files
 * as of now, preview does not have syntax highlighting
 */
#define PREVIEW 1

/* example commands
 * the array must have one element (a string)
 */
static const char *c1[] = {"ls"};
static const char *c2[] = {"ls %"}; /* % means the current file */
static const char *c3[] = {"ls %p"}; /* %p means the current working directory */
static const char *c4[] = {"ls %s"}; /* %s means the selection */

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
    {'v' & CtrlMask, selectionmanager,      {0}      },
    {'a' & CtrlMask, selectall,             {0}      },

    /* draw mode */
    {'~',            drawmodeswitch,        {0}      },
	{'`',            middleswitch,          {0}      },
    {'~' & CtrlMask, previewswitch,         {0}      },
    
    /* sorting */
    {'-' & CtrlMask, directoriesfirst,      {0}      },
    {'h' & CtrlMask, hiddenfilesswitch,     {0}      },
    {'.',            hiddenfilesswitch,     {0}      },
                                  	     
    /* copy, move, rename, bulk rename and trash put */
    {'y',            copyfiles,             {.i = 0} }, /* 0 = ask if there is a file with the same name, 1 = always replace, -1 = never ask, never replace */
    {'d',            movefiles,             {.i = 0} }, /* 0 = ask if there is a file with the same name, 1 = always replace, -1 = never ask, never replace */
    {'c',            normrename,            {0}      },
    {'b',            brename,               {0}      },
    
    /* searching */
    {'/',            search,                {.i = 0} },
    {'n',            search,                {.i = +1}},
    {'N',            search,                {.i = -1}},
    
    /* commands */
    {'!',            executecommand,        {0}      },
    /* example commands */
    {'[',            executecommand,        {.v = c1}},
    {']',            executecommand,        {.v = c2}},
    {'(',            executecommand,        {.v = c3}},
    {')',            executecommand,        {.v = c4}},
};
