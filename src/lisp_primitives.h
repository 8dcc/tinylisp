/**
 * @file      lisp_primitives.h
 * @brief     Lisp primitives and table of primitives
 * @author    8dcc, Robert A. van Engelen
 *
 * For descriptions of the primitives, see comment on source.
 */

#ifndef LISP_PRIMITIVES_H_
#define LISP_PRIMITIVES_H_ 1

/*
 * Lisp primitives
 * This functions will be used for the Lisp primitives.
 *
 * Descriptions:
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
 *  (quit)              exit the REPL
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

static L f_quit(L t, L e) {
    (void)t;
    (void)e;

    puts("Goodbye!");
    exit(0);
}

/**
 * @struct PrimPair
 * @details Asociates a name `s` to a function pointer `f`
 */
typedef struct {
    const char* s; /* Primitive name */
    L (*f)(L, L);  /* Pointer to primitive function declared above */
} PrimPair;

/* clang-format off */

/**
 * @var prim
 * @brief Table of Lisp primitives
 */
PrimPair prim[] = {
    { "eval",   f_eval },
    { "quote",  f_quote },
    { "cons",   f_cons },
    { "car",    f_car },
    { "cdr",    f_cdr },
    { "+",      f_add },
    { "-",      f_sub },
    { "*",      f_mul },
    { "/",      f_div },
    { "int",    f_int },
    { "<",      f_lt },
    { "equ",    f_eq },
    { "or",     f_or },
    { "and",    f_and },
    { "not",    f_not },
    { "cond",   f_cond },
    { "if",     f_if },
    { "let*",   f_leta },
    { "lambda", f_lambda },
    { "define", f_define },
    { "quit",   f_quit },
    { 0 },
};

/* clang-format on */

#endif /* LISP_PRIMITIVES_H_ */
