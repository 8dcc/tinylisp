
/**
 * @file    tinylisp.c
 * @author  8dcc, Robert A. van Engelen
 * @brief   Tiny lisp REPL.
 *
 * Huge credits to Robert for his initial project and amazing documentation. See
 * README.org for more information.
 *
 * @todo Add comment support (; to eol)
 * @todo Check [in] from args
 * @todo Add (quit) command
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

/*------------------------------ MACROS/GLOBALS ------------------------------*/

#ifdef VERBOSE_ERRORS
#define err_msg(...)                    \
    {                                   \
        printf("[err] %s: ", __func__); \
        printf(__VA_ARGS__);            \
        return err;                     \
    }
#else
#define err_msg(...) \
    { return err; }
#endif

/**
 * @def I
 * @brief Type used for unsigned integers
 * @details Either 16 bit, 32 bit or 64 bit unsigned. Variables and function
 * parameters are named as follows:
 *
 *  Variable | Description
 * ----------|------------------------------------------------------
 * `i`       | Any unsigned integer, e.g. a NaN-boxed ordinal value
 * `t`       | A NaN boxing tag
 */
typedef uint32_t I;

/**
 * @def L
 * @brief Lisp expression. Will be stored in a double using NaN boxing
 * @details See bellow comments or Robert's article for more information.
 * Variables and function parameters are named as follows:
 *
 *  Variable | Description
 * ----------|--------------------------------------------------------------
 * `x`,`y`   | any Lisp expression
 * `n`       | number
 * `t`       | list
 * `f`       | function or Lisp primitive
 * `p`       | pair, a cons of two Lisp expressions
 * `e`,`d`   | environment, a list of pairs, e.g. created with (define v x)
 * `v`       | the name of a variable (an atom) or a list of variables
 */
typedef double L;

/**
 * @def T
 * @brief Returns the *tag* bits of a NaN-boxed Lisp expression `x`
 */
#define T(x) *(uint64_t*)&x >> 48

/**
 * @def A
 * @brief Address of the atom heap is at the bottom of the cell stack
 */
#define A (char*)cell

/**
 * @def N
 * @brief Number of cells for the shared stack and atom heap
 * @todo Allocate in main and overwrite with arguments
 */
#define N 1024

/* hp: heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   sp: stack pointer, the stack starts at the top of cell[] with sp=N
   safety invariant: hp <= sp<<3 */
static I hp = 0, sp = N;

/**
 * @name Tags for NaN boxing
 * Atom, primitive, cons, closure and nil
 * @{ */
static I ATOM = 0x7ff8, PRIM = 0x7ff9, CONS = 0x7ffa, CLOS = 0x7ffb,
         NIL = 0x7ffc;
/** @} */

/* cell[N] array of Lisp expressions, shared by the stack and atom heap */
static L cell[N];

/**
 * @name Lisp constant expressions
 * List:
 * - `nil` (empty list)
 * - `t` (explicit truth)
 * - `err` (returned to indicate errors)
 * - `env` (global enviroment)
 * @{ */
static L nil, tru, err, env;
/** @} */

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
 *
 * @todo stdint type
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
 * @brief Get heap index corresponding to the atom name
 * @details Checks if the atom name already exists on the heap and returns the
 * index of the corresponding boxed atom.
 *
 * If the atom name is new, then additional heap space is allocated to copy the
 * atom name into the heap as a string.
 * @param[in] s Atom name (Lisp symbols)
 * @return Corresponding NaN-boxed ATOM
 */
