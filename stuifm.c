/* TODO: restore the current file to be as close to the old current file before a file execution */
/* See LICENSE file for license details */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
#define COLUMNS_MAX 7
#define N 200

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

#define NoConfirmationMask 0b1
#define NoReloadMask 0b10
#define NoEndWinMask 0b101
#define NoEndWinMaskBACKEND 0b100
#define CdLastLineMask 0b1000
#define SearchLastLineMask 0b10000
#define NoSaveSearchMask 0b100000
#define NoWaitUntilKeyPress 0b1000000

#define VERSION "2.0"

/* types/structs */
typedef struct FileElem FileElem;
struct FileElem {
	char name[NAME_MAX], path[PATH_MAX];
};

typedef struct Files Files;
struct Files {
	FileElem *contents;
	int end, n;
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
static void addelem(Files *list, char *path, char *name);
static void freelistcontents(Files *list);
static void rmvselection(char *path, char *name);
static int  isselected(char *path, char *name);
static void resizedetected(void);
static void rdrwf(void);
static void rdrwfmaincolumn(int column, int size);
static void rdrwfsecondarycolumn(char *comingfrom, char *pathtodraw, int column, int size, int direction, char *highlightedname);
static void rdrwfhelper(void);
static int  iscurrentonscreen(void);
static char *getreadablefs(double size, char *ret);
static char *escapestring(char *str, size_t n);
static void loop(void);
static void cleanup(void);
static void movev(const Arg *arg);
static void moveh(const Arg *arg);
static void first(const Arg *arg);
static void last(const Arg *arg);
static void topofscreenscroll(const Arg *arg);
static void drawratiosscroll(const Arg *arg);
static void toggleselect(const Arg *arg);
static void clearselection (const Arg *arg);
static void selectall(const Arg *arg);
static void directoriesfirst(const Arg *arg);
static void hiddenfilesswitch(const Arg *arg);
static void search(const Arg *arg);
static void executecommand(const Arg *arg);

/* global variables */
static Files selected;
static Files fileslist;
static int  maxy, maxx, current = 1, topofscreen = 1, sortbydirectories = 0, hiddenfiles = 0, cratio = 0;
static char status[NAME_MAX], pattern[PATH_MAX], cwd[PATH_MAX];

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
	freelistcontents(&fileslist);

	int lendir = -1, i;
	struct dirent **namelist;
	struct stat pathstat;

	getcwd(cwd, sizeof(cwd));
	lendir = scandir(".", &namelist, 0, alphasort);

	if (lendir <= 0) return;

