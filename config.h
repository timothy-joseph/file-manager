#define QUIT_CHAR 'q'
#define BRENAME_SCRIPT_PATH "/home/joseph/.cache/stuifm_brename.sh"
#define BRENAME_TXT_PATH "/home/joseph/.cache/stuifm_brename.txt"
#define VCD_PATH "/home/joseph/.vcd"

#define CtrlMask 0x1F

#define DIRECTORIESFIRST 1
#define SELECTEDCOLOR  COLOR_RED
#define DIRECTORYCOLOR COLOR_BLUE

static Key keys[] = {
	{'j',            movev,                 {.i = +1}},
	{'k',            movev,                 {.i = -1}},
	{'h',            moveh,                 {.i = -1}},
	{'l',            moveh,                 {.i = +1}},

	{'v',            selection,             {0}      },
	{'V',            clearselection,        {0}      },
	{'v' & CtrlMask, selectionmanager,      {0}      },
                                  	     
	{'y',            copyfiles,             {0}      },
	{'d',            movefiles,             {0}      },
	{'c',            normrename,            {0}      },
	{'b',            brename,               {0}      },
	{'-',            directoriesfirst,      {0}      },
	{'/',            search,                {0}      }
};
