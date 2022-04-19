#define QUIT_CHAR 'q'
#define BRENAME_SCRIPT_PATH "/home/joseph/.cache/stuifm_brename.sh"
#define BRENAME_TXT_PATH "/home/joseph/.cache/stuifm_brename.txt"
#define VCD_PATH "/home/joseph/.vcd"

#define CtrlMask 0x1F

#define DIRECTORIESFIRST 1
#define HIDDENFILES 1
#define SELECTEDCOLOR  COLOR_RED
#define DIRECTORYCOLOR COLOR_BLUE

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

	/* sorting */
	{'-',            directoriesfirst,      {0}      },
	{'h' & CtrlMask, hiddenfilesswitch,     {0}      },
	{'.',            hiddenfilesswitch,     {0}      },
                                  	     
	/* copy, move, rename, bulk rename and trash put */
	{'y',            copyfiles,             {.i = 0} }, /* 0 = ask if there is a file with the same name, 1 = always replace, -1 = never ask, never replace */
	{'d',            movefiles,             {.i = 0} }, /* 0 = ask if there is a file with the same name, 1 = always replace, -1 = never ask, never replace */
	{'d',            trashput,              {0}      },
	{'c',            normrename,            {0}      },
	{'b',            brename,               {0}      },

	/* searching */
	{'/',            search,                {.i = 0} },
	{'n',            search,                {.i = +1}},
	{'N',            search,                {.i = -1}}
};
