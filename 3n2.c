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
  mode_view,
  mode_search
};

int mode = mode_view,
    indx = 0,
    ndir = 0;
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

void put(){
  DIR *dir;
  struct dirent *ent;

  ndir = 0;
  dir = opendir(".");

  printf("\x1b[2J\x1b[0H");
  while((ent=readdir(dir))){
    ndir++;
    printf("%s\n", ent->d_name);
  }
  printf("\x1b[%i;1H", indx+1);
  fflush(stdout);

  closedir(dir);
}

void raw(){
  tcgetattr(STDIN_FILENO, &tio);
  tio_raw = tio;
  tio_raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio_raw);
}

void mod(int newmode){
  mode = newmode;
  
  if(newmode == mode_view){
  } else if(newmode == mode_search){
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
              if(indx > 0) indx--;
              put();
              break;
            case 'B': /* Down */
              if(indx < ndir) indx++;
              put();
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
        mod(mode_search);
        break;
    } 
  }
}

void end(){
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  exit(0);
}

/*
 * MAIN
 */

int main(){
  signal(SIGTERM, end);
  signal(SIGQUIT, end);
  signal(SIGINT,  end);

  put();
  raw();
  run();
  end();

  return 0;
}
