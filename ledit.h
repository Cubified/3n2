/*
 * ledit.c: a line editor
 *
 * Reference implementation -- this was written
 * before ledit.asm
 */

#ifndef __LEDIT_H
#define __LEDIT_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#define LEDIT_MAXLEN 255
#define LEDIT_HIST_INITIALSIZE 16

#define LENGTH(x) (sizeof(x)/sizeof(x[0]))

/* Fancy pseudo-closure C idiom */
#define set_cursor() \
  do { \
    printf("\x1b[%iG", prompt_len+cur+nread+1); \
    fflush(stdout); \
  } while(0)
#define redraw(is_final) \
  do { \
    LEDIT_HIGHLIGHT(is_final); \
    printf("\x1b[0m\x1b[0G\x1b[2K%s%s", prompt, out); \
    printf("\x1b[%iG", prompt_len+cur+nread+1); \
    fflush(stdout); \
  } while(0)
#define move_word(dir) \
  do { \
    int pos = cur, \
        lim = (dir==1 ? out_len : 0); \
    while((dir==1 ? pos < lim : pos > lim)){ \
      pos += dir; \
      if(out[pos] == ' ' || \
         out[pos] == '_' || \
         out[pos] == '(' || \
         out[pos] == ')' || \
         out[pos] == '/' || \
         out[pos] == ','){  \
        break; \
      } \
    } \
    cur = pos; \
    set_cursor(); \
  } while(0)

char out[LEDIT_MAXLEN],
     **history = NULL;
int history_len = 1, /* Size of allocated array */
    history_ind = 0, /* Current entry in array */
    history_pos = 0, /* Current entry while scrolling through history */
    out_len = 0,
    cur = 0;

void LEDIT_HIGHLIGHT();

char *ledit(char *prompt, int prompt_len){
  char buf[LEDIT_MAXLEN];
  int nread;
  struct termios tio, raw;

  /* Enter terminal raw mode */
  tcgetattr(STDIN_FILENO, &tio);
  raw = tio;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  memset(out, '\0', LEDIT_MAXLEN);
  memset(buf, '\0', LEDIT_MAXLEN);

  if(history == NULL) history = malloc(sizeof(char*)*history_len);
  history_pos = history_ind;

  /* Clear line, print prompt */
  out_len = 0;
  nread = 0;
  cur = 0;
  redraw(0);
  
  /* Main loop */
  while((nread=read(STDIN_FILENO, buf, LEDIT_MAXLEN)) > 0){
    switch(buf[0]){
      case '\n':
      case '\r':
        goto shutdown;
      case '\x1b':
        if(nread == 1) goto shutdown;
        nread = 0;
        if(buf[1] == '['){
          switch(buf[2]){
            case 'A': /* Up arrow */
              if(history_pos > 0){
                history_pos--;
                strcpy(out, history[history_pos]);
                out_len = strlen(out);
                cur = out_len;
                redraw(1);
              }
              break;
            case 'B': /* Down arrow */
              if(history_pos+1 == history_ind){
                history_pos++;
                memset(out, '\0', LEDIT_MAXLEN);
                cur = 0;
                redraw(1);
                break;
              } else if(history_pos < history_ind){
                history_pos++;
                strcpy(out, history[history_pos]);
                out_len = strlen(out);
                cur = out_len;
                redraw(1);
              }
              break;
            case 'C': /* Right arrow */
              if(cur < out_len){
                cur++;
                set_cursor();
              } else {
                redraw(-1);
                out_len = strlen(out);
                cur = out_len;
                redraw(0);
              }
              break;
            case 'D': /* Left arrow */
              if(cur > 0){
                cur--;
                set_cursor();
              }
              break;
            case '1':
              if(buf[3] == '~'){ /* Home */
                cur = 0;
                set_cursor();
              } else if(buf[3] == ';'){ /* Ctrl+... */
                if(buf[5] == 'C'){ /* ...Right */
                  move_word(1);
                } else if(buf[5] == 'D'){ /* ...Left */
                  move_word(-1);
                }
              }
              break;
            case '3': /* Delete */
              if(cur < out_len){
                memmove(out+cur, out+cur+1, out_len-cur);
                out_len--;
                redraw(0);
              }
              break;
            case '4': /* End */
              cur = out_len;
              set_cursor();
              break;
          }
        }
        break;
      case 0x7f: /* Backspace */
        if(out_len > 0 &&
           cur > 0){
          nread = 0;
          cur--;
          memmove(out+cur, out+cur+1, out_len-cur);
          out_len--;
          redraw(0);
        }
        break;
      case 0x01: /* Ctrl+A (same as home key) */
        nread = 0;
        cur = 0;
        set_cursor();
        break;
      case 0x03: /* Ctrl+C (but only if sommething is wrong) */
        goto shutdown;
      case 0x05: /* Ctrl+E (same as end key) */
        nread = 0;
        cur = out_len;
        set_cursor();
        break;
      case 0x09: /* Tab */
        nread = 0;
        redraw(-1);
        out_len = strlen(out);
        cur = out_len;
        redraw(0);
        break;
      default:
        buf[nread] = '\0';
        memmove(out+cur+nread, out+cur, out_len-cur);
        strncpy(out+cur, buf, nread);
        out_len++;

        redraw(0);

        cur+=nread;
        break;
    }
  }

shutdown:;
  redraw(1);
  history[history_ind++] = strdup(out);
  if(history_ind == history_len){
    history_len *= 2;
    history = realloc(history, sizeof(char*)*history_len);
  }
  printf("\x1b[0m\n");
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

  return out;
}

#endif /* __LEDIT_H */
