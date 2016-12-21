/*
 * util.c - commonly used utility functions.
 */

#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void*
checked_malloc(int len)
{
  void* p = malloc(len);
  if (!p) {
    fprintf(stderr, "\nRan out of memory!\n");
    exit(1);
  }
  return p;
}

string
String(char* s)
{
  string p = checked_malloc(strlen(s) + 1);
  strcpy(p, s);
  return p;
}

U_boolList
U_BoolList(bool head, U_boolList tail)
{
  U_boolList list = checked_malloc(sizeof(*list));
  list->head = head;
  list->tail = tail;
  return list;
}

#define BLEN (sizeof(unsigned char) * 8) // byte length in bit
U_bmat
U_Bmat(int size)
{
  U_bmat bm = checked_malloc(sizeof(*bm));
  bm->size = size;               // row & column size
  bm->linelen = size / BLEN + 1; // row len in byte
  bm->matrix = checked_malloc(size * bm->linelen);

  bzero(bm->matrix, size * bm->linelen);
  return bm;
}

bool
bmlook(U_bmat bm, int a, int b)
{
  assert(a < bm->size);
  assert(b < bm->size);
  unsigned char* linehead = bm->matrix + (a * bm->linelen);

  int offset = b / BLEN;
  int shift = b % BLEN;
  unsigned char* bbyte = (linehead + offset); // ab's byte
  unsigned char mask = 1 << shift;
  return !!(*bbyte & mask); // return 1 on true, 0 on false
}

void
bmenter(U_bmat bm, int a, int b, char v)
{
  assert(a < bm->size);
  assert(b < bm->size);

  unsigned char* linehead = bm->matrix + (a * bm->linelen);

  int offset = b / BLEN;
  int shift = b % BLEN;
  unsigned char* bbyte = (linehead + offset); // ab's byte
  unsigned char mask1 = (!!v) << shift;
  unsigned char mask2 = ~((!v) << shift);

  // x x x x x x x x *byte
  // 0 0 0 0 v 0 0 0 mask1
  // 1 1 1 1 v 1 1 1 mask2
  *bbyte = ((*bbyte | mask1) & mask2);
}
