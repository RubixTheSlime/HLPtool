#ifndef AA_TREE_H
#define AA_TREE_H

#include <stdio.h>

typedef enum trav { TRAV_IN, TRAV_POST, TRAV_PRE } trav;
typedef struct aa aa;

/* NULL on error, comp must be a comparator that returns:
 *  -1 when l < r
 *   0 when l == r
 *   1 when l > r */
aa *aa_new(int (*comp) (void *l, void *r));
/* 0 on no error, 1 on no tree, 2 on memory error, 3 on duplicate if dup not set, 4 on dup error
 * dup returns 0 on no error, 1 on error */
int aa_add(aa * tree, void *data, int (*dup) (void *orig, void *new));
/* 0 on no error, 1 on no tree, 2 on dup error
 * dup returns 0 on no error, 1 on error 
 * if del exists, it is applied to the data */
int aa_delete(aa * tree, void *data, int (*dup) (void *orig, void *data),
              void (*del) (void *));
/* NULL on error or not found */
void *aa_find(aa * tree, void *data);
/* returns length */
int aa_to_array(aa * tree, void *array, size_t size);
/* 0 on no error, 1 on no tree, 2 on bad trav (not one of TRAV_IN, TRAV_POST, TRAV_PRE) */
int aa_traverse(aa * tree, void *(*func) (void *data), trav t);
/* NULL on error */
void *aa_get_here(aa * tree);
/* 0 on no error, 1 on no tree, 2 on empty tree, dup stuff as above; replaces if no dup */
int aa_set_here(aa * tree, void *data,
                int (*dup) (void *orig, void *data));
/* 1 if true, 0 if false, -1 on error */
int aa_has_left(aa * tree);
/* 1 if true, 0 if false, -1 on error */
int aa_has_right(aa * tree);
/* 0 on no error, 1 on error */
int aa_go_right(aa * tree);
/* 0 on no error, 1 on error */
int aa_go_left(aa * tree);
/* 0 on no error, 1 on error */
int aa_to_root(aa * tree);
/* no-op on error */
void aa_freedata(aa * tree);
/* no-op on error */
void aa_free(aa * tree);
/* 0 on no error, -1 on no tree, -2 on empty tree */
int aa_print(aa * tree, FILE * fp, void (*printer) (FILE *, void *));

#endif                          /* AA_TREE_H */
