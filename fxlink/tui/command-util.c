//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include "tui.h"
#include "command-util.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

//---
// Command parsing utilities
//---

struct fxlink_tui_cmd fxlink_tui_cmd_parse(char const *input)
{
    struct fxlink_tui_cmd cmd;
    cmd.argc = 0;
    cmd.argv = NULL;
    cmd.data = malloc(strlen(input) + 1);
    if(!cmd.data)
        return cmd;

    char const *escapes1 = "\\nter";
    char const *escapes2 = "\\\n\t\e\r";

    /* Whether a new word needs to be created at the next character */
    bool word_finished = true;
    /* Offset into cmd.data */
    int i = 0;

    /* Read words eagerly, appending to cmd.data as we go */
    for(int j = 0; input[j]; j++) {
        int c = input[j];

        /* Stop words at spaces */
        if(isspace(c)) {
            if(!word_finished)
                cmd.data[i++] = 0;
            word_finished = true;
            continue;
        }

        /* Translate escapes */
        if(c == '\\') {
            char *p = strchr(escapes1, input[j+1]);
            if(p) {
                c = escapes2[p - escapes1];
                j++;
            }
        }

        /* Add a new word if necessary */
        if(word_finished) {
            cmd.argv = realloc(cmd.argv, (++cmd.argc) * sizeof *cmd.argv);
            cmd.argv[cmd.argc - 1] = cmd.data + i;
            word_finished = false;
        }

        /* Copy literals */
        cmd.data[i++] = c;
    }

    cmd.data[i++] = 0;
    cmd.argv = realloc(cmd.argv, (cmd.argc + 1) * sizeof *cmd.argv);
    cmd.argv[cmd.argc] = 0;
    return cmd;
}

void fxlink_tui_cmd_dump(struct fxlink_tui_cmd const *cmd)
{
    print(TUI.wConsole, "[%d]", cmd->argc);
    for(int i = 0; i < cmd->argc; i++) {
        char const *arg = cmd->argv[i];
        print(TUI.wConsole, " '%s'(%d)", arg, (int)strlen(arg));
    }
    print(TUI.wConsole, "\n");
}

void fxlink_tui_cmd_free(struct fxlink_tui_cmd const *cmd)
{
    free(cmd->argv);
    free(cmd->data);
}

static struct fxlink_device *find_connected_device(void)
{
    /* TODO: Use the "selected" device */
    for(int i = 0; i < TUI.devices.count; i++) {
        if(TUI.devices.devices[i].status == FXLINK_FDEV_STATUS_CONNECTED)
            return &TUI.devices.devices[i];
    }
    return NULL;
}

bool fxlink_tui_parse_args(int argc, char const **argv, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int i = 0;
    char const *spec = fmt;
    bool got_variadic = false;

    for(; *spec; i++, spec++) {
        /* Implicit/silent specifiers */
        if(*spec == 'd') {
            struct fxlink_device **ptr = va_arg(args, struct fxlink_device **);
            *ptr = find_connected_device();
            if(!*ptr) {
                fprint(TUI.wConsole, FMT_RED, "error: ");
                print(TUI.wConsole, "no device connected\n");
                goto failure;
            }
            /* Bad */
            i--;
            continue;
        }

        /* No specifier that consumes stuff allowed after '*' */
        if(got_variadic) {
            fprint(TUI.wConsole, FMT_RED, "error: ");
            print(TUI.wConsole, "got specifiers '%s' after '*'\n", spec);
            goto failure;
        }

        /* Specifiers allowed even when there is no argument left */
        if(*spec == '*') {
            char const ***ptr = va_arg(args, char const ***);
            *ptr = argv + i;
            got_variadic = true;
            continue;
        }

        /* Argument required beyond this point */
        if(i >= argc && *spec != '*') {
            fprint(TUI.wConsole, FMT_RED, "error: ");
            print(TUI.wConsole, "too few arguments\n");
            goto failure;
        }

        /* Standard specifiers */
        if(*spec == 's') {
            char const **ptr = va_arg(args, char const **);
            *ptr = argv[i];
        }
        else if(*spec == 'i') {
            int *ptr = va_arg(args, int *);
            char *endptr;
            long l = strtol(argv[i], &endptr, 0);
            if(*endptr) {
                fprint(TUI.wConsole, FMT_RED, "error: ");
                print(TUI.wConsole, "not a valid integer: '%s'\n", argv[i]);
                goto failure;
            }
            *ptr = l;
        }
    }

    va_end(args);
    return true;

failure:
    va_end(args);
    return false;
}

//---
// Command tree
//---

struct node {
    /* Command or subtree name */
    char *name;
    /* true if tree node, false if raw command */
    bool is_tree;

    union {
        struct node *children; /* is_subtree = true */
        int (*func)(int argc, char const **argv); /* is_subtree = false */
    };

    /* Next sibling */
    struct node *next;
};

