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


static struct {
  unsigned short pos;
  char data[1000];
} buffer;
int needtodelete;


void backspace() { if (buffer.pos) { buffer.pos--; needtodelete = 1; } }
void pushchar(char c) { if (buffer.pos < sizeof buffer.data - 1) buffer.data[buffer.pos++] = c; }


static struct winsize w;


void display(unsigned long exp) {
  int written = 0;
  int down = 0;
  while (written < buffer.pos) {
    int towrite = buffer.pos - written;
    int space = w.ws_col - 3;
    if (space <= towrite) {
      written += fwrite(buffer.data + written, 1, space, stdout);
      puts("\r");
      down++;
    }
    else
      written += fwrite(buffer.data + written, 1, towrite, stdout);
  }
  putchar('_');
  if (needtodelete) {
    printf("\e[K\e[J");
    needtodelete = 0;
  }
  if (down) printf("\e[%dA", down);

  static unsigned long color, direction = -1;
  printf("\r"
         "\e[%dC"
         "\e[48;2;%d;0;0m \e[m"
         "\r",
         w.ws_col - 2,
         color);
  if (color <= 0 || color >= 255)
    direction *= -1;
  color += direction * exp * 5;

  fflush(stdout);
}


static struct termios oldterm, newterm;

void quit(int sig) {
  printf("\e[m\e[?25h\n");
  tcsetattr(0, TCSANOW, &oldterm);
  exit(0);
}


static void ctrl(char c) {
  switch (CTRL(c)) {
    case CTRL('C'):
      quit(0);
      break;
    case CTRL('J'):
    case CTRL('M'):
      printf("\r\n");
      ctrl('U');
      break;
    case CTRL('?'):
    case CTRL('H'):
      if (buffer.pos) {
        buffer.pos--;
        needtodelete = 1;
      }
      break;
    case CTRL('U'):
      buffer.pos = 0;
      needtodelete = 1;
      break;
    case CTRL('W'):
  }
}


int main() {
  tcgetattr(0, &oldterm);
  cfmakeraw(&newterm);
  tcsetattr(0, TCSANOW, &newterm);

  {
    struct rgb { int r, g, b; } fg, bg;
    char buf[50] = { };

    printf("\e]11;?\e\\");
    fflush(stdout);
    read(0, buf, sizeof buf);
    sscanf(buf, "\e]11;rgb:%o/%o/%o", &fg.r, &fg.g, &fg.b);

    printf("\e]10;?\e\\");
    fflush(stdout);
    read(0, buf, sizeof buf);
    sscanf(buf, "\e]11;rgb:%o/%o/%o", &fg.r, &fg.g, &fg.b);
  }


  setvbuf(stdout, NULL, _IOFBF, 0); // stupid libc, let me handle this
  printf("\e[m\e[?25l");
  fflush(stdout);


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
      needtodelete = 1;
    }
    if (FD_ISSET(timerfd, &fds)) {
      unsigned long exp;
      read(timerfd, &exp, sizeof exp);
      display(exp);
    }
  }
}
