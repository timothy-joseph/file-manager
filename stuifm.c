/* See LICENSE file for license details */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>
#include <dirent.h>
#include <regex.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <linux/limits.h>

/* macros */
#define COMMAND_MAX 100000

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

#define PRINTW(COLOURPAIR, LINE, COLUMN, MAXSIZE, ISSEL, ISDIR, STR) attron(COLOR_PAIR((COLOURPAIR))); \
                                                                     move((LINE), (COLUMN)); \
                                                                     if ((ISSEL)) addch('>'); \
                                                                     if ((ISDIR)) printw("%.*s/", (MAXSIZE)-1-(!!(ISSEL)), (STR)); \
                                                                     else printw("%.*s", (MAXSIZE)-(!!(ISSEL)), (STR)); \
																	 for (int j = 1+(!!(ISSEL))+MIN((MAXSIZE), strlen((STR)))+(!!(ISDIR)); j <= MAXSIZE; j++) addch(' '); \
                                                                     attroff(COLOR_PAIR);


#define NUMOFDIGITS(RET, NUM, VAR) VAR = (NUM); \
                                   RET = 0; \
                                   while (VAR) { \
                                       VAR /= 10; \
                                       RET++; \
                                   } \
								   if (RET == 0) RET = 1; \

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) <= (Y)) ? (X) : (Y))

#define CtrlMask 0x1F

/* the following are masks are used to interact with the executecommand
 * if you don't want to use any of them, put a .i = 0 along side the command
 * the first three are pretty self-explanatory, but the last three are as follows:
 * CdLastLineMask -> chdir to the last line of the output of the command
 * SearchLastLineMask -> searches using the pattern showed on the last line of the output of the command
 * NoSaveSearchMask -> doesn't save the pattern used with the SearchLastLineMask
 *
 * in order to use multiple just put a bitwise or between them (eg: .i = NoConfirmationMask|NoReloadMask)
 */

#define NoConfirmationMask 0b1
#define NoReloadMask 0b10
#define NoEndWinMask 0b101
#define NoEndWinMaskBACKEND 0b100
#define CdLastLineMask 0b1000
#define SearchLastLineMask 0b10000
#define NoSaveSearchMask 0b100000

#define VERSION "2.0"

/* types/structs */
typedef struct Node Node;
struct Node {
	char path[PATH_MAX];
	char name[NAME_MAX];
	Node *next, *prev;
};

typedef struct Arg Arg;
struct Arg {
	int i;
	void *v;
};

typedef struct Key Key;
struct Key {
	int chr; /* this is int not a char, but i feel like the name chr is more descriptive */
	void (*func)(const Arg *arg);
	const Arg arg;
};

/* function declarations */
static void initialization(void);
static void getcurrentfiles(void);
static void append_ll(Node **head, char *path, char *name);
static void free_ll(Node **head);
static void rmvselection(char *path, char *name);
static int  isselected(char *path, char *name);
static void resizedetected(void);
static void rdrwf(void);
static void rdrwfmaincolumn(void);
static void rdrwfsecondarycolumn(void);
static void rdrwfhelper(void);
static void loop(void);
static void cleanup(void);
static char *getreadablefs(double size, char *ret);
static void movev(const Arg *arg);
static void moveh(const Arg *arg);
static void first(const Arg *arg);
static void last(const Arg *arg);
static void topofscreenscroll(const Arg *arg);
static void selection(const Arg *arg);
static void clearselection (const Arg *arg);
static void selectall(const Arg *arg);
static void directoriesfirst(const Arg *arg);
static void hiddenfilesswitch(const Arg *arg);
static void brename(const Arg *arg);
static void search(const Arg *arg);
static void executecommand(const Arg *arg);

/* global variables */
static Node *selhead = NULL;
static Node *dirlist = NULL;
static Node *current = NULL; /* the file that the cursor is placed on */
static Node *topofscreen = NULL;

static int  maxy, maxx, sortbydirectories = 0, hiddenfiles = 0;
static char status[NAME_MAX], pattern[PATH_MAX], cwd[PATH_MAX];

/* config.h */
#include "config.h"

/* function definitions */
void
initialization(void)
{
	static int executedbefore = 0;

	if (!executedbefore) {
		sortbydirectories = DIRECTORIESFIRST;
		hiddenfiles = HIDDENFILES;
		executedbefore = 1;
	}

	initscr();
	cbreak();
	noecho();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_WHITE, SELECTEDCOLOR);
	init_pair(3, DIRECTORYCOLOR, COLOR_BLACK);
	init_pair(4, DIRECTORYCOLOR, SELECTEDCOLOR);
	init_pair(5, COLOR_BLACK, COLOR_RED);
	init_pair(7, COLOR_BLACK, COLOR_WHITE);
	scrollok(stdscr, 1);
	getmaxyx(stdscr, maxy, maxx);
}

