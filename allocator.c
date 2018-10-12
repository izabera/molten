#include <stdlib.h>
#include <stdio.h>

struct thing {
  char data[11];
  char meow;
  int idx;
};

static struct thing *things, *clean, *recycled, *stop;
// allocating:
// if the clean list is empty, swap clean and recycled
// allocate blocks from the clean list
//
// freeing:
// add freed blocks to the recycled list

#define nelem 10

#if 0
#define debug(...)
#else
#define debug(x, ...) printf("\e[33m" x "\e[m", ##__VA_ARGS__)
#endif

void initthings(void) {
  things = malloc(nelem * sizeof *things);
  for (int i = 0; i < nelem-1; i++)
    things[i].idx = i + 1;

  clean = things;
  stop = recycled = things + nelem - 1;

  for (int i = 0; i < nelem-1; i++)
    sprintf(things[i].data, "{%d}", i);
  sprintf(stop->data, "{stop}");
}

struct thing *getthing(void) {
  if (clean == stop) {
    struct thing *tmp = clean;
    clean = recycled;
    recycled = tmp;
  }
  if (clean == stop) {
    debug("allocation failed\n");
    return stop;
  }

  struct thing *ret = clean;
  clean = things + ret->idx;
  debug("allocate %s\n", ret->data);
  return ret;
}

void freething(struct thing *thing) {
  if (thing == stop) return;
  debug("free %s\n", thing->data);
  thing->idx = recycled - things;
  recycled = thing;
}

void dump() {
  debug("clean: ");
  int count = 0;
  for (struct thing *t = clean; ; count++, t = things + t->idx) {
    debug("%s", t->data);
    if (t == stop) break;
    debug("->");
  }

  for (; count < nelem; count++)
    debug("     ");

  debug("recycled: ");
  for (struct thing *t = recycled; ; count++, t = things + t->idx) {
    debug("%s", t->data);
    if (t == stop) break;
    debug("->");
  }
  debug("\n");
}








#include <stdio.h>

int main() {
  initthings();

  dump();
  struct thing *meow[] = { getthing(), getthing(), getthing(), getthing(), getthing() };
  for (int i = 0; i < 5; i++)
    printf("meow[%d] = %s\n", i, meow[i]->data);

  dump();
  freething(meow[1]);
  dump();
  freething(meow[3]);
  dump();

  meow[1] = getthing();

  puts("====leak====");
  for (int i = 0; i < 5; i++)
    getthing();
  puts("====leak====");

  meow[3] = getthing();
  dump();

  for (int i = 0; i < 5; i++) {
    printf("meow[%d] = %s\n", i, meow[i]->data);
    freething(meow[i]);
  }
  dump();

  puts("free(get())");
  for (int i = 0; i < 15; i++) {
    freething(getthing());
    dump();
  }
}
