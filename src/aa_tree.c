#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include "aa_tree.h"

typedef int (*ftype) (void *, void *);

static jmp_buf env;

struct node {
    int level;
    void *data;
    struct node *left;
    struct node *right;
};

struct aa {
    struct node *root;
    struct node *cursor;
    ftype comp;
};

aa *aa_new(ftype comp)
{
    aa *tree;

    if (comp == NULL) {
        return NULL;
    }

    if ((tree = malloc(sizeof(aa))) == NULL) {
        return NULL;
    }

    tree->comp = comp;
    tree->root = tree->cursor = NULL;

    return tree;
}

static struct node *skew(struct node *t)
{
    struct node *l;

    if (t == NULL)
        return NULL;

    if (t->left == NULL)
        return t;

    if (t->left->level == t->level) {
        l = t->left;
        t->left = l->right;
        l->right = t;
        return l;
    } else {
        return t;
    }
}

static struct node *split(struct node *t)
{
    struct node *r;

    if (t == NULL)
        return NULL;

    if ((t->right == NULL) || (t->right->right == NULL))
        return t;

    if (t->level == t->right->right->level) {
        r = t->right;
        t->right = r->left;
        r->left = t;
        r->level++;
        return r;
    } else {
        return t;
    }
}

static struct node *insert(struct node *t, void *data, ftype dup,
                           ftype comp)
{
    if (t == NULL) {
        if ((t = malloc(sizeof(struct node))) == NULL) {
            longjmp(env, 2);
        }

        t->data = data;
        t->level = 1;
        t->left = NULL;
        t->right = NULL;
        return t;
    } else if (comp(data, t->data) == -1) {
        t->left = insert(t->left, data, dup, comp);
    } else if (comp(data, t->data) == 1) {
        t->right = insert(t->right, data, dup, comp);
    } else if (dup == NULL) {
        longjmp(env, 3);
    } else {
        if (dup(t->data, data) == 0) {
            return t;
        } else {
            longjmp(env, 4);
        }
    }

    t = skew(t);
    t = split(t);
    return t;
}

int aa_add(aa * tree, void *data, int (*dup) (void *orig, void *new))
{
    int rval;

    if (tree == NULL)
        return 1;

    if ((rval = setjmp(env)) == 0) {
        tree->root = insert(tree->root, data, dup, tree->comp);
    } else {
        return rval;
    }

    return 0;
}

