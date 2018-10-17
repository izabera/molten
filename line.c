#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <termios.h>
#include <unistd.h>


FILE *target;
#define print(...) fprintf(target, ## __VA_ARGS__)

static struct {
  unsigned short pos;
  char data[1000];
} buffer;


static void pushchar(char c) {
  if (buffer.pos < sizeof buffer.data - 1)
    buffer.data[buffer.pos++] = c;
}


static struct winsize w;


static int display_line(void) {
  int written = 0;
  int down = 0;
  while (written < buffer.pos) {
    int towrite = buffer.pos - written;
    int space = w.ws_col - 1;
    if (space <= towrite) {
      written += fwrite(buffer.data + written, 1, space, target);
      print("\r\n");
      down++;
    }
    else
      written += fwrite(buffer.data + written, 1, towrite, target);
  }
  return down;
}

struct rgb { int r, g, b; } fg, bg, direction;
void display_cursor(int exp) {
  static struct rgb color;
  static int direction = -1;

  print("\e[48;2;%d;%d;%dm \e[m", color.r, color.g, color.b);
  print("\e[K\e[J");

  if (color.r <= 0 || color.r >= 255)
    direction *= -1;
  color.r += direction * exp * 5;
}

void display(int exp) {
  int down = display_line();

  display_cursor(exp);

  if (down) print("\e[%dA", down);

  print("\r");
  fflush(target);
}


static struct termios oldterm, newterm;

void quit(int sig) {
  print("\e[m\e[K\e[J\e[?25h");
  tcsetattr(0, TCSANOW, &oldterm);
  fwrite(buffer.data, 1, buffer.pos, stdout);
  printf("\n");
  exit(sig ? 128+sig : 0);
}



struct dictionary {
  int maxlen;
  struct {
    char **words;
    size_t nwords;
  } lists[];
} *dictionary;

void *get_dictionary(void *_) {
  FILE *dict = fopen("/usr/share/dict/words", "r");
  if (!dict) return 0;

  char *line = 0;
  size_t linesize = 0;
  ssize_t ret;

  int maxlen = 0;
  struct dictionary *loaded = 0;

  while ((ret = getline(&line, &linesize, dict)) > 0) {
    line[--ret] = 0;
    if (strcspn(line, "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != ret) {
      if (ret > maxlen) {
        loaded = realloc(loaded, sizeof *loaded + sizeof *(loaded->lists) * ret);
        memset(loaded->lists + maxlen, 0, sizeof *(loaded->lists) * (ret - maxlen));
        maxlen = ret;
      }
#define LIST loaded->lists[ret-1]
#define WORDS LIST.words
      WORDS = realloc(WORDS, ++(LIST.nwords) * sizeof(char *));
      WORDS[LIST.nwords-1] = strdup(line);
    }
  }
  free(line);

  loaded->maxlen = maxlen;
  dictionary = loaded;

  return 0;
}


void tabcomplete(void) {
  if (!dictionary) return;
  /*if (!buffer.pos) return;*/

  /*char *word = buffer.data + buffer.pos - 1;*/
  /*while (word > buffer.pos && *word != ' ') word--;*/

  /*char *completion = bsearch(word, completions, ncompletions, sizeof(char *), compare);*/
  /*if (!completion) puts("sad");*/
}


static void ctrl(char c) {
  switch (CTRL(c)) {
    case CTRL('C'):
      quit(2);
      break;
    case CTRL('D'):
    case CTRL('J'):
    case CTRL('M'):
      quit(0);
      break;
    case CTRL('?'):
    case CTRL('H'):
      if (buffer.pos)
        buffer.pos--;
      break;
    case CTRL('U'):
      buffer.pos = 0;
      break;
    case CTRL('W'):
      while (buffer.pos && buffer.data[buffer.pos-1] == ' ') buffer.pos--;
      while (buffer.pos && buffer.data[buffer.pos-1] != ' ') buffer.pos--;
    /*case CTRL('R'):*/
    case CTRL('I'):
      tabcomplete();
  }
}


int main() {
  target = stderr;

  pthread_t t;
  pthread_create(&t, 0, get_dictionary, 0);

  tcgetattr(0, &oldterm);
  cfmakeraw(&newterm);
  tcsetattr(0, TCSANOW, &newterm);

  {
    struct rgb fg, bg;
    char buf[50] = { };

    print("\e]11;?\e\\");
    fflush(target);
    read(0, buf, sizeof buf);
    sscanf(buf, "\e]11;rgb:%o/%o/%o", &fg.r, &fg.g, &fg.b);

    print("\e]10;?\e\\");
    fflush(target);
    read(0, buf, sizeof buf);
    sscanf(buf, "\e]10;rgb:%o/%o/%o", &fg.r, &fg.g, &fg.b);
  }


  setvbuf(target, NULL, _IOFBF, 0); // stupid libc, let me handle this
  print("\e[m\e[?25l");
  fflush(target);


  ioctl(0, TIOCGWINSZ, &w);


  // signalfd for sigwinch
  int sigfd;
  {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &set, 0);
    sigfd = signalfd(-1, &set, SFD_CLOEXEC);
  }


  // timer to update display and stuff
  int timerfd;
  {
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct timespec ti = { .tv_sec = 0, .tv_nsec = 16000000 };
    struct itimerspec it = { ti, ti };
    timerfd_settime(timerfd, 0, &it, 0);
  }




  int maxfd = timerfd > sigfd ? timerfd : sigfd;

  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_SET(sigfd, &fds);
    FD_SET(timerfd, &fds);

    select(maxfd + 1, &fds, 0, 0, 0);

    if (FD_ISSET(0, &fds)) {
      char c;
      read(0, &c, 1);
      if (!isascii(c)) { /* yeah not for now sorry */ }
      else {
        if (iscntrl(c)) ctrl(c);
        else pushchar(c);
      }
    }
    if (FD_ISSET(sigfd, &fds)) {
      struct signalfd_siginfo si;
      read(sigfd, &si, sizeof si);
      ioctl(0, TIOCGWINSZ, &w);
    }
    if (FD_ISSET(timerfd, &fds)) {
      unsigned long exp;
      read(timerfd, &exp, sizeof exp);
      display((int)exp);
    }
  }
}