void
getcurrentfiles(void)
{
	/* free ll */
	free_ll(&dirlist);

	int lendir = -1, i;
	struct dirent **namelist;
	struct stat pathstat;

	getcwd(cwd, sizeof(cwd));
	lendir = scandir(".", &namelist, 0, alphasort);

	if (lendir <= 0)
		return;

	if (sortbydirectories) {
		/* put all other files into dirlist first (because it's a linked list they will end up at the bottom) */
		for (i = lendir-1; i >= 0; i--) {
			if (stat(namelist[i]->d_name, &pathstat) == 0 && !S_ISDIR(pathstat.st_mode) && \
					strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") != 0 && \
					(hiddenfiles == 0 ? namelist[i]->d_name[0] != '.' : 1))
				append_ll(&dirlist, cwd, namelist[i]->d_name);
		}
		/* then put all directories except . and .. */
		for (i = lendir-1; i >= 0; i--) {
			
			if (stat(namelist[i]->d_name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode) && \
					strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") != 0 && \
					(hiddenfiles == 0 ? namelist[i]->d_name[0] != '.' : 1))
				append_ll(&dirlist, cwd, namelist[i]->d_name);
		}
		/* then free each element in namelist */
		for (i = lendir-1; i >= 0; i--) {
			free(namelist[i]);
		}
	} else {
		for (i = lendir-1; i >= 0; i--) {
			if (strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") && \
					(hiddenfiles == 0 ? namelist[i]->d_name[0] != '.' : 1))
				append_ll(&dirlist, cwd, namelist[i]->d_name);
			free(namelist[i]);
		}
	}
	topofscreen = dirlist;
	current = dirlist;
	free(namelist);
}

void
rmvselection(char *path, char *name)
{
	/* used to remove files from the selection ll
	 * the basic algorithm to remove a node from doubly linked list
	 */
	if (selhead == NULL) {
		return;
	}

	Node *tmp = selhead;
	Node *tmp2 = NULL;
	Node *tmp3 = NULL;

	/* search */
	while (tmp != NULL) {
		if (strcmp(tmp->name, name) == 0 &&
				strcmp(tmp->path, path) == 0)
			break;
		tmp = tmp->next;
	}

	if (tmp == selhead) {
		tmp = selhead->next;
		if (tmp != NULL)
			tmp->prev = NULL;
		tmp2 = selhead;
		selhead = tmp;
		free(tmp2);
	} else if (tmp != NULL) {
		tmp2 = tmp->prev;
		tmp3 = tmp->next;
		if (tmp2 != NULL)
			tmp2->next = tmp3;
		if (tmp3 != NULL)
			tmp3->prev = tmp2;

		free(tmp);
	}
}

void
append_ll(Node **head, char *path, char *name)
{
	/* used to add files to the selection ll */
	Node *tmp = (Node *)malloc(sizeof(Node));
	strncpy(tmp->path, path, PATH_MAX);
	strncpy(tmp->name, name, NAME_MAX);
	tmp->next = *head;
	tmp->prev = NULL;
	if (*head != NULL)
		(*head)->prev = tmp;
	*head = tmp;
}

void
free_ll(Node **head)
{
	Node *tmp = NULL;
	while (*head != NULL) {
		tmp = *head;
		*head = (*head)->next;
		free(tmp);
	}
}


int
isselected(char *path, char *name)
{
	Node *tmp = selhead;

	while (tmp != NULL) {
		if (strcmp(tmp->name, name) == 0 &&
				strcmp(tmp->path, path) == 0)
			return 1;
		tmp = tmp->next;
	}
	return 0;
}

void
resizedetected(void)
{
	/* firstly end the win so we can reinit it */
	endwin();
	Node *tmp = current;
	initialization();
	rdrwf();
	current = tmp;
	strncpy(status, "window resized", NAME_MAX);
}

void
rdrwf(void)
{
	struct stat pathstat;
	struct passwd *pwd;
	struct group *gr;
	int i, numoffiles = 0, posinfiles = 0, digitsfiles, digitspos, t1;
	char fileinfo[NAME_MAX], perms[11], user[NAME_MAX], group[NAME_MAX], readablefilesize[NAME_MAX], date[NAME_MAX];
	Node *tmp = NULL;

	/* get and display the file information */
	clear();
	move(0, 0);
	if (stat(current->name, &pathstat) == 0) {
		if (S_ISDIR(pathstat.st_mode)) perms[0] = 'd'; else perms[0] = '-';
		if (pathstat.st_mode & S_IRUSR) perms[1] = 'r'; else perms[1] = '-';
		if (pathstat.st_mode & S_IWUSR) perms[2] = 'w'; else perms[2] = '-';
		if (pathstat.st_mode & S_IXUSR) perms[3] = 'x'; else perms[3] = '-';
		if (pathstat.st_mode & S_IRGRP) perms[4] = 'r'; else perms[4] = '-';
		if (pathstat.st_mode & S_IWGRP) perms[5] = 'w'; else perms[5] = '-';
		if (pathstat.st_mode & S_IXGRP) perms[6] = 'x'; else perms[6] = '-';
		if (pathstat.st_mode & S_IROTH) perms[7] = 'r'; else perms[7] = '-';
		if (pathstat.st_mode & S_IWOTH) perms[8] = 'w'; else perms[8] = '-';
		if (pathstat.st_mode & S_IXOTH) perms[9] = 'x'; else perms[9] = '-';
		perms[10] = 0;

		if ((pwd = getpwuid(pathstat.st_uid)) != NULL) strncpy(user, pwd->pw_name, NAME_MAX);
		else snprintf(user, NAME_MAX, "%d", pathstat.st_uid);
	
		if ((gr = getgrgid(pathstat.st_gid)) != NULL) strncpy(group, gr->gr_name, NAME_MAX);
		else snprintf(group, NAME_MAX, "%d", pathstat.st_gid);
	
		getreadablefs((double)pathstat.st_size, readablefilesize);
	    strftime(date, NAME_MAX, "%Y-%B-%d %H:%M", gmtime(&(pathstat.st_ctim).tv_sec));
	
		snprintf(fileinfo, maxx-1, "%s %d %s %s %s %s", perms, (int)pathstat.st_nlink, user, group, readablefilesize, date);
		PRINTW(1, 0, 0, maxx, 0, 0, fileinfo);
	}
	if (maxx-strlen(cwd) > 0) PRINTW(1, 0, maxx-strlen(cwd), strlen(cwd), 0, 0, cwd);

	move(1, 0);
	for (i = 0; i < maxx; i++) {
		addch('-');
	}

	/* draw based on modes */
	move(2, 0);

	/* column drawing */
	rdrwfhelper();

	/* print a line to separate files from the status */
	move(maxy-2, 0);
	for (i = 0; i < maxx; i++) {
		addch('-');
	}

	/* print the status */
	PRINTW(1, maxy-1, 0, maxx-1, 0, 0, status);


	/* print the number of files and what number is the current file */
	numoffiles = 0;
	posinfiles = 0;
	tmp = dirlist;
	while (tmp) {
		numoffiles++;
		if (tmp == current) {
			posinfiles = numoffiles;
		}
		tmp = tmp->next;
	}

	NUMOFDIGITS(digitsfiles, numoffiles, t1);
	NUMOFDIGITS(digitspos, posinfiles, t1);

	if (maxx-digitspos-3-digitsfiles > 0) mvprintw(maxy-1, maxx-digitspos-3-digitsfiles, " %d/%d", posinfiles, numoffiles);
}


void
rdrwfhelper(void)
{
	int i, j, size = (maxx-2)/3, lendir, currentonscreen = 0, overwrite = 0;
	char prevpath[PATH_MAX], *p, statpath[PATH_MAX], name[NAME_MAX];
	struct dirent **namelist;
	struct stat pathstat;
	Node *tmp = topofscreen;

	/* first column */

	if (strcmp(cwd, "/") == 0) goto skip0;
	lendir = scandir("..", &namelist, 0, alphasort);
	if (lendir <= 0) goto skip0;

	if (realpath("..", prevpath) == NULL) prevpath[0] = 0;

	if (sortbydirectories) {
		i = 2;
		/* draw first the directories */
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", prevpath, name);
			if (stat(statpath, &pathstat) == 0 && S_ISDIR(pathstat.st_mode) && \
					strcmp(name, ".") != 0 && \
					strcmp(name, "..") != 0 && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {
				
				p = strrchr(cwd, '/');
				if (p == NULL) p = cwd+strlen(cwd);
				else p += 1;

				if (strcmp(name, p) == 0) {
					PRINTW(7, i, 0, size-1, isselected(prevpath, name), 1, name);
				} else if (isselected(prevpath, name)) {
					PRINTW(4, i, 0, size-1, 1, 1, name);
				} else {
					PRINTW(3, i, 0, size-1, 0, 1, name);
				}
				i++;
			}
		}
		/* then put all other files */
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", prevpath, name);
			if (stat(statpath, &pathstat) == 0 && !S_ISDIR(pathstat.st_mode) && \
					strcmp(name, ".") != 0 && \
					strcmp(name, "..") != 0 && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {

				if (isselected(prevpath, name)) {
					PRINTW(2, i, 0, size-1, 1, 1, name);
				} else {
					PRINTW(1, i, 0, size-1, 0, 1, name);
				}
				i++;
			}
		}
		/* then free each element in namelist */
		for (j = 0; j < lendir; j++) {
			free(namelist[j]);
		}
	} else {
		for (j = 0; j < lendir && i <= maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", prevpath, name);
			if (strcmp(name, ".") != 0 && \
					strcmp(name, "..") && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {

				p = strrchr(cwd, '/');
				if (p == NULL) p = cwd+strlen(cwd);
				else p += 1;

				overwrite = 0;
				if (strcmp(name, p) == 0) {
					overwrite = 7;
				}

				if (stat(statpath, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
					if (isselected(prevpath, name)) {
						PRINTW(MAX(overwrite, 4), i, 0, size-1, 1, 1, name);
					} else {
						PRINTW(MAX(overwrite, 3), i, 0, size-1, 0, 1, name);
					}
				} else {
					if (isselected(prevpath, name)) {
						PRINTW(MAX(overwrite, 2), i, 0, size-1, 1, 0, name);
					} else {
						PRINTW(MAX(overwrite, 1), i, 0, size-1, 0, 0, name);
					}
				}

				i++;
			}
			free(namelist[i]);
		}
	}
	free(namelist);


skip0:
	i = 2;
	while (tmp != NULL && i < maxy-2) {
		overwrite = 0;
		if (tmp == current) {
			overwrite = 7;
			currentonscreen = 1;
		}

		/* decision on wheter the element is a directory and if it is selected */
		if (stat(tmp->name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
			if (isselected(cwd, tmp->name)) {
				PRINTW(MAX(overwrite, 4), i, size, size-1, 1, 1, tmp->name);
			} else {
				PRINTW(MAX(overwrite, 3), i, size, size-1, 0, 1, tmp->name);
			}
		} else {
			if (isselected(cwd, tmp->name)) {
				PRINTW(MAX(overwrite, 2), i, size, size-1, 1, 0, tmp->name);
			} else {
				PRINTW(MAX(overwrite, 1), i, size, size-1, 0, 0, tmp->name);
			}
		}


		tmp = tmp->next;
		i++;
	}

	if (i == 2 || dirlist == NULL) {
		PRINTW(5, i, size, size-1, 0, 0, "NO FILES IN CURRENT DIRECTORY");
		currentonscreen = 1; /* to stop a possible infinite loop */
	}
	
	if (!currentonscreen) {
		topofscreen = current;
		rdrwf();
	}

	/* column 3 - preview
	 * ..........
	 * i need to do the mess i did at column 1 again...
	 */
	char nextpath[PATH_MAX];
	if (stat(current->name, &pathstat) != 0 || !S_ISDIR(pathstat.st_mode)) return;

	lendir = scandir(current->name, &namelist, 0, alphasort);
	if (lendir <= 0) return;

	if (realpath(current->name, nextpath) == NULL) nextpath[0] = 0;

	if (sortbydirectories) {
		i = 2;
		/* draw first the directories */
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", nextpath, name);
			if (stat(statpath, &pathstat) == 0 && S_ISDIR(pathstat.st_mode) && \
					strcmp(name, ".") != 0 && \
					strcmp(name, "..") != 0 && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {
				
				p = strrchr(cwd, '/');
				if (p == NULL) p = cwd+strlen(cwd);
				else p += 1;

				if (i == 2) {
					PRINTW(7, i, 2*size, maxx-2*size-1, isselected(nextpath, name), 1, name);
				} else if (isselected(nextpath, name)) {
					PRINTW(4, i, 2*size, maxx-2*size-1, 1, 1, name);
				} else {
					PRINTW(3, i, 2*size, maxx-2*size-1, 0, 1, name);
				}
				i++;
			}
		}
		/* then put all other files */
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", nextpath, name);
			if (stat(statpath, &pathstat) == 0 && !S_ISDIR(pathstat.st_mode) && \
					strcmp(name, ".") != 0 && \
					strcmp(name, "..") != 0 && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {

				if (isselected(nextpath, name)) {
					PRINTW(2, i, 2*size, maxx-2*size-1, 1, 1, name);
				} else {
					PRINTW(1, i, 2*size, maxx-2*size-1, 0, 1, name);
				}
				i++;
			}
		}
		/* then free each element in namelist */
		for (j = 0; j < lendir; j++) {
			free(namelist[j]);
		}
	} else {
		for (j = 0; j < lendir && i <= maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", nextpath, name);
			if (strcmp(name, ".") != 0 && \
					strcmp(name, "..") && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {

				p = strrchr(cwd, '/');
				if (p == NULL) p = cwd+strlen(cwd);
				else p += 1;

				overwrite = 0;
				if (strcmp(name, p) == 0) {
					overwrite = 7;
				}

				if (stat(statpath, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
					if (isselected(nextpath, name)) {
						PRINTW(MAX(overwrite, 4), i, 2*size, maxx-2*size-1, 1, 1, name);
					} else {
						PRINTW(MAX(overwrite, 3), i, 2*size, maxx-2*size-1, 0, 1, name);
					}
				} else {
					if (isselected(nextpath, name)) {
						PRINTW(MAX(overwrite, 2), i, 2*size, maxx-2*size-1, 1, 0, name);
					} else {
						PRINTW(MAX(overwrite, 1), i, 2*size, maxx-2*size-1, 0, 0, name);
					}
				}

				i++;
			}
			free(namelist[i]);
		}
	}
	free(namelist);
	return;
}

void
rdrwfmaincolumn(void) /* (r)e(dr)a(w) (f)unction */
{
	/* draws the main column (current column) */
	struct stat pathstat;
	int i, currentonscreen = 0, overwrite = 0, column = 0, maxsize = maxx;
	Node *tmp = topofscreen;

	i = 2;
	while (tmp != NULL && i < maxy-2) {
		overwrite = 0;

		if (tmp == current) {
			overwrite = 7;
			currentonscreen = 1;
		}

		/* decision on wheter the element is a directory and if it is selected */
		if (stat(tmp->name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
			if (isselected(cwd, tmp->name)) {
				PRINTW(MAX(overwrite, 4), i, column, maxsize, 1, 1, tmp->name);
			} else {
				PRINTW(MAX(overwrite, 3), i, column, maxsize, 0, 1, tmp->name);
			}
		} else {
			if (isselected(cwd, tmp->name)) {
				PRINTW(MAX(overwrite, 2), i, column, maxsize, 1, 0, tmp->name);
			} else {
				PRINTW(MAX(overwrite, 1), i, column, maxsize, 0, 0, tmp->name);
			}
		}


		tmp = tmp->next;
		i++;
	}

	if (i == 2 || dirlist == NULL) {
		PRINTW(5, i, column, maxsize, 0, 0, "NO FILES IN CURRENT DIRECTORY");
		currentonscreen = 1; /* to stop a possible infinite loop */
	}
	
	if (!currentonscreen) {
		topofscreen = current;
		rdrwf();
	}
}

void
rdrwfsecondarycolumn(void)
{
}

void
loop(void)
{
	int c, i;
	rdrwf();
	while ((c = getch()) != QUIT_CHAR) {
		if (c == KEY_RESIZE) {
			resizedetected();
			continue;
		}

		status[0] = 0;
		getcwd(cwd, PATH_MAX);
		for (i = 0; i < LENGTH(keys); i++) {
			if (keys[i].chr == c) {
				keys[i].func(&keys[i].arg);
			}
		}
		rdrwf();
	}
}

void
cleanup(void)
{
	/* free any allocated memory */
	free_ll(&dirlist);
	free_ll(&selhead);
	/* close curses session */
	clear();
	endwin();
	FILE *fp = NULL;
	if ((fp = fopen(VCD_PATH, "w+")) == NULL)
		return;

	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
		fprintf(fp, "%s\n", cwd);
	}
	
	fclose(fp);
}

char *
getreadablefs(double size, char *ret)
{
	int i = 0;
	char orders[4] = "BKMG";

	ret[0] = 0;

	while (size > 1024 && i < 3) {
		size /= 1024;
		i++;
	}

	snprintf(ret, NAME_MAX, "%.2f%c", size, orders[i]);
	return ret;
}

void
movev(const Arg *arg)
{
	if (!arg) return;
	if (current == NULL || dirlist == NULL) return;

	int i = arg->i, j = 0;

	/* decide if we should go forward or backwards */

	if (i == 1) {
		if (current->next) current = current->next;
	} else if (i == -1) {
		if (current->prev) current = current->prev;
	} else if (i == 2) {
		for (j = 1; j <= maxy/2; j++) {
			if (current->next) current = current->next;
		}
	} else  if (i == -2) {
		for (j = 1; j <= maxy/2; j++) {
			if (current->prev) current = current->prev;
		}
	}
}

void
moveh(const Arg *arg)
{
	struct stat pathstat;
	char oldpattern[PATH_MAX], *p;
	Arg searcharg = {.i = 1};
	int i, tosearch = 0;

	memset(oldpattern, 0, sizeof(oldpattern));

	if (!arg)
		return;
	
	if (arg->i == -1) {
		/* for a single level undo */
		p = strrchr(cwd, '/');

		if (p && p[1] != 0) {
			tosearch = 1;
			strncpy(oldpattern, pattern, PATH_MAX);
			strncpy(pattern, p+1, PATH_MAX);
			// printf(pattern);
		}

		chdir("..");
	} else {
		if (stat(current->name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
			chdir(current->name);
		} else {
			return;
		}
	}

	getcurrentfiles();
	if (tosearch) {
		search(&searcharg);
		strncpy(pattern, oldpattern, PATH_MAX);
	}
	strncpy(status, "changed directory", NAME_MAX);
}

void
first(const Arg *arg)
{
	current = dirlist;
}

void
last(const Arg *arg)
{
	if (dirlist == NULL) return;

	int i;
	Node *tmp = dirlist;

	while (tmp->next != 0) {
		tmp = tmp->next;
	}

	current = tmp;
	topofscreen = tmp;

	for (i = 1; i < maxy / 2; i++) {
		if (topofscreen->prev) topofscreen = topofscreen->prev;
	}
}

void
topofscreenscroll(const Arg *arg)
{
	int i = arg->i, ok = 0, j = 0;
	Node *tmp = NULL;

	/* no need to check if current will still be on screen for i == 1 because 
	 * rdrwf does that
	 */

	j = 0;

	if (i == 1) {
		if (topofscreen->next) topofscreen = topofscreen->next;
	} else if (i == -1) {
		/* checking if current will be on screen, otherwise it will snap back to current being topofscreen */
		tmp = topofscreen->prev;

		while (tmp && j < maxy-3) {
			if (current == tmp) ok = 1;
			tmp = tmp->next;
			j++;
		}

		if (topofscreen->prev && ok) topofscreen = topofscreen->prev;
	}
}

void
selection(const Arg *arg)
{
	if (current == NULL) {
		strncpy(status, "no current file/no files in directory", NAME_MAX);
		return;
	}

	if (isselected(current->path, current->name)) {
		/* remove from list */
		rmvselection(current->path, current->name);
	} else {
		/* add it to the list */
		append_ll(&selhead, current->path, current->name);
	}
	strncpy(status, "changed selection", NAME_MAX);
}

void
clearselection(const Arg *arg)
{
	/* free the ll */
	free_ll(&selhead);
	strncpy(status, "cleared selection", NAME_MAX);
}

void
selectall(const Arg *arg)
{
	Node *tmp = dirlist;
	while (tmp) {
		if (!isselected(current->path, current->name)) {
			append_ll(&selhead, current->path, current->name);
		}
	}
}

void
directoriesfirst(const Arg *arg)
{
	sortbydirectories = !sortbydirectories;
	if (sortbydirectories == 1) {
		strncpy(status, "sorting with directories being first", NAME_MAX);
	} else {
		strncpy(status, "sorting with normaly", NAME_MAX);
	}
	getcurrentfiles();
}

void
hiddenfilesswitch(const Arg *arg)
{
	hiddenfiles = !hiddenfiles;
	if (hiddenfiles == 1) {
		strncpy(status, "showing hidden files", NAME_MAX);
	} else {
		strncpy(status, "hiding hidden files", NAME_MAX);
	}
	getcurrentfiles();
}

void
brename(const Arg *arg)
{
	/* this one is done with coreutils */
	/* don't do anything if nothing is selected */
	if (selhead == NULL) return;

	/* endwin */
	endwin();

	Node *tmp = selhead;
	char command[PATH_MAX], newname[NAME_MAX], input[NAME_MAX];

	FILE *fp = NULL, *gp = NULL;
	if ((fp = fopen(BRENAME_TXT_PATH, "w+")) == NULL)
		return;

	/* get all of the names into one file */
	while (tmp != NULL) {
		fprintf(fp, "%s\n", tmp->name);
		tmp = tmp->next;
	}
	fclose(fp);

	/* let the user edit it */
	sprintf(command, "$EDITOR %s", BRENAME_TXT_PATH);
	system(command);

	/* build rename script */
	if ((fp = fopen(BRENAME_TXT_PATH, "r")) == NULL)
		return;
	if ((gp = fopen(BRENAME_SCRIPT_PATH, "w")) == NULL)
		return;

	fprintf(gp, "#!/bin/bash\n");

	tmp = selhead;

	/* get the new names line by line */
	while (tmp != NULL) {
		fgets(newname, NAME_MAX, fp);
		fprintf(gp, "mv -i %s/%s %s/%s", tmp->path, tmp->name, tmp->path, newname);
		tmp = tmp->next;
	}
	fclose(fp);
	fclose(gp);

	sprintf(command, "$EDITOR %s", BRENAME_SCRIPT_PATH);
	system(command);

	printf("do you want to run that script [yes/No]: ");
	fgets(input, NAME_MAX, stdin);

	printf("%s\n", input);
	if (input[0] == 'y' || input[0] == 'Y') {
		system(BRENAME_SCRIPT_PATH);
		strncpy(status, "performed a bulk rename", NAME_MAX);
	}

	/* finally clear the selection */
	clearselection(NULL);
	getcurrentfiles();
}


void
search(const Arg *arg)
{
	/* search through the ll until i find a element that starts with that
	 * then redraw the page starting with that element
	 */
	Node *tmp = current, *dirlistend = NULL;
	regex_t regex;
	int reti, oktofree = 0;

    switch (arg->i) {
    case 0: endwin();
    
            printf("search: ");
            fgets(pattern, PATH_MAX, stdin);
            if (pattern[strlen(pattern)-1] == '\n') pattern[strlen(pattern)-1] = 0;
            
            strncpy(status, "searched with new pattern", NAME_MAX);
            
            if (pattern[0] != 0) {
            	reti = regcomp(&regex, pattern, 0);
             	if (reti) return;
            	oktofree = 1;
            } else {
            	strncpy(status, "please input a pattern", NAME_MAX);
            	initialization();
             	return;
            }
            /* FALLTHROUGH */
    
    case 1: if (!oktofree && pattern[0] != 0) {
                reti = regcomp(&regex, pattern, 0);
                 if (reti) return;
                 oktofree = 1;
            } else if (!oktofree) {
               strncpy(status, "please input a pattern", NAME_MAX);
               return;
            }
            
            if (arg->i == 1) tmp = tmp->next; /* so it doesn't detect the same element if we want the next one
                                               * but in case we search with a new pattern, we want it to match the current
                                               * position if it matches
                                               */
            
            while (tmp != NULL) {
               reti = regexec(&regex, tmp->name, 0, NULL, 0);
               if (!reti) break;
               tmp = tmp->next;
            }
            
            if (tmp == NULL) {
                tmp = dirlist;
                strncpy(status, "search reached BOTTOM, starting from the TOP", NAME_MAX);
                
                while (tmp != NULL) {
                   reti = regexec(&regex, tmp->name, 0, NULL, 0);
                   if (!reti) break;
                   tmp = tmp->next;
                }
            }
            break;
    
    case -1: if (pattern[0] != 0) {
    	         reti = regcomp(&regex, pattern, 0);
    	         if (reti) return;
    	         oktofree = 1;
             } else {
    	         strncpy(status, "please input a pattern", NAME_MAX);
    	         return;
             }
    
             tmp = tmp->prev;
             while (tmp != NULL) {
                 reti = regexec(&regex, tmp->name, 0, NULL, 0);
                 if (!reti) break;
                 tmp = tmp->prev;
             }
             
             if (tmp == NULL) {
                 tmp = dirlist;
                 strncpy(status, "search reached TOP, starting from the BOTTOM", NAME_MAX);
                 
                 while (tmp) {
                     dirlistend = tmp;
                     tmp = tmp->next;
                 }
                 tmp = dirlistend;
                 
                 while (tmp != NULL) {
                     reti = regexec(&regex, tmp->name, 0, NULL, 0);
                     if (!reti) break;
                     tmp = tmp->prev;
                 }
             }
             break;
    }

	if (oktofree) {
		regfree(&regex);
	}

	if (tmp == NULL) {
		strncpy(status, "no item with that pattern was found", NAME_MAX);
		tmp = current;
	}

	initialization();
	current = tmp;
}

void
executecommand(const Arg *arg)
{
	char input[NAME_MAX], inputcommand[PATH_MAX], command[COMMAND_MAX], toconcat[PATH_MAX];
	int i, k = 0;
	Node *tmp = NULL;
	
	if (!(arg->i & NoEndWinMaskBACKEND)) endwin();

	if (!arg || !arg->v) {
		printf("your command: ");
		fgets(inputcommand, PATH_MAX, stdin);
		if (inputcommand[strlen(inputcommand)-1] == '\n') inputcommand[strlen(inputcommand)-1] = 0;
	} else {
		strncpy(inputcommand, *((char **)arg->v), PATH_MAX);
	}

	/* determine if in the input command i need to put the current file name, the whole selection or leave it like this
	 * % means current file
	 * %s means whole selection
	 * %p means the current working directory
	 */
	
	for (i = 0; inputcommand[i] != 0 && k < PATH_MAX - 1 && i < COMMAND_MAX; i++) {
		if (inputcommand[i] == '%') {
			if (inputcommand[i+1] == '%') {
				command[k] = '%';
				k++;
				i++;
			} else if (inputcommand[i+1] == 'c') {
				strncat(command, current->name, COMMAND_MAX-strlen(current->name)-1);
				k = strlen(command);
				i++;
			} else if (inputcommand[i+1] == 'C') {
				snprintf(toconcat, PATH_MAX, "%s/%s", current->path, current->name);
				strncat(command, current->name, COMMAND_MAX-strlen(toconcat)-1);
				k = strlen(command);
				i++;
			} else if (inputcommand[i+1] == 's') {
				if (selhead == NULL) {
					strncpy(status, "selection is empty", NAME_MAX);
					goto skipexecutecommand;
				}
				tmp = selhead;

				while (tmp) {
					if (tmp->next) snprintf(toconcat, PATH_MAX, "%s/%s ", tmp->path, tmp->name);
					else snprintf(toconcat, PATH_MAX, "%s/%s", tmp->path, tmp->name);
					strncat(command, toconcat, COMMAND_MAX-strlen(toconcat)-1);
					toconcat[0] = 0;
					k = strlen(command);

					tmp = tmp->next;
				}
				i++;

			} else if (inputcommand[i+1] == 'S') {
				command[k] = '%';
				k++;
				command[k+1] = 'S';
				k++;
				i++;
			}
		} else {
			command[k] = inputcommand[i];
			k++;
			command[k] = 0;
		}
	}

	/* TODO: remove confirmation once i'm sure everything works fine */
	if (!(arg->i & NoConfirmationMask)) printf("are you sure you want to execute the command '%s' [yes/No]: ", command);
	if (!(arg->i & NoConfirmationMask)) fgets(input, NAME_MAX, stdin);

	if (arg->i & NoConfirmationMask || input[0] == 'y' || input[0] == 'Y') {
		/* TODO: %S -> run a command for each of the elements in the selection */
		system(command);
		if (!(arg->i & NoEndWinMaskBACKEND)) snprintf(status, NAME_MAX, "executed the command '%s'", command);
	} else if (!(arg->i & NoEndWinMaskBACKEND)) {
		snprintf(status, NAME_MAX, "didn't execute the command '%s'", command);
	}

skipexecutecommand:
	if (!(arg->i & NoEndWinMaskBACKEND)) printf("press any key to continue\n");
	if (!(arg->i & NoEndWinMaskBACKEND)) fgetc(stdin);

	if (!(arg->i & NoEndWinMaskBACKEND)) initialization();
	if (!(arg->i & NoReloadMask)) getcurrentfiles();

	if (arg->i & SearchLastLineMask) {
	/* <++> */
	}

	if (arg->i & CdLastLineMask) {
	/* <++> */
	}
}


/* main */
int
main(int argc, char *argv[])
{
	if (argc > 1) {
		if (strcmp(argv[1], "--version") == 0) {
			printf("stuifm-%s\n", VERSION);
			return 0;
		} else if(strcmp(argv[1], "--help") == 0) {
			printf("use: stuifm [--version|--help] or stuifm [directory]\n");
			printf("check the README.md for a tutorial\n");
			printf("for using it as a way to cd into a directory, put the following in your .bashrc:\n");
			printf("alias fm='stuifm; LASTDIR=`cat $HOME/.vcd`; cd \"$LASTDIR\"'\n");
			printf("and call the program using fm\n\n");
			printf("in order to use the bulkrename function, you need to define the $EDITOR environment variable with your prefered editor\n");
			return 0;
		} else  {
			chdir(argv[1]);
		}
	}

	initialization();
	getcurrentfiles();
	loop();
	cleanup();

	return 0;
}