static int min(int a, int b)
{
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

static struct node *decrease_level(struct node *t)
{
    int should_be;

    should_be = min(t->left->level, t->right->level) + 1;

    if (should_be < t->level) {
        t->level = should_be;
        if (should_be < t->right->level) {
            t->right->level = should_be;
        }
    }

    return t;
}

static void *predecessor(struct node *t)
{
    t = t->left;

    while (t->right != NULL) {
        t = t->right;
    }

    return t->data;
}

static void *successor(struct node *t)
{
    t = t->right;

    while (t->left != NULL) {
        t = t->left;
    }

    return t->data;
}

static struct node *delete(struct node *t, void *data, ftype dup,
                           ftype comp, void (*del) (void *))
{
    void *l;

    if (t == NULL) {
        return NULL;
    } else if (comp(data, t->data) == -1) {
        t->left = delete(t->left, data, dup, comp, del);
    } else if (comp(data, t->data) == 1) {
        t->right = delete(t->right, data, dup, comp, del);
    } else if (dup != NULL) {
        if (dup(t->data, data) == 0) {
            return t;
        } else {
            longjmp(env, 4);
        }

    } else {
        if ((t->left == NULL) && (t->right == NULL)) {
            if (del)
                del(t->data);

            free(t);
            return NULL;
        } else if (t->left == NULL) {
            l = successor(t);
            t->right = delete(t->right, l, dup, comp, del);
            if (del)
                del(t->data);

            t->data = l;
        } else {
            l = predecessor(t);
            t->left = delete(t->left, l, dup, comp, del);
            if (del)
                del(t->data);

            t->data = l;
        }
    }

    t = decrease_level(t);
    t = skew(t);
    t->right = skew(t->right);
    t->right->right = skew(t->right->right);
    t = split(t);
    t->right = split(t->right);
    return t;
}

int aa_delete(aa * tree, void *data, int (*dup) (void *orig, void *data),
              void (*del) (void *))
{
    int rval;

    if (tree == NULL)
        return 1;

    if ((rval = setjmp(env)) == 0) {
        tree->root = delete(tree->root, data, dup, tree->comp, del);
    } else {
        return rval;
    }

    return 0;
}

static void *find(struct node *t, void *data, ftype comp)
{
    if (t == NULL) {
        return NULL;
    }

    if (t->data == NULL) {
        return NULL;
    }

    int cmp_result = comp(data, t->data);
    if (cmp_result == -1) {
        return find(t->left, data, comp);
    } else if (cmp_result == 1) {
        return find(t->right, data, comp);
    } else {
        return t->data;
    }
}

void *aa_find(aa * tree, void *data)
{
    if (tree == NULL) {
        return NULL;
    }

    return find(tree->root, data, tree->comp);
}

static void preorder(struct node *node, void *(*func) (void *data))
{
    if (node == NULL)
        return;

    node->data = func(node->data);

    if (node->left)
        preorder(node->left, func);

    if (node->right)
        preorder(node->right, func);
}

static void postorder(struct node *node, void *(*func) (void *data))
{
    if (node == NULL)
        return;

    if (node->left)
        postorder(node->left, func);

    if (node->right)
        postorder(node->right, func);

    node->data = func(node->data);
}

static void inorder(struct node *node, void *(*func) (void *data))
{
    if (node == NULL)
        return;

    if (node->left)
        inorder(node->left, func);

    node->data = func(node->data);

    if (node->right)
        inorder(node->right, func);
}

static void * to_array(struct node * node, void *start, size_t size) {
    if (node == NULL) {
        return start;
    }

    start = to_array(node->left, start, size);
    memcpy(start, node->data, size);
    start += size;
    return to_array(node->right, start, size);
}

int aa_to_array(aa * tree, void *array, size_t size) {
    if (tree == NULL) {
        return 0;
    }

    return (to_array(tree->root, array, size) - array) / size;
}

int aa_traverse(aa * tree, void *(*func) (void *data), trav t)
{
    if (tree == NULL) {
        return 1;
    }

    switch (t) {
    case TRAV_IN:
        inorder(tree->root, func);
        break;
    case TRAV_POST:
        postorder(tree->root, func);
        break;
    case TRAV_PRE:
        preorder(tree->root, func);
        break;
    default:
        return 2;
        break;
    }

    return 0;
}

static void *freedata(void *data)
{
    free(data);
    return NULL;
}

void aa_freedata(aa * tree)
{
    if (tree == NULL) {
        return;
    }

    preorder(tree->root, freedata);
}

static void freetree(struct node *node)
{
    if (node == NULL)
        return;

    if (node->left)
        freetree(node->left);

    if (node->right)
        freetree(node->right);

    free(node);
}

void aa_free(aa * tree)
{
    if (tree == NULL) {
        return;
    }

    freetree(tree->root);
    free(tree);
}

void *aa_get_here(aa * tree)
{
    if (tree == NULL) {
        return NULL;
    }

    if (tree->cursor == NULL) {
        return NULL;
    }

    return tree->cursor->data;
}

int aa_set_here(aa * tree, void *data, int (*dup) (void *orig, void *data))
{
    if (tree == NULL) {
        return 1;
    }

    if (tree->cursor == NULL) {
        return 2;
    }

    if (tree->comp(data, tree->cursor->data) == 0) {
        if (dup != NULL) {
            if (dup(tree->cursor->data, data) == 0) {
                return 0;
            } else {
                return 4;
            }

        } else {
            return 3;
        }

    } else {
        tree->cursor->data = data;
        return 0;
    }
}

int aa_has_left(aa * tree)
{
    if (tree == NULL) {
        return -1;
    } else if ((tree->cursor == NULL) || (tree->cursor->left == NULL)) {
        return 0;
    } else {
        return 1;
    }
}

int aa_has_right(aa * tree)
{
    if (tree == NULL) {
        return -1;
    } else if ((tree->cursor == NULL) || (tree->cursor->right == NULL)) {
        return 0;
    } else {
        return 1;
    }
}

int aa_go_left(aa * tree)
{
    if (tree == NULL) {
        return 1;
    } else if ((tree->cursor == NULL) || (tree->cursor->left == NULL)) {
        return 1;
    } else {
        tree->cursor = tree->cursor->left;
        return 0;
    }
}

int aa_go_right(aa * tree)
{
    if (tree == NULL) {
        return 1;
    } else if ((tree->cursor == NULL) || (tree->cursor->right == NULL)) {
        return 1;
    } else {
        tree->cursor = tree->cursor->right;
        return 0;
    }
}

int aa_to_root(aa * tree)
{
    if (tree == NULL) {
        return 1;
    } else {
        tree->cursor = tree->root;
        return 0;
    }
}

static void tree_print(struct node *node, unsigned long long int n,
                       void (*printer) (FILE *, void *), FILE * fp)
{
    unsigned long long int pn = n / 2;

    fprintf(fp, "node%llu [label = \"<f0> | <f1> ", n);
    printer(fp, node->data);
    fprintf(fp, "|<f2> \"];\n");

    if (pn != 0) {
        if (n % 2 == 0) {
            fprintf(fp, "\"node%llu\":f0 -> \"node%llu\":f1;\n", pn, n);
        } else {
            fprintf(fp, "\"node%llu\":f2 -> \"node%llu\":f1;\n", pn, n);
        }
    }

    if (node->left)
        tree_print(node->left, 2 * n, printer, fp);

    if (node->right)
        tree_print(node->right, 2 * n + 1, printer, fp);
}

int aa_print(aa * tree, FILE * fp, void (*printer) (FILE *, void *))
{
    if (!tree)
        return -1;

    if (tree->root == NULL)
        return -2;

    fprintf(fp, "digraph aa_tree {\n");
    fprintf(fp, "node [shape = record];\n");
    tree_print(tree->root, 1, printer, fp);
    fprintf(fp, "}\n");

    return 0;
}
