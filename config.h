#define QUIT_CHAR 'q'
#define BRENAME_SCRIPT_PATH "/home/joseph/.cache/stuifm_brename.sh"
#define BRENAME_TXT_PATH "/home/joseph/.cache/stuifm_brename.txt"
#define VCD_PATH "/home/joseph/.vcd"

static Key keys[] = {
	{'j', 		movev, 		     {.i = +1}},
	{'k', 		movev, 		     {.i = -1}},
	{'h', 		moveh, 		     {.i = -1}},
	{'l', 		moveh, 		     {.i = +1}},

	{'v', 		selection,       {0}      },
	{'V', 		clearselection,  {0}      },
                                      
	{'y', 		copyfiles, 	     {0}	  },
	{'d', 		movefiles, 	     {0}	  },
	{'c', 		normrename,	     {0}	  },
	{'b', 		brename,	     {0}	  },
	{'/', 		search,	     	 {0}	  }
};
