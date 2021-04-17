/*
 * 3n2.c: a very fast terminal file browser
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <signal.h>

struct termios tio, tio_raw;

void raw(){
  tcgetattr(STDIN_FILENO, &tio);
  tio_raw = tio;
  tio_raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio_raw);
}

void end(int sig){
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  exit(0);
}

int main(){
  int num;
  char buf[256];

  signal(SIGTERM, end);
  signal(SIGQUIT, end);
  signal(SIGINT,  end);

  raw();

  while((num=read(STDIN_FILENO, buf, sizeof(buf))) != 0){
    if(buf[0] == '\n') break;
  }

  end(0);

  return 0;
}
