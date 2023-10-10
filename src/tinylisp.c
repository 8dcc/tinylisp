/**
 * @file      tinylisp.c
 * @brief     Tiny lisp REPL.
 * @author    8dcc, Robert A. van Engelen
 *
 * Huge credits to Robert for his initial project and amazing documentation. See
 * README.org for more information.
 *
 * @todo Add comment support (; to eol)
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @def VERBOSE_ERRORS
 * @brief If defined, the program will print a brief description of the errors.
 */
#define VERBOSE_ERRORS

#include "tinylisp.h" /* Typedefs, macros, globals, function prototypes */
#include "lisp_primitives.h" /* Lisp primitives, table of primitives */

/*-------------------------------- NaN BOXING --------------------------------*/

/**
 * @brief Tag and NaN-box ordinal content `i` with the specified tag `t`
 * @details Tag `t` is supposed to be ATOM, PRIM, CONS, CLOS or NIL. For more
 * information about NaN boxing, see:
 *   https://softwareengineering.stackexchange.com/questions/185406/what-is-the-purpose-of-nan-boxing#185431
 * @param[in] t Tag
 * @param[in] i Content
 * @return Tagged NaN-boxed double
 */
static L box(I t, I i) {
    L x;
    *(uint64_t*)&x = (uint64_t)t << 48 | i;
    return x;
}

/**
 * @brief Unbox the unsigned integer (ordinal) of a tagged float
 * @details Tagged floats are created with box(). The return value is narrowed
 * to 32 bit unsigned integer to remove the tag.
 *
 * Keep in mind we are casting from a double float (L) to a 64 bit unsigned and
 * then *returning a 32 bit unsigned* (I).
 * @param[in] x NaN-boxed double (tagged ordinal)
 * @return Unboxed 32 bit ordinal value from NaN box
 */
static I ord(L x) {
    return *(uint64_t*)&x;
}

/**
 * @brief Returns literal NaN box
 * @details Does nothing, could check for NaN
 * @param[in] n NaN-boxed double
 * @return Same as argument
 */
static L num(L n) {
    return n;
}

/**
 * @brief Compares 2 NaN-boxed values to see if they match
 * @details Because equality comparisons `==` with NaN values always produce
 * false, we just need to compare the 64 bits of the values for equality
 * @param[in] x NaN-boxed expression 1
 * @param[in] y NaN-boxed expression 2
 * @return Non-zero if they are equal, zero otherwise
 */
static I equ(L x, L y) {
    return *(uint64_t*)&x == *(uint64_t*)&y;
}

/*-------------------------------- TODO --------------------------------*/

/**
 * @brief Get heap index corresponding to the atom name, or allocate new
 * atom-tagged float
 * @details Checks if the atom name already exists on the heap (by iterating
 * until the heap pointer) and returns the index of the corresponding boxed
 * atom.
 *
 * If the atom name is new, then additional heap space is allocated to copy the
 * atom name into the heap as a string.
 * @param[in] s Atom name (Lisp symbols)
 * @return Corresponding NaN-boxed ATOM
 */
static L atom(const char* s) {
    I i = 0;

    /* Search for a matching atom name on the heap.
     * Compares the string parameter with each element in the heap. We check
     * until hp (heap pointer) for already existing atoms. If we reach hp, we
     * allocate a new one. */
    while (i < hp && strcmp(A + i, s))
        i += strlen(A + i) + 1;

    /* If not found allocate and add a new atom name to the heap. Otherwise, if
     * (i != hp), it means we found the atom, so we can skip this part and
     * return it */
    if (i == hp) {
        /* Increase the heap pointer by the length of the new string + NULL */
        hp += strlen(strcpy(A + i, s)) + 1;

        /* Abort when out of memory. (n << 3) => (n * 8) */
        if (hp > sp << 3)
            abort();
    }

    return box(ATOM, i);
}

/* construct pair (x . y) returns a NaN-boxed CONS */
static L cons(L x, L y) {
    cell[--sp] = x;   /* push the car value x */
    cell[--sp] = y;   /* push the cdr value y */
    if (hp > sp << 3) /* abort when out of memory */
        abort();
    return box(CONS, sp);
}