static L atom(const char* s) {
    I i = 0;

    /* Search for a matching atom name on the heap */
    while (i < hp && strcmp(A + i, s))
        i += strlen(A + i) + 1;

    /* If not found allocate and add a new atom name to the heap */
    if (i == hp) {
        hp += strlen(strcpy(A + i, s)) + 1;

        /* abort when out of memory */
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
        err_msg("symbol %p not found\n", (void*)(long)ord(v));
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

static L eval(L x, L e);

/* return a new list of evaluated Lisp expressions t in environment e */
static L evlis(L t, L e) {
    if (T(t) == CONS)
        return cons(eval(car(t), e), evlis(cdr(t), e));
    else if (T(t) == ATOM)
        return assoc(t, e);
    else
        return nil;
}

/*----------------------------- LISP PRIMITIVES ------------------------------*/

/**
 * @name Lisp primitives
 * This functions will be used for the Lisp primitives.
 * @{ */
/*
 *  (eval x)            return evaluated x (such as when x was quoted)
 *  (quote x)           special form, returns x unevaluated "as is"
 *  (cons x y)          construct pair (x . y)
 *  (car p)             car of pair p
 *  (cdr p)             cdr of pair p
 *  (add n1 n2 ... nk)  sum of n1 to nk
 *  (sub n1 n2 ... nk)  n1 minus sum of n2 to nk
 *  (mul n1 n2 ... nk)  product of n1 to nk
 *  (div n1 n2 ... nk)  n1 divided by the product of n2 to nk
 *  (int n)             integer part of n
 *  (< n1 n2)           #t if n1<n2, otherwise ()
 *  (equ x y)           #t if x equals y, otherwise ()
 *  (not x)             #t if x is (), otherwise ()
 *  (or x1 x2 ... xk)   first x that is not (), otherwise ()
 *  (and x1 x2 ... xk)  last x if all x are not (), otherwise ()
 *  (cond (x1 y1)
 *        (x2 y2)
 *        ...
 *        (xk yk))      the first yi for which xi evaluates to non-()
 *  (if x y z)          if x is non-() then y else z
 *  (let* (v1 x1)
 *        (v2 x2)
 *        ...
 *        y)            sequentially binds each variable v1 to xi to evaluate y
 *  (lambda v x)        construct a closure
 *  (define v x)        define a named value globally
 */

static L f_eval(L t, L e) {
    return eval(car(evlis(t, e)), e);
}

static L f_quote(L t, L _) {
    (void)_;
    return car(t);
}

static L f_cons(L t, L e) {
    t = evlis(t, e);
    return cons(car(t), car(cdr(t)));
}

static L f_car(L t, L e) {
    return car(car(evlis(t, e)));
}

static L f_cdr(L t, L e) {
    return cdr(car(evlis(t, e)));
}

static L f_add(L t, L e) {
    L n;
    t = evlis(t, e);
    n = car(t);

    while (!not(t = cdr(t)))
        n += car(t);

    return num(n);
}

static L f_sub(L t, L e) {
    L n;
    t = evlis(t, e);
    n = car(t);

    while (!not(t = cdr(t)))
        n -= car(t);

    return num(n);
}

static L f_mul(L t, L e) {
    L n;
    t = evlis(t, e);
    n = car(t);

    while (!not(t = cdr(t)))
        n *= car(t);

    return num(n);
}

static L f_div(L t, L e) {
    L n;
    t = evlis(t, e);
    n = car(t);

    while (!not(t = cdr(t)))
        n /= car(t);

    return num(n);
}

static L f_int(L t, L e) {
    L n = car(evlis(t, e));
    if (n < 1e16 && n > -1e16)
        return (int64_t)n;
    else
        return n;
}

static L f_lt(L t, L e) {
    t = evlis(t, e);
    if (car(t) - car(cdr(t)) < 0)
        return tru;
    else
        return nil;
}

static L f_eq(L t, L e) {
    t = evlis(t, e);
    if (equ(car(t), car(cdr(t))))
        return tru;
    else
        return nil;
}

static L f_not(L t, L e) {
    if (not(car(evlis(t, e))))
        return tru;
    else
        return nil;
}

static L f_or(L t, L e) {
    L x = nil;

    while (T(t) != NIL && not(x = eval(car(t), e)))
        t = cdr(t);

    return x;
}

static L f_and(L t, L e) {
    L x = nil;

    while (T(t) != NIL && !not(x = eval(car(t), e)))
        t = cdr(t);

    return x;
}

static L f_cond(L t, L e) {
    while (T(t) != NIL && not(eval(car(car(t)), e)))
        t = cdr(t);

    return eval(car(cdr(car(t))), e);
}

static L f_if(L t, L e) {
    return eval(car(cdr(not(eval(car(t), e)) ? cdr(t) : t)), e);
}

static L f_leta(L t, L e) {
    for (; let(t); t = cdr(t))
        e = pair(car(car(t)), eval(car(cdr(car(t))), e), e);

    return eval(car(t), e);
}

static L f_lambda(L t, L e) {
    return closure(car(t), car(cdr(t)), e);
}

static L f_define(L t, L e) {
    env = pair(car(t), eval(car(cdr(t)), e), env);
    return car(t);
}
/** @} */

/**
 * @struct prim
 * @brief Table of Lisp primitives
 * @details Asociates a name `s` to a function pointer `f`
 */
struct {
    const char* s; /**< @brief Primitive name */
    L (*f)(L, L);  /**< @brief Pointer to primitive function declared above */
} prim[] = { { "eval", f_eval },     { "quote", f_quote },   { "cons", f_cons },
             { "car", f_car },       { "cdr", f_cdr },       { "+", f_add },
             { "-", f_sub },         { "*", f_mul },         { "/", f_div },
             { "int", f_int },       { "<", f_lt },          { "equ", f_eq },
             { "or", f_or },         { "and", f_and },       { "not", f_not },
             { "cond", f_cond },     { "if", f_if },         { "let*", f_leta },
             { "lambda", f_lambda }, { "define", f_define }, { 0 } };

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

static void look();
static I seeing(char c);
static char get();
static char scan();
static L read();
static L list();
static L quote();
static L atomic();
static L parse();

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

static void print(L x);
static void printlist(L t);

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
 * @param argc Number of arguments
 * @param argv Vector of string arguments
 * @return Exit code
 */
int main() {
    I i;
    printf("--- TinyLisp REPL ---");

    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("t");
    env = pair(tru, tru, nil);

    for (i = 0; prim[i].s; ++i)
        env = pair(atom(prim[i].s), box(PRIM, i), env);

    while (1) {
        printf("\n[%u]> ", sp - hp / 8);
        print(eval(read(), env));
        gc();
    }
}