static struct node *node_mkcmd(char const *name, int (*func)())
{
    assert(name);
    struct node *cmd = calloc(1, sizeof *cmd);
    cmd->name = strdup(name);
    cmd->func = func;
    return cmd;
}

static struct node *node_mktree(char const *name)
{
    assert(name);
    struct node *tree = calloc(1, sizeof *tree);
    tree->is_tree = true;
    tree->name = strdup(name);
    return tree;
}

static void node_free(struct node *node);

static void node_free_chain(struct node *node)
{
    struct node *next;
    while(node) {
        next = node->next;
        node_free(node);
        node = next;
    }
}

static void node_free(struct node *node)
{
    free(node->name);
    if(node->is_tree) {
        node_free_chain(node->children);
        free(node);
    }
}

static void node_tree_add(struct node *tree, struct node *node)
{
    assert(tree->is_tree);
    node->next = tree->children;
    tree->children = node;
}

static struct node *node_tree_get(struct node const *tree, char const *name)
{
    assert(tree->is_tree);
    for(struct node *n = tree->children; n; n = n->next) {
        if(!strcmp(n->name, name))
            return n;
    }
    return NULL;
}

static struct node *node_tree_get_or_make_subtree(struct node *tree,
    char const *name)
{
    assert(tree->is_tree);
    struct node *n = node_tree_get(tree, name);
    if(n)
        return n;
    n = node_mktree(name);
    node_tree_add(tree, n);
    return n;
}

static struct node *node_tree_get_path(struct node *tree, char const **path,
    int *path_end_index)
{
    assert(tree->is_tree);
    struct node *n = node_tree_get(tree, path[0]);
    if(!n)
        return NULL;

    (*path_end_index)++;
    if(!n->is_tree)
        return n;

    if(!path[1]) {
        fprint(TUI.wConsole, FMT_RED, "error: ");
        print(TUI.wConsole, "'%s' takes a sub-command argument\n", path[0]);
        return NULL;
    }
    return node_tree_get_path(n, path+1, path_end_index);
}

static void node_insert_command(struct node *tree, char const **path,
    int (*func)(), int i)
{
    assert(tree->is_tree);

    if(!path[i]) {
        fprintf(stderr, "error: cannot register empty command!\n");
        return;
    }
    else if(!path[i+1]) {
        struct node *cmd = node_tree_get(tree, path[i]);
        if(cmd) {
            fprintf(stderr, "error: '%s' already registred!\n", path[i]);
            return;
        }
        node_tree_add(tree, node_mkcmd(path[i], func));
    }
    else {
        struct node *subtree = node_tree_get_or_make_subtree(tree, path[i]);
        if(!subtree->is_tree) {
            fprintf(stderr, "error: '%s' is not a category!\n", path[i]);
            return;
        }
        return node_insert_command(subtree, path, func, i+1);
    }
}

static void node_dump(struct node const *node, int indent)
{
    print(TUI.wConsole, "%*s", 2*indent, "");

    if(node->is_tree) {
        print(TUI.wConsole, "%s\n", node->name);
        struct node *child = node->children;
        while(child) {
            node_dump(child, indent+1);
            child = child->next;
        }
    }
    else {
        print(TUI.wConsole, "%s: %p\n", node->name, node->func);
    }
}

static struct node *cmdtree = NULL;

void fxlink_tui_register_cmd(char const *name,
    int (*func)(int argc, char const **argv))
{
    int i = 0;
    while(name[i] && (isalpha(name[i]) || strchr("?/-_ ", name[i])))
        i++;
    if(name[i] != 0) {
        fprintf(stderr, "error: invalid command path '%s'\n", name);
        return;
    }

    if(!cmdtree)
        cmdtree = node_mktree("(root)");

    /* Parse as a command because why not */
    struct fxlink_tui_cmd path = fxlink_tui_cmd_parse(name);
    node_insert_command(cmdtree, path.argv, func, 0);

    fxlink_tui_cmd_free(&path);
}

__attribute__((destructor))
static void free_command_tree(void)
{
    node_free(cmdtree);
    cmdtree = NULL;
}

void TUI_execute_command(char const *command)
{
    struct fxlink_tui_cmd cmd = fxlink_tui_cmd_parse(command);
    if(cmd.argc < 1)
        goto end;

    int args_index = 0;
    struct node *node = node_tree_get_path(cmdtree, cmd.argv, &args_index);

    if(node) {
        node->func(cmd.argc - args_index, cmd.argv + args_index);
        /* ignore return code? */
    }
    else {
        fprint(TUI.wConsole, FMT_RED, "error: ");
        print(TUI.wConsole, "unrecognized command: ");
        fxlink_tui_cmd_dump(&cmd);
    }

end:
    fxlink_tui_cmd_free(&cmd);
}

FXLINK_COMMAND("?cmdtree")
{
    node_dump(cmdtree, 0);
    return 0;
}
