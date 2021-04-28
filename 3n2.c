/*
 * 3n2.c: a very fast terminal file browser
 *
 * TODO:
 *  - Viewport (scrolling)
 *  - Moving during search
 *  - Return value
 *  - Remove unncessary printfs/repeated puts calls
 *  - Performance improvements/optimizations
 *  - Hook into other applications for column view (affine, vex, bat, etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <pty.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define LEDIT_HIGHLIGHT put
#include "ledit.h"

// #include "quadsort/quadsort.h"

#include "config.h"

/*
 * GLOBAL VARIABLES
 * AND ENUMS
 */

#define SZFL_INIT 64
#define CLEAN(f) free(f->name);free(f->stat);free(f)
#define DOTEST(cmd) \
  sprintf(cwd, cmd, files[indx]->name); \
  fp = popen(cwd, "r"); \
  if(WEXITSTATUS(pclose(fp)) == 0) goto run;

enum {
  view,
  search
};

enum {
  srch = 0,
  srch_final = 1,
  full = 2,
  line = 3,
  colm = 4
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

extern char **environ;

int mode = view,
    indx = 0,
    prev = 0,
    ndir = 0,
    fcnt = 0,
    dcnt = 0,
    szfl = SZFL_INIT,
    pty_m;
pid_t pty;
file **files;
struct termios tio, tio_raw;
struct winsize ws, ws_pty;

/*
 * STATIC
 * DEFINITIONS
 */

int  cmp();
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

int cmp(const void *a, const void *b){
  int a_isdir = S_ISDIR((*(file**)a)->stat->st_mode),
      b_isdir = S_ISDIR((*(file**)b)->stat->st_mode);
  if(a_isdir != b_isdir) return (b_isdir - a_isdir);
  return strcmp((*(file**)a)->name, (*(file**)b)->name);
}

void gen(file **f, struct dirent *ent){
  *f = malloc(sizeof(file));
  (*f)->name = strdup(ent->d_name);
  (*f)->stat = malloc(sizeof(struct stat));
  stat(ent->d_name, (*f)->stat);
  if(S_ISDIR((*f)->stat->st_mode)){
    (*f)->fmt = DIRECTORY;
    strcat((*f)->name, "/");
    dcnt++;
  } else if((*f)->stat->st_mode & 0100){
    (*f)->fmt = EXECUTABLE;
    fcnt++;
  } else {
    (*f)->fmt = REGULAR;
    fcnt++;
  }
}

void put(int type){
  int i, oldndir,
      colsize = ws.ws_col / 2;
  char cwd[PATH_MAX];
  DIR *dir;
  FILE *fp;
  struct dirent *ent;
  struct tm *ts;

  if(type == full){
    oldndir = ndir;
    indx = 0;
    prev = 0;
    ndir = 0;
    fcnt = 0;
    dcnt = 0;
    dir = opendir(".");
    getcwd(cwd, sizeof(cwd));

    while((ent=readdir(dir))){
      if(ent->d_name[0] == '.') continue;
      if(ndir > szfl) files = realloc(files, (szfl=ndir*2));
      if(ndir < oldndir){
        CLEAN(files[ndir]);
      }
      gen(&files[ndir++], ent);
    }
    closedir(dir);

    // quadsort(files, ndir, sizeof(file*), cmp);
    qsort(files, ndir, sizeof(file*), cmp);

    printf("\x1b[2J\x1b[0H\x1b[?25l " CWD "%s " DIRCOUNT "(" DIRNUMBER "%i" DIRCOUNT " file%s, " DIRNUMBER "%i" DIRCOUNT " director%s)" SEPCOLOR "\n", cwd, fcnt, (fcnt == 1 ? "" : "s"), dcnt, (dcnt == 1 ? "y" : "ies"));
    for(i=0;i<ws.ws_col;i++){
      printf(HSEPARATOR);
    }
    puts("");
    for(i=0;i<ndir;i++){
      fputs(files[i]->fmt, stdout);
      puts(files[i]->name);
    }
    put(line);
    put(colm);
  } else if(type == line){
    ts = localtime(&files[indx]->stat->st_mtime);
    strftime(cwd, sizeof(cwd), "%a %Y-%m-%d %H:%M:%S %Z", ts);
    printf(
      "\x1b[%i;1H\x1b[0m%s%s\x1b[%i;1H%s\x1b[7m%s\x1b[0m\x1b[%i;%iH\x1b[1K\x1b[G" TIMETEXT "Last modified: " TIME "%s",
      prev+3, files[prev]->fmt, files[prev]->name, indx+3, files[indx]->fmt, files[indx]->name, ws.ws_row, colsize, cwd
    );
    put(colm);
  } else if(type == colm) {
    printf("\x1b[3;%iH" SEPCOLOR, colsize);
    for(i=3;i<=ws.ws_row;i++){
      printf(VSEPARATOR "\x1b[K\x1b[B\x1b[D");
    }
    printf("\x1b[1;%iH\x1b[K" VSEPARATOR CWD " %s", colsize, files[indx]->name);

    if(S_ISDIR(files[indx]->stat->st_mode)){
      dir = opendir(files[indx]->name);
      printf("\x1b[3;%iH", colsize+1);
      while((ent=readdir(dir))){
        if(ent->d_name[0] == '.') continue;

        printf(REGULAR "%s\x1b[E\x1b[%iG", ent->d_name, colsize+1);
      }
      fflush(stdout);
      closedir(dir);
    } else {
#ifdef HAS_AFFINE
      DOTEST("affine %s\n");
#endif

#ifdef HAS_BAT
      DOTEST("bat -Pf --style=plain %s\n");
#endif

#ifdef HAS_VEX
      DOTEST("vex %s n\n");
#endif

      sprintf(cwd, "cat %s\n", files[indx]->name);
run:;
      write(pty_m, cwd, strlen(cwd));
      while((oldndir=read(pty_m, cwd, sizeof(cwd))) > 0){
        printf("%s", cwd);
        fflush(stdout);
      }
    }

    fflush(stdout);
  } else if(type == SIGWINCH){
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    put(full);
  } else { /* Search */
    prev = 0;
    oldndir = 0;
    fputs("\x1b[3H\x1b[J\x1b[?25l", stdout);
    for(i=0;i<ndir;i++){
      if(strstr(files[i]->name, out) != NULL){
        fputs(files[i]->fmt, stdout);
        if(indx == i){
          oldndir = 1;
          fputs("\x1b[7m", stdout);
        }
        fputs(files[i]->name, stdout);
        puts("\x1b[0m");
      }
    }
    if(!oldndir) indx = 0;
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
  ws_pty.ws_col = ws.ws_col / 2;
  ws_pty.ws_row = ws.ws_row;
  if((pty=forkpty(&pty_m, NULL, NULL, &ws_pty))
       == -1){
    puts("\x1b[31mFailed to open pseudoterminal.\x1b[0m");
    exit(1);
  } else if(pty == 0){
    environ = malloc(sizeof(char*)*2);
    environ[0] = "PS1=magicmagic";
    environ[1] = NULL;
    execvp("sh", NULL);
    _exit(0);
  }

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

  fputs("\x1b[0m\x1b[0H\x1b[2J\x1b[?25h", stdout);

  exit(0);
}

/*
 * MAIN
 */

int main(){
  signal(SIGTERM,  end);
  signal(SIGQUIT,  end);
  signal(SIGINT,   end);
  signal(SIGWINCH, put);

  raw();
  put(full);
  run();
  end();

  return 0;
}
