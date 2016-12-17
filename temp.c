/*
 * temp.c - functions to create and manipulate temporary variables which are
 *          used in the IR tree representation before it has been determined
 *          which variables are to go into registers.
 *
 */

#include "temp.h"
#include "symbol.h"
#include "table.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Temp_temp_ {
  int num;
};

string
Temp_labelstring(Temp_label s)
{
  return S_name(s);
}

static int labels = 0;

Temp_label
Temp_newlabel(void)
{
  char buf[100];
  sprintf(buf, "L%d", labels++);
  return Temp_namedlabel(String(buf));
}

/* The label will be created only if it is not found. */
Temp_label
Temp_namedlabel(string s)
{
  return S_Symbol(s);
}

static int temps = 100;

Temp_temp
Temp_newtemp(void)
{
  Temp_temp p = (Temp_temp)checked_malloc(sizeof(*p));
  p->num = temps++;
  {
    char r[16];
    sprintf(r, "%d", p->num);
    Temp_enter(Temp_name(), p, String(r));
  }
  return p;
}

struct Temp_map_ {
  TAB_table tab;
  Temp_map under;
};

Temp_map
Temp_name(void)
{
  static Temp_map m = NULL;
  if (!m) m = Temp_empty();
  return m;
}

Temp_map
newMap(TAB_table tab, Temp_map under)
{
  Temp_map m = checked_malloc(sizeof(*m));
  m->tab = tab;
  m->under = under;
  return m;
}

Temp_map
Temp_empty(void)
{
  return newMap(TAB_empty(), NULL);
}

Temp_map
Temp_layerMap(Temp_map over, Temp_map under)
{
  if (over == NULL)
    return under;
  else
    return newMap(over->tab, Temp_layerMap(over->under, under));
}

void
Temp_enter(Temp_map m, Temp_temp t, string s)
{
  assert(m && m->tab);
  TAB_enter(m->tab, t, s);
}

string
Temp_look(Temp_map m, Temp_temp t)
{
  string s;
  assert(m && m->tab);
  s = TAB_look(m->tab, t);
  if (s)
    return s;
  else if (m->under)
    return Temp_look(m->under, t);
  else
    return NULL;
}

Temp_tempList
Temp_TempList(Temp_temp h, Temp_tempList t)
{
  Temp_tempList p = (Temp_tempList)checked_malloc(sizeof(*p));
  p->head = h;
  p->tail = t;
  return p;
}

Temp_labelList
Temp_LabelList(Temp_label h, Temp_labelList t)
{
  Temp_labelList p = (Temp_labelList)checked_malloc(sizeof(*p));
  p->head = h;
  p->tail = t;
  return p;
}

static FILE* outfile;
void
showit(Temp_temp t, string r)
{
  fprintf(outfile, "t%d -> %s\n", t->num, r);
}

void
Temp_dumpMap(FILE* out, Temp_map m)
{
  outfile = out;
  TAB_dump(m->tab, (void (*)(void*, void*))showit);
  if (m->under) {
    fprintf(out, "---------\n");
    Temp_dumpMap(out, m->under);
  }
}

// no need to gurantee order.
Temp_tempList
Temp_copyList(Temp_tempList tl)
{
  Temp_tempList r = NULL, last;
  for (; tl; tl = tl->tail) {
    r = Temp_TempList(tl->head, r);
  }
  return r;
}

void
Temp_print(void* t)
{
  printf("temp:%d\n", ((Temp_temp)t)->num);
}

void
Temp_printList(Temp_tempList tl)
{
  if (!tl) printf("-/-\n");
  for (; tl; tl = tl->tail) {
    Temp_print(tl->head);
  }
}

bool
inList(Temp_tempList tl, Temp_temp t)
{
  for (; tl; tl = tl->tail) {
    if (tl->head == t) return TRUE;
  }
  return FALSE;
}

int
Temp_num(Temp_temp t)
{
  return t->num;
}

// t1 - t2
Temp_tempList
except(Temp_tempList t1, Temp_tempList t2)
{
  if (!t2) return t1;

  Temp_tempList r = NULL;
  for (; t1; t1 = t1->tail) {
    if (!inList(t2, t1->head)) r = Temp_TempList(t1->head, r);
  }
  return r;
}

// t1 U t2
Temp_tempList
unionn(Temp_tempList t1, Temp_tempList t2)
{
  if (!t1) return t2;

  Temp_tempList r = Temp_copyList(t1);
  // assert(equals(r, t1));
  for (; t2; t2 = t2->tail) {
    if (!t1 || !inList(t1, t2->head)) r = Temp_TempList(t2->head, r);
  }

  // printf("union result :\n");
  // Temp_printList(r);

  return r;
}

// t1 == t2
bool
equals(Temp_tempList t1, Temp_tempList t2)
{
  Temp_tempList tmp = t1;
  for (; t2; t2 = t2->tail, t1 = t1->tail) {
    if (!inList(tmp, t2->head)) return FALSE;
    if (!t1) return FALSE; // t1 too short
  }
  if (t1) return FALSE; // t1 too long

  return TRUE;
}

void
deletee(Temp_tempList* t1, Temp_temp t)
{
  *t1 = except(*t1, Temp_TempList(t, NULL));
}

void
add(Temp_tempList* t1, Temp_temp t)
{
  *t1 = unionn(*t1, Temp_TempList(t, NULL));
}
