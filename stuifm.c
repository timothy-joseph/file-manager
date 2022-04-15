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

/* macros */
#define MAX_PATH 4096
#define MAX_NAME 255

#define LENGTH(X) (sizeof(X) / sizeof(X[0]))
#define PRINTW(X, STR, ISSEL, ISDIR) if ((ISSEL)) addch('>'); \
						  			 for (int j = 1; j <= maxx/2-strlen((STR))-(!!ISSEL); j++) { \
						  			     addch(' '); \
						  			 } \
						  			 attron(COLOR_PAIR((X))); /* colour - colour pair X */ \
						  			 if ((ISDIR)) printw("%s/\n", (STR)); \
						  			 else printw("%s\n", (STR)); \
						  			 attroff(COLOR_PAIR((X))); \

#define NUMOFDIGITS(RET, NUM, VAR) VAR = (NUM); \
								   RET = 0; \
								   while (VAR) { \
							           VAR /= 10; \
									   RET++; \
							       } \

#define VERSION "1.0"

/* types/structs */
typedef struct Node Node;
struct Node {
	char path[MAX_PATH];
	char name[MAX_NAME];
	Node *next, *prev;
};

typedef union Arg Arg;
union Arg {
	int i;
	char *s;
};

typedef struct Key Key;
struct Key {
	int chr; /* this is int not a char, but i feel like the name chr is more descriptive */
	void (*func)(const Arg *arg);
	const Arg arg;
};

/* function declarations */
static void initialization(void);
static void getcurentfiles(void);
static void rdrwf(void);
static void loop(void);
static void cleanup(void);
static void movev(const Arg *arg);
static void moveh(const Arg *arg);
static void search(const Arg *arg);
static void append_ll(Node **head, char *path, char *name);
static void free_ll(Node **head);
static void selection(const Arg *arg);
static void clearselection (const Arg *arg);
static void rmvselection(char *path, char *name);
static int  isselected(char *path, char *name);
static void movefiles(const Arg *arg);
static void copyfiles(const Arg *arg);
static void normrename(const Arg *arg);
static void brename(const Arg *arg);
static void directoriesfirst(const Arg *arg);
static void selectionmanager(const Arg *arg);
static void resizedetected(void);

/* global variables */
static Node *selhead = NULL;
static Node *dirlist = NULL;
static Node *curent = NULL; /* the file that the cursor is placed on */
static Node *topofscreen = NULL;

static int  maxy, maxx, sortbydirectories = 0, numoffiles = 0;
static char status[MAX_NAME];

/* config.h */
#include "config.h"

/* function definitions */
void
initialization(void)
{
	initscr();
	cbreak();
	noecho();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_WHITE, SELECTEDCOLOR);
	init_pair(3, DIRECTORYCOLOR, COLOR_BLACK);
	init_pair(4, DIRECTORYCOLOR, SELECTEDCOLOR);
	init_pair(5, COLOR_BLACK, COLOR_RED);
	scrollok(stdscr, 1);
	getmaxyx(stdscr, maxy, maxx);
}