/* return the car of a pair or ERR if not a pair */
static L car(L p) {
    if ((T(p) & ~(CONS ^ CLOS)) == CONS)
        return cell[ord(p) + 1];
    else
        err_msg("not a pair\n");
}

/* return the cdr of a pair or ERR if not a pair */
static L cdr(L p) {
    if ((T(p) & ~(CONS ^ CLOS)) == CONS)
        return cell[ord(p)];
    else
        err_msg("not a pair\n");
}

/* construct a pair to add to environment e, returns the list ((v . x) . e) */
static L pair(L v, L x, L e) {
    return cons(cons(v, x), e);
}

/* construct a closure, returns a NaN-boxed CLOS */
static L closure(L v, L x, L e) {
    return box(CLOS, ord(pair(v, x, equ(e, env) ? nil : e)));
}

/* look up a symbol in an environment, return its value or ERR if not found */
static L assoc(L v, L e) {
    while (T(e) == CONS && !equ(v, car(car(e))))
        e = cdr(e);

    if (T(e) == CONS)
        return cdr(car(e));
    else
        err_msg("symbol %s not found\n", A + ord(v));
}

/**
 * @brief Check if the argument is an empty list (`nil`)
 * @details Keep in mind that empty lists in Lisp are considered *false* and any
 * other values are considered *true*
 * @param[in] x Expression to check
 * @return Non-zero if NILL, zero if non-nil
 */
static I not(L x) {
    return T(x) == NIL;
}

/* let(x) is nonzero if x is a Lisp let/let* pair */
static I let(L x) {
    return T(x) != NIL && !not(cdr(x));
}

/* return a new list of evaluated Lisp expressions t in environment e */
static L evlis(L t, L e) {
    if (T(t) == CONS)
        return cons(eval(car(t), e), evlis(cdr(t), e));
    else if (T(t) == ATOM)
        return assoc(t, e);
    else
        return nil;
}

/*------------------------------- ENVIROMENTS --------------------------------*/

/**
 * @brief Create environment by extending `e` with variables `v` bound to values
 * `t`
 * @details Description
 * @param[in] v Variables for the enviroment
 * @param[in] t List of values for the variables
 * @param[in] e Enviroment
 * @return Enviroment with `t` binded to `v`
 */
static L bind(L v, L t, L e) {
    if (T(v) == NIL)
        return e;
    else if (T(v) == CONS)
        return bind(cdr(v), cdr(t), pair(car(v), car(t), e));
    else
        return pair(v, t, e);
}

/**
 * @brief Apply closure `f` to arguments `t` in environemt `e`
 * @details Called by apply() if the expression is a primitive
 * @param[in] f Closure to apply
 * @param[in] t List of arguments for closure `f`
 * @param[in] e Enviroment
 * @return Applied closure
 */
static L reduce(L f, L t, L e) {
    return eval(cdr(car(f)),
                bind(car(car(f)), evlis(t, e), not(cdr(f)) ? env : cdr(f)));
}

/**
 * @brief Apply closure or primitive `f` to arguments `t` in environment `e`
 * @details Will call reduce() if closure
 * @param[in] f Clousure or primitive
 * @param[in] t List of arguments
 * @param[in] e Enviroment
 * @return Applied expression if closure or primitive, `err` otherwise
 */
static L apply(L f, L t, L e) {
    if (T(f) == PRIM)
        return prim[ord(f)].f(t, e);
    else if (T(f) == CLOS)
        return reduce(f, t, e);
    else
        err_msg("not a valid clousure or primitive\n");
}

/**
 * @brief Evaluate `x` and return its value in environment `e`
 * @param[in] x Expression to evaluate
 * @param[in] e Enviroment of the expression
 * @return Evaluated expression
 */
static L eval(L x, L e) {
    if (T(x) == ATOM)
        return assoc(x, e);
    else if (T(x) == CONS)
        return apply(eval(car(x), e), cdr(x), e);
    else
        return x;
}

/*--------------------------------- PARSING ----------------------------------*/

/**
 * @var buf
 * @brief Tokenization buffer
 */
static char buf[40];

/**
 * @var see
 * @brief The next character that we are looking at (last read by look())
 */
static char see = ' ';

/**
 * @brief Store the current character in `see` and advances to the next
 * character
 * @details Checks for EOF
 */
static void look() {
    int c = getchar();
    see   = c;
    if (c == EOF)
        exit(0);
}

