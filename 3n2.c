/*
 * 3n2.c: a very fast terminal file browser
 *
 * TODO:
 *  - Viewport (scrolling)
 *  - Moving during search
 *  - Return value
 *  - Preview
 *  - Column view
 *  - Sorting
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

#define DIRECTORY "\x1b[36m "
#define EXECUTABLE "\x1b[32m "
#define REGULAR "\x1b[0m "

#define CLEAN(f) free(f->name);free(f->stat);free(f)

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

void gen(file **f, struct dirent *ent){
  *f = malloc(sizeof(file));
  (*f)->name = strdup(ent->d_name);
  (*f)->stat = malloc(sizeof(struct stat));
  stat(ent->d_name, (*f)->stat);
  if(S_ISDIR((*f)->stat->st_mode)){
    (*f)->fmt = DIRECTORY;
    strcat((*f)->name, "/");
  } else if((*f)->stat->st_mode & 0100){
    (*f)->fmt = EXECUTABLE;
  } else {
    (*f)->fmt = REGULAR;
  }
}

void put(int type){
  int i, oldndir;
  char cwd[PATH_MAX];
  DIR *dir;
  struct dirent *ent;

  if(type == full){
    oldndir = ndir;
    indx = 0;
    prev = 0;
    ndir = 0;
    dir = opendir(".");
    getcwd(cwd, sizeof(cwd));

    fputs("\x1b[2J\x1b[0H\x1b[?25l", stdout);
    puts(cwd);
    puts("");
    while((ent=readdir(dir))){
      if(ent->d_name[0] == '.') continue;
      if(ndir > szfl) files = realloc(files, (szfl=ndir*2));

      if(ndir < oldndir){
        CLEAN(files[ndir]);
      }

      gen(&files[ndir], ent);

      fputs(files[ndir]->fmt, stdout);
      puts(files[ndir]->name);
      ndir++;
    }
    printf("\x1b[%i;1H%s\x1b[7m%s\x1b[0m", indx+3, files[indx]->fmt, files[indx]->name);
    fflush(stdout);

    closedir(dir);
  } else if(type == line){
    printf("\x1b[%i;1H\x1b[0m%s%s\x1b[%i;1H%s\x1b[7m%s\x1b[0m", prev+3, files[prev]->fmt, files[prev]->name, indx+3, files[indx]->fmt, files[indx]->name);
    fflush(stdout);
  } else {
    indx = 0;
    prev = 0;

    fputs("\x1b[3H\x1b[J\x1b[?25l", stdout);
    for(i=0;i<ndir;i++){
      if(strstr(files[i]->name, out) != NULL){
        fputs(files[i]->fmt, stdout);
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

  files = malloc(szfl*sizeof(file));
}

void mod(int newmode){
  int i;
  mode = newmode;
  
  if(newmode == view){
    fputs("\x1b[3H\x1b[J\x1b[?25l", stdout);
    for(i=0;i<ndir;i++){
      fputs(files[i]->fmt, stdout);
      puts(files[i]->name);
    }
    put(line);
  } else if(newmode == search){
    printf("\x1b[%i;H%s%s\x1b[%i;1H", indx+3, files[indx]->fmt, files[indx]->name, ws.ws_row);
    ledit("\x1b[?25hsearch: ", 8);
    mod(view);
  }
}

void dir(int way){
  if(way == down){
    chdir("..");
  } else if(S_ISDIR(files[indx]->stat->st_mode)){
    chdir(files[indx]->name);
  } else return;

  put(full);
}

void run(){
  int num;
  char buf[256];
  while((num=read(STDIN_FILENO, buf, sizeof(buf))) != 0){
    switch(buf[0]){
      case '\n':
        dir(up);
        break;
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