void
getcurentfiles(void)
{
	/* free ll */
	free_ll(&dirlist);

	int lendir = -1, i;
	struct dirent **namelist;
	char cwd[MAX_PATH];
	struct stat pathstat;

	getcwd(cwd, sizeof(cwd));
	lendir = scandir(".", &namelist, 0, alphasort);

	if (lendir <= 0)
		return;

	if (sortbydirectories) {
		/* put all other files into dirlist first (because it's a linked list they will end up at the bottom) */
		for (i = lendir-1; i >= 0; i--) {
			stat(namelist[i]->d_name, &pathstat);
			if (S_ISREG(pathstat.st_mode) && \
					strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") != 0)
				append_ll(&dirlist, cwd, namelist[i]->d_name);
		}
		/* then put all other files */
		for (i = lendir-1; i >= 0; i--) {
			stat(namelist[i]->d_name, &pathstat);
			if (!S_ISREG(pathstat.st_mode) && \
					strcmp(namelist[i]->d_name, ".") != 0 && \
					strcmp(namelist[i]->d_name, "..") != 0)
				append_ll(&dirlist, cwd, namelist[i]->d_name);
		}
		/* then free each element in namelist */
		for (i = lendir-1; i >= 0; i--) {
			free(namelist[i]);
		}
	} else {
		for (i = lendir-1; i >= 0; i--) {
			/* insert into ll */
			if (strcmp(namelist[i]->d_name, ".") != 0 &&
					strcmp(namelist[i]->d_name, ".."))
				append_ll(&dirlist, cwd, namelist[i]->d_name);
			/* free element at i */
			free(namelist[i]);
		}
	}
	topofscreen = dirlist;
	curent = dirlist;
	/* free whole namelist */
	free(namelist);
	/* draw */
	// rdrwf();
}

void
rdrwf(void) /* (r)e(dr)a(w) (f)unction */
{
	struct stat pathstat;
	char cwd[MAX_PATH];
	getcwd(cwd, sizeof(cwd));
	int i, curentonscreen = 0, pos = 0, numoffiles = 0, posinfiles = 0, digitsfiles, digitspos, t1;
	Node *tmp = topofscreen;

	/* display */
	i = 0;

	clear();
	while (tmp != NULL && i < maxy-3) {
		/* decision on wheter the element is a directory and if it is selected */
		stat(tmp->name, &pathstat);
		if (!S_ISREG(pathstat.st_mode)) {
			if (isselected(cwd, tmp->name)) {
				PRINTW(4, tmp->name, 1, 1);
			} else {
				PRINTW(3, tmp->name, 0, 1);
			}
		} else {
			if (isselected(cwd, tmp->name)) {
				PRINTW(2, tmp->name, 1, 0);
			} else {
				PRINTW(1, tmp->name, 0, 0);
			}
		}

		if (tmp == curent) {
			/* TODO: add color for current */
			curentonscreen = 1;
			pos = i;
		}

		tmp = tmp->next;
		i++;
	}

	if (i == 0 || dirlist == NULL) {
		PRINTW(5, "NO FILES IN CURRENT DIRECTORY", 0, 0);
		curentonscreen = 1; /* to stop a possible infinite loop */
	}
	
	if (!curentonscreen) {
		topofscreen = curent;
		rdrwf();
	}
	/* finally add the cursor */
	mvaddch(pos, maxx/2+2, '<');

	/* print a line to separate files from the status */
	for (i = 0; i < maxx; i++) {
		mvaddch(maxy-2, i, '-');
	}

	/* print the number of files and what number is the current file */
	numoffiles = 0;
	posinfiles = 0;
	tmp = dirlist;
	while (tmp) {
		numoffiles++;
		if (tmp == curent) {
			posinfiles = numoffiles;
		}
		tmp = tmp->next;
	}

	NUMOFDIGITS(digitsfiles, numoffiles, t1);
	NUMOFDIGITS(digitspos, posinfiles, t1);

	mvprintw(maxy-1, maxx-digitspos-2-digitsfiles, "%d/%d", posinfiles, numoffiles);

	/* print the status */
	mvprintw(maxy-1, 0, "%.*s", maxx-digitspos-3-digitsfiles, status);
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
		for (i = 0; i < LENGTH(keys); i++)
			if (keys[i].chr == c)
				keys[i].func(&keys[i].arg);
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
	/* output the curent directory */
	FILE *fp = NULL;
	if ((fp = fopen(VCD_PATH, "w+")) == NULL)
		return;

	char cwd[MAX_PATH];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
		fprintf(fp, "%s\n", cwd);
	}
	
	fclose(fp);
}

void
movev(const Arg *arg)
{
	if (!arg)
		return;

	int i = arg->i, rdrw = 0, j = 0;
	Node *tmp = NULL;

	/* decide if we should go forward or backwards */
	if (i == 1)
		tmp = curent->next;
	else if (i == -1)
		tmp = curent->prev;

	if (tmp == NULL)
		return;
	
	curent = tmp;
}

void
moveh(const Arg *arg)
{
	if (!arg) /* arg == NULL */
		return;
	
	if (arg->i == -1) {
		chdir("..");
	} else {
		/* see if it is file or directory and act accordingly */
		struct stat pathstat;
		stat(curent->name, &pathstat);

		if (!S_ISREG(pathstat.st_mode))
			chdir(curent->name);
	}

	getcurentfiles();
	strncpy(status, "changed directory", MAX_NAME);
}

void
search(const Arg *arg)
{
	/* search through the ll until i find a element that starts with that
	 * then redraw the page starting with that element
	 */
	Node *tmp = curent;
	char pattern[MAX_PATH];
	regex_t regex;
	int reti;

	/* endwin */
	endwin();

	/* get pattern into normal string */
	printf("search: ");
	fgets(pattern, MAX_PATH, stdin);
	if (pattern[strlen(pattern)-1] == '\n') pattern[strlen(pattern)-1] = 0;

	/* get pattern into regex */
	reti = regcomp(&regex, pattern, 0);
	if (reti) return;

	while (tmp != NULL) {
		reti = regexec(&regex, tmp->name, 0, NULL, 0);
		if (!reti) break;
		tmp = tmp->next;
	}

	regfree(&regex);

	/* just in case we haven't found anything, go back to where the user left the cursor */
	if (tmp == NULL)
		tmp = curent;

	/* reinit */
	initialization();
	getcurentfiles();
	curent = tmp;
	strncpy(status, "searched", MAX_NAME);
}

