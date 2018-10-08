#include <sys/timerfd.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

void display(unsigned long exp) {
  static unsigned long oldcolor, direction = -1;
  printf("\e[48;2;%d;0;0m \r", oldcolor);
  if (oldcolor <= 0 || oldcolor >= 255)
    direction *= -1;
  oldcolor += direction * exp * 5;
}

void quit(int sig) {
  printf("\e[m\e[?25h\n");
  system("stty echo echoctl");
  exit(0);
}

int main() {
  system("stty -echo -echoctl");
  setvbuf(stdout, NULL, _IONBF, 0);
  signal(SIGINT, quit);
  printf("\e[m\e[?25l");
  int fd = timerfd_create(CLOCK_MONOTONIC, 0);
  struct timespec ti = { .tv_sec = 0, .tv_nsec = 16000000 };
  struct itimerspec it = { ti, ti };
  timerfd_settime(fd, 0, &it, 0);

  while (1) {
    unsigned long exp;
    read(fd, &exp, sizeof exp);
    display(exp);
  }
}
