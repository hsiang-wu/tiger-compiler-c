#ifndef UTIL_H
#define UTIL_H
#include <assert.h>
#include <stdio.h>

typedef char* string;
typedef char bool;

#define TRUE 1
#define FALSE 0

void* checked_malloc(int);
string String(char*);

typedef struct U_boolList_* U_boolList;
struct U_boolList_ {
    bool head;
    U_boolList tail;
};
U_boolList U_BoolList(bool head, U_boolList tail);

// bit matrix
typedef struct U_bmat_* U_bmat;
struct U_bmat_ {
    int size;
    int linelen;
    unsigned char* matrix;
};

U_bmat U_Bmat(int size);
bool bmlook(U_bmat bm, int a, int b);
void bmenter(U_bmat bm, int a, int b, char v);

#endif