void
append_ll(Node **head, char *path, char *name)
{
	/* used to add files to the selection ll */
	Node *tmp = (Node *)malloc(sizeof(Node));
	strncpy(tmp->path, path, MAX_PATH);
	strncpy(tmp->name, name, MAX_NAME);
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

void
selection(const Arg *arg)
{
	if (isselected(curent->path, curent->name)) {
		/* remove from list */
		rmvselection(curent->path, curent->name);
	} else {
		/* add it to the list */
		append_ll(&selhead, curent->path, curent->name);
	}
	strncpy(status, "changed selection", MAX_NAME);
}

void
clearselection(const Arg *arg)
{
	/* free the ll */
	free_ll(&selhead);
	strncpy(status, "cleared status", MAX_NAME);
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
movefiles(const Arg *arg)
{
	Node *tmp = selhead;
	char command[MAX_PATH] = "mv ";
	char cwd[MAX_PATH];
	getcwd(cwd, sizeof(cwd));

	/* end the window so we can get standard input/output */
	endwin();

	/* run the move command on all selected files */
	while (tmp != NULL) {
		/* create the command (yes i'm using the coreutils for this) */
		sprintf(command, "mv -v -i '%s/%s' '%s'", tmp->path, tmp->name, cwd);
		/* execute the command */
		system(command);
		
		tmp = tmp->next;
	}

	/* clear the selection and reinit*/
	clearselection(NULL);
	initialization();
	getcurentfiles();
}

void
copyfiles(const Arg *arg)
{
	/* same as move command, but with cp instead of mv */
	Node *tmp = selhead;
	char command[MAX_PATH];
	char cwd[MAX_PATH];
	getcwd(cwd, sizeof(cwd));

	endwin();

	while (tmp != NULL) {
		/* create the command (yes i'm using the coreutils for this) */
		sprintf(command, "cp -v -i -r '%s/%s' '%s'", tmp->path, tmp->name, cwd);
		/* execute the command */
		system(command);
		
		tmp = tmp->next;
	}

	clearselection(NULL);
	initialization();
	getcurentfiles();
}

void
normrename(const Arg *arg)
{
	char newname[MAX_PATH];
	/* endwin */
	endwin();
	/* get the new name */
	printf("the name of the new file: ");

	fgets(newname, MAX_PATH, stdin);
	if (newname[strlen(newname)-1] == '\n') newname[strlen(newname)-1] = 0; /* strip '\n' */

	/* move/rename */
	if (rename(curent->name, newname) == 0) {
		printf("file renamed\n");
	} else {
		printf("file not renamed\n");
	}
	/* maybe i'll remove this part, this one is for debugging */
	printf("press RETURN to continue");
	getchar();

	/* reinitialize */
	initialization();
	getcurentfiles();
}

void
brename(const Arg *arg)
{
	/* don't do anything if nothing is selected */
	if (selhead == NULL) return;

	/* endwin */
	endwin();

	Node *tmp = selhead;
	char command[MAX_PATH], newname[MAX_NAME];

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
	if ((gp = fopen(BRENAME_SCRIPT_PATH, "w+")) == NULL)
		return;

	fprintf(gp, "#!/bin/bash\n");

	tmp = selhead;

	/* get the new names line by line */
	while (tmp != NULL) {
		fgets(newname, MAX_NAME, fp);
		fprintf(gp, "mv -i %s/%s %s/%s", tmp->path, tmp->name, tmp->path, newname);
		tmp = tmp->next;
	}
	fclose(fp);
	fclose(gp);

	sprintf(command, "$EDITOR %s", BRENAME_SCRIPT_PATH);
	system(command);
	system(BRENAME_SCRIPT_PATH);

	/* finally clear the selection */
	clearselection(NULL);
	getcurentfiles();
}

void
directoriesfirst(const Arg *arg)
{
	sortbydirectories = !sortbydirectories;
	getcurentfiles();
}

void
selectionmanager(const Arg *arg)
{
}

void
resizedetected(void)
{
	/* firstly end the win so we can reinit it */
	endwin();
	Node *tmp = curent;
	initialization();
	rdrwf();
	curent = tmp;
	strncpy(status, "window resized", MAX_NAME);
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
			/* TODO: show the default bindings */
			printf("for using it as a way to cd into a directory, put the following in your .bashrc:\n");
			printf("alias fm='stuifm; LASTDIR=`cat $HOME/.vcd`; cd \"$LASTDIR\"'\n");
			printf("and call the program using fm\n\n");
			printf("in order to use the bulkrename function, you need to define the $EDITOR environment variable with your prefered editor\n");
			return 0;
		} else  {
			chdir(argv[1]);
		}
	}

	sortbydirectories = DIRECTORIESFIRST;
	initialization();
	getcurentfiles();
	loop();
	cleanup();

	return 0;
}