/**
 * @brief Check if we are looking at character c
 * @details Character `' '` will be passed as parameter in case of *any*
 * whitespace
 * @param[in] c Character to check
 * @return Non-zero if we are seeing that character
 */
static inline I seeing(char c) {
    if (c == ' ')
        return see > 0 && see <= c;
    else
        return see == c;
}

/**
 * @brief Return the look ahead character from standard input, advances to the
 * next
 * @return Current look ahead character
 */
static char get() {
    char c = see;
    look();
    return c;
}

/**
 * @brief Tokenize input into buf[]
 * @return First character of buf[]
 */
static char scan() {
    I i = 0;

    while (seeing(' '))
        look();

    if (seeing('(') || seeing(')') || seeing('\'')) {
        buf[i++] = get();
    } else {
        do {
            buf[i++] = get();
        } while (i < 39 && !seeing('(') && !seeing(')') && !seeing(' '));
    }

    buf[i] = 0;
    return *buf;
}

/**
 * @brief Scan and parse an expression from standard input
 * @details Calls scan() and parse()
 * @return The Lisp expression read from standard input
 */
static L read() {
    scan();
    return parse();
}

/**
 * @brief Parse a Lisp list
 * @details Uses input buffer `buf`
 * @return Parsed Lisp list
 */
static L list() {
    L x;
    if (scan() == ')')
        return nil;

    if (!strcmp(buf, ".")) {
        x = read();
        scan();
        return x;
    }

    x = parse();
    return cons(x, list());
}

/**
 * @brief Parse a Lisp expression quoted as `(quote x)`
 * @details Uses input buffer `buf`
 * @return Parsed quoted expression
 */
static L quote() {
    return cons(atom("quote"), cons(read(), nil));
}

/**
 * @brief Parse ab atomic Lisp expression
 * @details Uses input buffer `buf`. An atomic expression is a number or an atom
 * @return Parsed atomic Lisp expression
 */
static L atomic() {
    L n;
    I i;

    if (sscanf(buf, "%lg%n", &n, &i) > 0 && !buf[i])
        return n;
    else
        return atom(buf);
}

/**
 * @brief Parse a Lisp expression
 * @details Uses input buffer `buf`
 * @return Parsed Lisp expression
 */
static L parse() {
    if (*buf == '(')
        return list();
    else if (*buf == '\'')
        return quote();
    else
        return atomic();
}

/*--------------------------------- PRINTING ---------------------------------*/

/**
 * @brief Display a Lisp expression
 * @param[in] x Expression to print
 */
static void print(L x) {
    /* NOTE: We don't use switch here because type tags are not constant at
     * compile time */
    if (T(x) == NIL)
        printf("()");
    else if (T(x) == ATOM)
        printf("%s", A + ord(x));
    else if (T(x) == PRIM)
        printf("<%s>", prim[ord(x)].s);
    else if (T(x) == CONS)
        printlist(x);
    else if (T(x) == CLOS)
        printf("{%u}", ord(x));
    else
        printf("%.10lg", x);
}

/**
 * @brief Display a Lisp list
 * @param[in] t List to be printed
 */
static void printlist(L t) {
    for (putchar('(');; putchar(' ')) {
        print(car(t));
        t = cdr(t);

        if (T(t) == NIL)
            break;

        if (T(t) != CONS) {
            printf(" . ");
            print(t);
            break;
        }
    }
    putchar(')');
}

/*--------------------------------- GARBAGE ----------------------------------*/

/**
 * @brief Garbage collection
 * @details Removes temporary cells, keeps global environment
 */
static void gc() {
    sp = ord(env);
}

/*----------------------------------- MAIN -----------------------------------*/

/**
 * @brief Entry point of the REPL
 * @details We initialize the predefined atoms (`nil`, `err` and `tru`) and the
 * enviroment (`env`). We add the primitives to the enviroment and start the
 * main loop.
 * @return Exit code
 */
int main(void) {
    printf("--- TinyLisp REPL ---");

    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("t");
    env = pair(tru, tru, nil);

    for (I i = 0; prim[i].s; i++)
        env = pair(atom(prim[i].s), box(PRIM, i), env);

    while (1) {
        printf("\n[%u]> ", sp - hp / 8);
        print(eval(read(), env));
        gc();
    }
}
