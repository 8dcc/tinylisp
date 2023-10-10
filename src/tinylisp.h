/**
 * @file      tinylisp.h
 * @brief     Typedefs, macros, globals and function prototypes
 * @author    8dcc
 *
 * Contains function prototypes to avoid forward-declarations and make the
 * source cleaner.
 *
 * Typedefs, macros and globals are needed for the function prototypes/other
 * headers included in tinylisp.c
 */

#ifndef TINYLISP_H_
#define TINYLISP_H_ 1

/*--------------------------------- TYPEDEFS ---------------------------------*/

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

/*---------------------------------- MACROS ----------------------------------*/

#ifdef VERBOSE_ERRORS
#define err_msg(...)                             \
    {                                            \
        fprintf(stderr, "[err] %s: ", __func__); \
        fprintf(stderr, __VA_ARGS__);            \
        putc('\n', stderr);                      \
        return err;                              \
    }
#else
#define err_msg(...) \
    { return err; }
#endif

/**
 * @def T
 * @brief Returns the *tag* bits of a NaN-boxed Lisp expression `x`
 */
#define T(x) *(uint64_t*)&x >> 48

/**
 * @def A
 * @brief Address of the atom heap is at the bottom of the cell stack
 * @details The expression `(HEAP_BOTTOM + i)` will be used through the code to
 * access the `i` offset in the heap.
 */
#define HEAP_BOTTOM ((char*)cell)

/**
 * @def N
 * @brief Number of cells for the shared stack and atom heap
 * @todo Allocate in main and overwrite with arguments
 */
#define N 1024

/*---------------------------------- GLOBALS ---------------------------------*/

/**
 * @name Heap and stack pointer
 * hp: heap pointer. Will be used as an offset in the cell[] array, by adding it
 * to the bottom of the stack: (HEAP_BOTTOM + offset)
 *
 * sp: stack pointer. Stack starts at the top of the cell[] array, and its
 * initial value is N, the size of the array: cell[N]
 */
static I hp = 0, sp = N;

/**
 * @name Tags for NaN boxing
 * Atom, primitive, cons, closure and nil
 */
static I ATOM = 0x7ff8, PRIM = 0x7ff9, CONS = 0x7ffa, CLOS = 0x7ffb,
         NIL = 0x7ffc;

/**
 * @var cell
 * @brief Array of Lisp expressions, shared by the stack and atom heap
 * @details Array of N (1024 by default) tagged floats
 */
static L cell[N];

/**
 * @name Lisp constant expressions
 * List:
 * - `nil` (empty list)
 * - `t` (explicit truth)
 * - `err` (returned to indicate errors)
 * - `env` (global enviroment)
 */
static L nil, tru, err, env;

/*--------------------------------- FUNCTIONS --------------------------------*/

/*
 * For more information:
 *   - See comments in tinylisp.c
 *   - Run "doxygen" on the project root and see file: doc/html/index.html
 */
static L box(I t, I i);
static I ord(L x);
static L num(L n);
static I equ(L x, L y);
static L atom(const char* s);
static L cons(L x, L y);
static L car(L p);
static L cdr(L p);
static L pair(L v, L x, L e);
static L closure(L v, L x, L e);
static L assoc(L v, L e);
static I not(L x);
static I let(L x);
static L evlis(L t, L e);
static L bind(L v, L t, L e);
static L reduce(L f, L t, L e);
static L apply(L f, L t, L e);
static L eval(L x, L e);
static void look();
static inline I seeing(char c);
static char get();
static char scan();
static L read();
static L list();
static L quote();
static L atomic();
static L parse();
static void print(L x);
static void printlist(L t);
static void gc();
int main();

#endif    // TINYLISP_H_
