/*
 * 3n2.c: a very fast terminal file browser
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <signal.h>

#include "ledit.h"

/*
 * GLOBAL VARIABLES
 * AND ENUMS
 */

enum {
  view,
  search
};

enum {
  full,
  line
};

int mode = view,
    indx = 0,
    prev = 0,
    ndir = 0;
char *cwd = ".",
     *files[16];
struct termios tio, tio_raw;

/*
 * STATIC
 * DEFINITIONS
 */

void put();
void raw();
void mod();
void run();
void end();

/*
 * FUNCTION
 * BODIES
 */

void put(int type){
  DIR *dir;
  struct dirent *ent;

  if(type == full){
    ndir = 0;
    dir = opendir(cwd);

    printf("\x1b[2J\x1b[0H\x1b[?25l");
    while((ent=readdir(dir))){
      if(ent->d_name[0] == '.') continue;
      files[ndir] = strdup(ent->d_name); /* TODO: Memleak */
      ndir++;
      printf("%s\n", ent->d_name);
    }
    printf("\x1b[%i;1H\x1b[30;47m%s\x1b[0m", indx+1, files[indx]);
    fflush(stdout);

    closedir(dir);
  } else if(type == line){
    printf("\x1b[%i;1H\x1b[0m%s\x1b[%i;1H\x1b[30;47m%s\x1b[0m", prev+1, files[prev], indx+1, files[indx]);
    fflush(stdout);
  }
}

void raw(){
  tcgetattr(STDIN_FILENO, &tio);
  tio_raw = tio;
  tio_raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio_raw);
}

void mod(int newmode){
  mode = newmode;
  
  if(newmode == view){
  } else if(newmode == search){
    ledit("search: ", 8);
  }
}

void run(){
  int num;
  char buf[256];
  while((num=read(STDIN_FILENO, buf, sizeof(buf))) != 0){
    switch(buf[0]){
      case '\n':
        end();
        break;
      case '\x1b': /* Escape sequence */
        if(num >= 2 &&
           buf[1] == '['){
          switch(buf[2]){
            case 'A': /* Up */
              if(indx > 0){
                prev = indx;
                indx--;
                put(line);
              }
              break;
            case 'B': /* Down */
              if(indx < ndir-1){
                prev = indx;
                indx++;
                put(line);
              }
              break;
            case 'C': /* Right */
              break;
            case 'D': /* Left */
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
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  printf("\x1b[?25h\n");

  exit(0);
}

/*
 * MAIN
 */

int main(){
  signal(SIGTERM, end);
  signal(SIGQUIT, end);
  signal(SIGINT,  end);

  put(full);
  raw();
  run();
  end();

  return 0;
}