	if (sortbydirectories) {
		/* put all directories except in . and .. */
		for (i = 0; i < lendir; i++) {
			if (stat(namelist[i]->d_name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode) && \
					strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") != 0 && \
					(hiddenfiles == 0 ? namelist[i]->d_name[0] != '.' : 1))
				addelem(&fileslist, cwd, namelist[i]->d_name);
		}
		/* then everything else */
		for (i = 0; i < lendir; i++) {
			if (stat(namelist[i]->d_name, &pathstat) == 0 && !S_ISDIR(pathstat.st_mode) && \
					(hiddenfiles == 0 ? namelist[i]->d_name[0] != '.' : 1))
				addelem(&fileslist, cwd, namelist[i]->d_name);
		}
		/* then free each element in namelist */
		for (i = 0; i < lendir; i++) {
			free(namelist[i]);
		}
	} else {
		for (i = 0; i < lendir; i++) {
			if (strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") && \
					(hiddenfiles == 0 ? namelist[i]->d_name[0] != '.' : 1)) {
				addelem(&fileslist, cwd, namelist[i]->d_name);
				free(namelist[i]);
			}
		}
	}
	current = topofscreen = 1;
	free(namelist);
}

void
rmvselection(char *path, char *name)
{
	if (!selected.contents) return;

	int i, found = 0;
	for (i = 1; i <= selected.end; i++) {
		if (strcmp(selected.contents[i].path, path) == 0 && strcmp(selected.contents[i].name, name) == 0) {
			found = 1;
		}

		if (found) { /* this to remove the element instead of 2 for loops */
			strncpy(selected.contents[i].path, selected.contents[i+1].path, PATH_MAX);
			strncpy(selected.contents[i].name, selected.contents[i+1].name, NAME_MAX);
		}
	}

	if (found) selected.end--;
}

void
addelem(Files *list, char *path, char *name)
{
	if (list->contents == NULL) {
		list->contents = (FileElem *)malloc(N * sizeof(FileElem));
		list->n = N;
		list->end = 0;
	} else if (list->end+1 >= list->n) {
		list->contents = (FileElem *)realloc(list->contents, (list->n + N) * sizeof(FileElem));
		list->n += N;
	}

	if (list->contents == NULL) {
		perror("couldn't allocate memory for the file contents");
		exit(1);
	}
	
	list->end++; /* this means that lists are 1 indexed, because list->end is initially 0 */
	strncpy(list->contents[list->end].path, path, PATH_MAX);
	strncpy(list->contents[list->end].name, name, NAME_MAX);
}

void
freelistcontents(Files *list)
{
	if (list->contents == NULL) return;
	free(list->contents);
	list->contents = NULL;
	list->n = 0;
	list->end = 0;
}


int
isselected(char *path, char *name)
{
	if (!selected.contents) return 0;

	int i = 1;

	for (i = 1; i <= selected.end; i++) {
		if (strcmp(selected.contents[i].path, path) == 0 && strcmp(selected.contents[i].name, name) == 0) {
			return 1;
		}
	}

	return 0;
}

void
resizedetected(void)
{
	endwin();
	initialization();
	strncpy(status, "window resized", NAME_MAX);
}

void
rdrwf(void)
{
	struct stat pathstat;
	struct passwd *pwd;
	struct group *gr;
	int i, digitsfiles, digitspos, t1;
	char fileinfo[NAME_MAX], perms[11], user[NAME_MAX], group[NAME_MAX], readablefilesize[NAME_MAX], date[NAME_MAX];

	if (!iscurrentonscreen()) {
		if (current >= topofscreen+maxy-4) {
			topofscreen = current-maxy+5;
		} else {
			topofscreen = current;
		}

		if (topofscreen < 1) topofscreen = current;
		if (topofscreen > fileslist.end) topofscreen = current;
		if (!iscurrentonscreen()) topofscreen = current;
	}

	/* get and display the file information */
	clear();
	move(0, 0);
	if (stat(fileslist.contents[current].name, &pathstat) == 0) {
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
	if (maxx-strlen(cwd) > 0) {PRINTW(1, 0, maxx-strlen(cwd), strlen(cwd), 0, 0, cwd);}

	move(1, 0);
	for (i = 0; i < maxx; i++) {
		addch('-');
	}

	move(2, 0);

	/* column drawing */
	rdrwfhelper();

	/* print a line to separate files from the status */
	move(maxy-2, 0);
	for (i = 0; i < maxx; i++) {
		addch('-');
	}

	/* print the status */
	escapestring(status, NAME_MAX);
	PRINTW(1, maxy-1, 0, maxx-1, 0, 0, status);


	/* print the number of files and what number is the current file */
	NUMOFDIGITS(digitsfiles, fileslist.end, t1);
	NUMOFDIGITS(digitspos, current, t1);

	if (maxx-digitspos-3-digitsfiles > 0) mvprintw(maxy-1, maxx-digitspos-3-digitsfiles, " %d/%d", current, fileslist.end);
}

void
rdrwfhelper(void)
{
	int i, size = maxx, currentcolumn = 0, currentposition = 1, ratiossum = 0, overwritesize = 0;
	char prevpath[PATH_MAX] = "", nextpath[PATH_MAX] = "", resolvedpath[PATH_MAX], highlightedname[NAME_MAX], tmpstr[PATH_MAX], *p, *pnext;
	struct stat pathstat;

	for (i = 0; drawratios[cratio][i]; i++) {
		ratiossum += drawratios[cratio][i];
	}
	currentposition = drawratios[cratio][i+1];
	
	if (currentposition >= i || ratiossum == 0) {
		rdrwfmaincolumn(0, maxx);
		strncpy(status, "ratios not properly defined", NAME_MAX);
		return;
	} else {
		size = maxx/ratiossum;
	}

	for (i = 0; i < currentposition; i++) {
		strncat(prevpath, "../", PATH_MAX-strlen(prevpath)-1);
	}
	p = prevpath;

	if (fileslist.contents) strncpy(nextpath, fileslist.contents[current].name, NAME_MAX);

	for (i = 0; drawratios[cratio][i]; i++) {
		overwritesize = 0;

		if (drawratios[cratio][i+1] == 0) {
			overwritesize = maxx - currentcolumn;
		}

		if (i < currentposition) {
			/* for highlighting the name of the next column's folder name */
			if (strcmp(cwd, "/") != 0) {
				pnext = strchr(p, '/');
				if (pnext != NULL && strcmp(pnext, "/") != 0) {
					pnext++;

					if (realpath(pnext, resolvedpath) == NULL) resolvedpath[0] = 0;
				} else {
					strncpy(resolvedpath, cwd, PATH_MAX);
				}

				if (p == NULL || p[0] == 0 || strcmp(p, "/") == 0) {
					rdrwfsecondarycolumn(resolvedpath, "..", currentcolumn, MAX(overwritesize, drawratios[cratio][i]*size), 0, NULL);
				} else {
					rdrwfsecondarycolumn(resolvedpath, p, currentcolumn, MAX(overwritesize, drawratios[cratio][i]*size), 0, NULL);
				}

				p = pnext;
			}
		} else if (i == currentposition) {
			rdrwfmaincolumn(currentcolumn, MAX(overwritesize, drawratios[cratio][i]*size));
		} else {
			if (!fileslist.contents) break;

			rdrwfsecondarycolumn("", nextpath, currentcolumn, MAX(overwritesize, drawratios[cratio][i]*size), 1, highlightedname);
			snprintf(tmpstr, PATH_MAX, "%s/%s", nextpath, highlightedname);

			if (stat(tmpstr, &pathstat) != 0 ) break;
			if (!S_ISDIR(pathstat.st_mode)) break;

			strncpy(nextpath, tmpstr, PATH_MAX);
		}

		currentcolumn += drawratios[cratio][i]*size;
	}
}

void
rdrwfmaincolumn(int column, int size) /* (r)e(dr)a(w) (f)unction */
{
	struct stat pathstat;
	int i, overwrite = 0;

	i = 2;
	while (topofscreen+i-2 <= fileslist.end && i < maxy-2) {
		overwrite = 0;

		if (topofscreen+i-2 == current) {
			overwrite = 7;
		}

		/* decision on wheter the element is a directory and if it is selected */
		if (stat(fileslist.contents[topofscreen+i-2].name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
			if (isselected(cwd, fileslist.contents[topofscreen+i-2].name)) {
				PRINTW(MAX(overwrite, 4), i, column, size-1, 1, 1, fileslist.contents[topofscreen+i-2].name);
			} else {
				PRINTW(MAX(overwrite, 3), i, column, size-1, 0, 1, fileslist.contents[topofscreen+i-2].name);
			}
		} else {
			if (isselected(cwd, fileslist.contents[topofscreen+i-2].name)) {
				PRINTW(MAX(overwrite, 2), i, column, size-1, 1, 0, fileslist.contents[topofscreen+i-2].name);
			} else {
				PRINTW(MAX(overwrite, 1), i, column, size-1, 0, 0, fileslist.contents[topofscreen+i-2].name);
			}
		}


		i++;
	}

	if (i == 2 || !fileslist.contents) {
		PRINTW(5, i, column, size-1, 0, 0, "NO FILES IN CURRENT DIRECTORY");
	}
}

void
rdrwfsecondarycolumn(char *comingfrom, char *pathtodraw, int column, int size, int direction, char *highlightedname) /* direction 0 -> backward; direction 1 -> forwards */
{
	int i, j, lendir, overwrite = 0;
	char resolvedpath[PATH_MAX], *p, statpath[PATH_MAX], name[NAME_MAX];
	struct dirent **namelist;
	struct stat pathstat;

	if (stat(pathtodraw, &pathstat) != 0) return;
	if (!S_ISDIR(pathstat.st_mode)) return;

	lendir = scandir(pathtodraw, &namelist, 0, alphasort);
	if (lendir == 0) {PRINTW(5, 2, column, size-1, 0, 0, "NO FILES HERE");} /* it doesn't enter here when the folder is empty */
	if (lendir <= 0) return;

	if (realpath(pathtodraw, resolvedpath) == NULL) resolvedpath[0] = 0;

	i = 2;
	if (sortbydirectories) {
		/* draw first the directories */
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", resolvedpath, name);
			if (stat(statpath, &pathstat) == 0 && S_ISDIR(pathstat.st_mode) && \
					strcmp(name, ".") != 0 && \
					strcmp(name, "..") != 0 && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {
				
				p = strrchr(comingfrom, '/');
				if (p == NULL) p = comingfrom+strlen(comingfrom);
				else p += 1;

				if ((direction && i == 2) || (!direction && strcmp(name, p) == 0)) {
					PRINTW(7, i, column, size-1, isselected(resolvedpath, name), 1, name);
					if (highlightedname) strncpy(highlightedname, name, NAME_MAX);
				} else if (isselected(resolvedpath, name)) {
					PRINTW(4, i, column, size-1, 1, 1, name);
				} else {
					PRINTW(3, i, column, size-1, 0, 1, name);
				}
				i++;
			}
		}
		/* then put all other files */
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", resolvedpath, name);
			if (stat(statpath, &pathstat) == 0 && !S_ISDIR(pathstat.st_mode) && \
					strcmp(name, ".") != 0 && \
					strcmp(name, "..") != 0 && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {

				if (direction && i == 2) {
					PRINTW(7, i, column, size-1, isselected(resolvedpath, name), 0, name);
					if (highlightedname) strncpy(highlightedname, name, NAME_MAX);
				} else if (isselected(resolvedpath, name)) {
					PRINTW(2, i, column, size-1, 1, 0, name);
				} else {
					PRINTW(1, i, column, size-1, 0, 0, name);
				}
				i++;
			}
		}
		/* then free each element in namelist */
		for (j = 0; j < lendir; j++) {
			free(namelist[j]);
		}
	} else {
		for (j = 0; j < lendir && i < maxy-2; j++) {
			strncpy(name, namelist[j]->d_name, NAME_MAX); /* otherwise it gives a segmentation fault and i have no idea why */
			snprintf(statpath, PATH_MAX, "%s/%s", resolvedpath, name);
			if (strcmp(name, ".") != 0 && \
					strcmp(name, "..") && \
					(hiddenfiles == 0 ? name[0] != '.' : 1)) {

				p = strrchr(comingfrom, '/');
				if (p == NULL) p = comingfrom+strlen(comingfrom);
				else p += 1;

				overwrite = 0;
				if ((direction && i == 2) || (!direction && strcmp(name, p) == 0)) {
					overwrite = 7;
					if (highlightedname) strncpy(highlightedname, name, NAME_MAX);
				}

				if (stat(statpath, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
					if (isselected(resolvedpath, name)) {
						PRINTW(MAX(overwrite, 4), i, column, size-1, 1, 1, name);
					} else {
						PRINTW(MAX(overwrite, 3), i, column, size-1, 0, 1, name);
					}
				} else {
					if (isselected(resolvedpath, name)) {
						PRINTW(MAX(overwrite, 2), i, column, size-1, 1, 0, name);
					} else {
						PRINTW(MAX(overwrite, 1), i, column, size-1, 0, 0, name);
					}
				}

				i++;
			}
			free(namelist[j]);
		}
	}
	free(namelist);

	if (i == 2) {
		PRINTW(5, 2, column, size-1, 0, 0, "NO FILES");
	}

}

int
iscurrentonscreen(void)
{
	return topofscreen <= current && current < topofscreen+maxy-4;
}

char*
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

char*
escapestring(char *str, size_t n)
{
	char escapechars[256] = {0};
	int i, k = 0;
	char *buf = (char *)malloc(n);

	escapechars['\a'] = 1;
	escapechars['\b'] = 1;
	escapechars['\f'] = 1;
	escapechars['\n'] = 1;
	escapechars['\r'] = 1;
	escapechars['\t'] = 1;
	escapechars['\v'] = 1;
	escapechars['\\'] = 1;
	escapechars['\?'] = 1;

	for (i = 0; str && i < n && k < n; i++) {
		if (escapechars[str[i]]) {
			buf[k++] = '\\';
			
			switch (str[i]) {
				case '\a': buf[k++] = 'a'; break;
				case '\b': buf[k++] = 'b'; break;
				case '\f': buf[k++] = 'f'; break;
				case '\n': buf[k++] = 'n'; break;
				case '\r': buf[k++] = 'r'; break;
				case '\t': buf[k++] = 't'; break;
				case '\v': buf[k++] = 'v'; break;
				case '\\': buf[k++] = '\\'; break;
				case '\?': buf[k++] = '?'; break;
			}
		} else {
			buf[k++] = str[i];
		}
	}
	buf[k] = 0;

	strncpy(str, buf, n);
	free(buf);
	return str;
}

void
loop(void)
{
	int c, i;
	rdrwf();
	while ((c = getch()) != QUIT_CHAR) {
		if (c == KEY_RESIZE) {
			resizedetected();
		} else {
			status[0] = 0;
		}

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
	freelistcontents(&fileslist);
	freelistcontents(&selected);
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


void
movev(const Arg *arg)
{
	if (!arg) return;
	if (!fileslist.contents) return;

	int i = arg->i, j = 0;

	if (i == 1) {
		if (current+1 <= fileslist.end) current++;
	} else if (i == -1) {
		if (current-1 >= 1) current--;
	} else if (i == 2) {
		if (current + maxy/2 <= fileslist.end) current += maxy/2;
		else current = fileslist.end;
	} else  if (i == -2) {
		if (current - maxy/2 >= 1) current -= maxy/2;
		else current = 1;
	}
}

void
moveh(const Arg *arg)
{
	struct stat pathstat;
	char oldpattern[PATH_MAX], *p;
	Arg searcharg = {.i = 0};
	int tosearch = 0;

	memset(oldpattern, 0, sizeof(oldpattern));

	if (!arg) return;
	if (strcmp(cwd, "/") == 0 && arg->i == -1) return;
	
	if (arg->i == -1) {
		/* for a single level undo */
		p = strrchr(cwd, '/');

		if (p && p[1] != 0) {
			tosearch = 1;
			strncpy(oldpattern, pattern, PATH_MAX);
			strncpy(pattern, p+1, PATH_MAX);
		}

		chdir("..");
	} else {
		if (stat(fileslist.contents[current].name, &pathstat) == 0 && S_ISDIR(pathstat.st_mode)) {
			chdir(fileslist.contents[current].name);
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
	current = 1;
}

void
last(const Arg *arg)
{
	if (fileslist.contents == NULL) return;
	current = fileslist.end;
}

void
topofscreenscroll(const Arg *arg)
{
	if (!arg || arg->i == 0) return;

	int i = arg->i;

	if (i == 1) {
		if (topofscreen + 1 <= fileslist.end && topofscreen+1 <= current) topofscreen++;
	} else if (i == -1) {
		if (current <= topofscreen+maxy-3) topofscreen--;
	}
}

void
drawratiosscroll(const Arg *arg)
{
	if (!arg || arg->i == 0) {
		strncpy(status, "define the arguments for drawrationscroll", NAME_MAX);
		return;
	}

	cratio = ((cratio+arg->i) % LENGTH(drawratios));

	if (cratio < 0) {
		cratio = (LENGTH(drawratios)+cratio);
	}

	strncpy(status, "changed the draw ratio", NAME_MAX);
}

void
toggleselect(const Arg *arg)
{
	if (!fileslist.contents) {
		strncpy(status, "no current file/no files in directory", NAME_MAX);
		return;
	}

	if (isselected(fileslist.contents[current].path, fileslist.contents[current].name)) {
		rmvselection(fileslist.contents[current].path, fileslist.contents[current].name);
	} else {
		addelem(&selected, fileslist.contents[current].path, fileslist.contents[current].name);
	}
	strncpy(status, "changed selected", NAME_MAX);
}

void
clearselection(const Arg *arg)
{
	freelistcontents(&selected);
	strncpy(status, "cleared selected", NAME_MAX);
}

void
selectall(const Arg *arg)
{
	int i = 1;
	for (i = 1; i <= fileslist.end; i++) {
		if (!isselected(fileslist.contents[i].path, fileslist.contents[i].name)) {
			addelem(&selected, fileslist.contents[i].path, fileslist.contents[i].name);
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
search(const Arg *arg)
{
	if (!fileslist.contents) {
		strncpy(status, "no files in current directory", NAME_MAX);
		return;
	}

	regex_t regex;
	int reti, oktofree = 0, i = current;

    switch (arg->i) {
    case 0: /* FALLTHROUGH */
    case 1: if (!oktofree && pattern[0] != 0) {
                reti = regcomp(&regex, pattern, 0);
                 if (reti) return;
                 oktofree = 1;
            } else if (!oktofree) {
               strncpy(status, "please input a pattern", NAME_MAX);
               return;
            }
            
            if (arg->i == 1) i++;  /* so it doesn't detect the same element if we want the next one
             		                * but in case we search with a new pattern, we want it to match the current
             		                * position if it matches
             		                */
            
            for (; i <= fileslist.end; i++) {
               reti = regexec(&regex, fileslist.contents[i].name, 0, NULL, 0);
               if (!reti) break;
            }
            
            if (i > fileslist.end) {
                i = 1;
                strncpy(status, "search reached BOTTOM, starting from the topofscreen", NAME_MAX);
                
				for (; i <= fileslist.end; i++) {
                   reti = regexec(&regex, fileslist.contents[i].name, 0, NULL, 0);
                   if (!reti) break;
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
    
             i--;

             for (; i >= 1; i--) {
                reti = regexec(&regex, fileslist.contents[i].name, 0, NULL, 0);
                if (!reti) break;
             }
             
             if (i == 0) {
			 	 i = fileslist.end;
                 strncpy(status, "search reached topofscreen, starting from the BOTTOM", NAME_MAX);
                 
             	 for (; i >= 1; i--) {
             	    reti = regexec(&regex, fileslist.contents[i].name, 0, NULL, 0);
             	    if (!reti) break;
             	 }
             }
             break;
    }

	if (oktofree) {
		regfree(&regex);
	}

	if (i <= 0 || i > fileslist.end) {
		strncpy(status, "no item with that pattern was found", NAME_MAX);
		i = current;
	}

	initialization();
	current = i;

	/* put the result in the middle */
	if (!iscurrentonscreen()) {
		topofscreen = current-maxy/2;
		if (topofscreen < 1) topofscreen = 1;
		if (topofscreen > fileslist.end) topofscreen = fileslist.end;
	}
}

void
executecommand(const Arg *arg)
{
	char input[NAME_MAX], inputcommand[PATH_MAX], command[COMMAND_MAX], toconcat[PATH_MAX], coutput[PATH_MAX] = {0}, buf[PATH_MAX], oldpattern[PATH_MAX], chr;
	int i, k = 0, j = 1, cpid, pipefd[2] = {0}, readok = 0, cstatus = 0;
	Arg searcharg = {.i = 0};
	
	if (!(arg->i & NoEndWinMaskBACKEND)) endwin();

	if (!arg || !arg->v) {
		printf("your command: ");
		fgets(inputcommand, PATH_MAX, stdin);
		if (inputcommand[strlen(inputcommand)-1] == '\n') inputcommand[strlen(inputcommand)-1] = 0;
	} else {
		strncpy(inputcommand, *((char **)arg->v), PATH_MAX);
	}
	
	for (i = 0; inputcommand[i] != 0 && k < PATH_MAX - 1 && i < COMMAND_MAX; i++) {
		if (inputcommand[i] == '%') {
			if (inputcommand[i+1] == '%') {
				command[k] = '%';
				k++;
				i++;
			} else if (inputcommand[i+1] == 'c') {
				if (!fileslist.contents) {
					strncpy(status, "directory is empty - no current file set", NAME_MAX);
					goto skipexecutecommand;
				}
				strncat(command, fileslist.contents[current].name, COMMAND_MAX-strlen(fileslist.contents[current].name)-1);
				k = strlen(command);
				i++;
			} else if (inputcommand[i+1] == 's') {
				if (!selected.contents) {
					strncpy(status, "nothing selected", NAME_MAX);
					goto skipexecutecommand;
				}

				for (j = 1; j <= selected.end; j++) {
					if (j+1 <= selected.end) snprintf(toconcat, PATH_MAX, "%s/%s ", selected.contents[j].path, selected.contents[j].name);
					else snprintf(toconcat, PATH_MAX, "%s/%s", selected.contents[j].path, selected.contents[j].name);
					strncat(command, toconcat, COMMAND_MAX-strlen(toconcat)-1);
					toconcat[0] = 0;
					k = strlen(command);
				}
				i++;

			}
		} else {
			command[k] = inputcommand[i];
			k++;
			command[k] = 0;
		}
	}

	if (!(arg->i & NoConfirmationMask)) printf("are you sure you want to execute the command '%s' [yes/No]: ", command);
	if (!(arg->i & NoConfirmationMask)) fgets(input, NAME_MAX, stdin);

	if (arg->i & NoConfirmationMask || input[0] == 'y' || input[0] == 'Y') {
		if (pipe(pipefd) != 0) {
			snprintf(status, NAME_MAX, "pipe error when trying to execute '%s'", command);
			goto skipexecutecommand;
		}

		if ((cpid = fork()) == 0) {
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);

			execl("/bin/sh", "sh", "-c", command, (char *)NULL);
			_exit(1);
		} else if (cpid > 0) {
			close(pipefd[1]);

			k = 0;
			while (read(pipefd[0], &chr, sizeof(char)) > 0) {
				putchar(chr);
				fflush(stdout);

				if (k == PATH_MAX-1 || chr == '\n') {
					buf[k] = 0;
					strncpy(coutput, buf, PATH_MAX);
					readok = 1;
					k = 0;
				} else {
					buf[k++] = chr;
				}
			}
			buf[k] = 0;
			if (k != 0) strncpy(coutput, buf, PATH_MAX);

			if (wait(&cstatus) != cpid || cstatus != 0) {
				snprintf(status, NAME_MAX, "error on wait or the child process exited with non-zero status when executing '%s'", command);
				readok = 0;
			}

			close(pipefd[0]);
		}

		if (cpid < 0) snprintf(status, NAME_MAX, "error on fork() when trying to execute '%s'", command);
		else if (!(arg->i & NoEndWinMaskBACKEND)) snprintf(status, NAME_MAX, "executed the command '%s'", command);
	} else if (!(arg->i & NoEndWinMaskBACKEND)) {
		snprintf(status, NAME_MAX, "didn't execute the command '%s'", command);
	}

skipexecutecommand:
	if (!(arg->i & NoEndWinMaskBACKEND) && !(arg->i & NoWaitUntilKeyPress)) printf("press any key to continue\n");
	if (!(arg->i & NoEndWinMaskBACKEND) && !(arg->i & NoWaitUntilKeyPress)) fgetc(stdin);

	if (!(arg->i & NoEndWinMaskBACKEND)) initialization();
	if (!(arg->i & NoReloadMask)) getcurrentfiles();

	if (readok && arg->i & SearchLastLineMask) {
		strncpy(oldpattern, pattern, PATH_MAX);
		strncpy(pattern, coutput, PATH_MAX);

		search(&searcharg);
		printf("%s\n", pattern);
		if (arg->i & NoSaveSearchMask) strncpy(pattern, oldpattern, PATH_MAX);
	}

	if (arg->i & CdLastLineMask) {
		chdir(coutput);
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
