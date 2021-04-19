/*
 * 3n2.c: a very fast terminal file browser
 *
 * TODO:
 *  - Viewport (scrolling)
 *  - Moving during search
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define LEDIT_HIGHLIGHT put
#include "ledit.h"

/*
 * GLOBAL VARIABLES
 * AND ENUMS
 */

#define SZFL_INIT 64

#define DIRECTORY "\x1b[36m"
#define EXECUTABLE "\x1b[32m"
#define REGULAR "\x1b[0m"

enum {
  view,
  search
};

enum {
  srch = 0,
  srch_final = 1,
  full = 2,
  line = 3
};

enum {
  up,
  down
};

typedef struct {
  char *fmt;
  char *name;
  struct stat *stat;
} file;

int mode = view,
    indx = 0,
    prev = 0,
    ndir = 0,
    szfl = SZFL_INIT;
char cwd[PATH_MAX];
file **files;
struct termios tio, tio_raw;
struct winsize ws;

/*
 * STATIC
 * DEFINITIONS
 */

void gen();
void put();
void raw();
void mod();
void dir();
void run();
void end();

/*
 * FUNCTION
 * BODIES
 */

void gen(file **f, struct dirent *ent, struct stat *sb){
  *f = malloc(sizeof(file));
  (*f)->name = strdup(ent->d_name);
  (*f)->stat = sb;
  if(S_ISDIR(sb->st_mode)){
    (*f)->fmt = DIRECTORY;
    strcat((*f)->name, "/");
  } else if(sb->st_mode & 0100){
    (*f)->fmt = EXECUTABLE;
  } else {
    (*f)->fmt = REGULAR;
  }
}

void put(int type){
  int i;
  char tmp[256];
  DIR *dir;
  struct dirent *ent;
  struct stat *sb;

  if(type == full){
    indx = 0;
    prev = 0;
    ndir = 0;
    dir = opendir(cwd);

    fputs("\x1b[2J\x1b[0H\x1b[?25l", stdout);
    while((ent=readdir(dir))){
      if(ent->d_name[0] == '.') continue;
      if(ndir > szfl) files = realloc(files, (szfl=ndir*2));

      sprintf(tmp, "%s/%s", cwd, ent->d_name);
      sb = malloc(sizeof(struct stat));
      stat(tmp, sb);
      gen(&files[ndir], ent, sb);

      fputs(files[ndir]->fmt, stdout);
      puts(files[ndir]->name);
      ndir++;
    }
    printf("\x1b[%i;1H%s\x1b[7m%s\x1b[0m", indx+1, files[indx]->fmt, files[indx]->name);
    fflush(stdout);

    closedir(dir);
  } else if(type == line){
    printf("\x1b[%i;1H\x1b[0m%s%s\x1b[%i;1H%s\x1b[7m%s\x1b[0m", prev+1, files[prev]->fmt, files[prev]->name, indx+1, files[indx]->fmt, files[indx]->name);
    fflush(stdout);
  } else {
    indx = 0;
    prev = 0;

    fputs("\x1b[2J\x1b[0H\x1b[?25l", stdout);
    for(i=0;i<ndir;i++){
      if(strstr(files[i]->name, out) != NULL){
        puts(files[i]->name);
      }
    }
    printf("\x1b[%i;1H", ws.ws_row);
    fflush(stdout);
  }
}

void raw(){
  tcgetattr(STDIN_FILENO, &tio);
  tio_raw = tio;
  tio_raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio_raw);
  setvbuf(stdout, NULL, _IOFBF, 4096);

  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

  realpath(".", cwd);

  files = malloc(szfl*sizeof(file));
}

void mod(int newmode){
  int i;
  mode = newmode;
  
  if(newmode == view){
    fputs("\x1b[2J\x1b[0H\x1b[?25l", stdout);
    for(i=0;i<ndir;i++){
      puts(files[i]->name);
    }
    put(line);
  } else if(newmode == search){
    printf("\x1b[%i;H\x1b[0m%s\x1b[%i;1H", indx+1, files[indx]->name, ws.ws_row);
    ledit("\x1b[?25hsearch: ", 8);
    mod(view);
  }
}

void dir(int way){
  char tmp[256];
  
  strcpy(tmp, cwd);
  strcat(tmp, "/");
  strcat(tmp, (way == up ? files[indx]->name : ".."));
  if(way == down ||
     S_ISDIR(files[indx]->stat->st_mode)){
    if(strlen(tmp) >= sizeof(cwd)) realpath(tmp, cwd);
    else strcpy(cwd, tmp);
    put(full);
  }
}

void run(){
  int num;
  char buf[256];
  while((num=read(STDIN_FILENO, buf, sizeof(buf))) != 0){
    switch(buf[0]){
      case '\n':
      case 'q':
        end();
        break;
      case '\x1b': /* Escape sequence */
        if(num >= 2 &&
           buf[1] == '['){
          switch(buf[2]){
            case 'A': /* Up */
              prev = indx;
              if(indx > 0) indx--;
              else indx = ndir-1;
              put(line);
              break;
            case 'B': /* Down */
              prev = indx;
              if(indx < ndir-1) indx++;
              else indx = 0;
              put(line);
              break;
            case 'C': /* Right */
              dir(up);
              break;
            case 'D': /* Left */
              dir(down);
              break;
            case '5': /* Page Up */
              prev = indx;
              indx -= ws.ws_row;
              if(indx < 0) indx = 0;
              put(line);
              break;
            case '6': /* Page Down */
              prev = indx;
              indx += ws.ws_row;
              if(indx > ndir-1) indx = ndir-1;
              put(line);
              break;
          }
        } else { /* Plain escape key */
          end();
        }
        break;
      case '/': /* Search */
        mod(search);
        break;
    } 
  }
}

void end(){
  int i;
  for(i=0;i<szfl;i++){
    if(i < ndir){
      free(files[i]->name);
      free(files[i]->stat);
    }
    free(files[i]);
  }
  free(files);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  fputs("\x1b[0H\x1b[2J\x1b[?25h", stdout);

  exit(0);
}

/*
 * MAIN
 */

int main(){
  signal(SIGTERM, end);
  signal(SIGQUIT, end);
  signal(SIGINT,  end);

  raw();
  put(full);
  run();
  end();

  return 0;
}
