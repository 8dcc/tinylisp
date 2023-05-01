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

/*---------------------------------- GLOBALS ---------------------------------*/

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

/*--------------------------------- FUNCTIONS --------------------------------*/

/**
 * @brief Tag and NaN-box ordinal content `i` with the specified tag `t`
 * @details Tag `t` is supposed to be ATOM, PRIM, CONS, CLOS or NIL. For more
 * information about NaN boxing, see:
 *   https://softwareengineering.stackexchange.com/questions/185406/what-is-the-purpose-of-nan-boxing#185431
 * @param[in] t Tag
 * @param[in] i Content
 * @return Tagged NaN-boxed double
 */
static L box(I t, I i);

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
static I ord(L x);

/**
 * @brief Returns literal NaN box
 * @details Does nothing, could check for NaN
 * @param[in] n NaN-boxed double
 * @return Same as argument
 */
static L num(L n);

/**
 * @brief Compares 2 NaN-boxed values to see if they match
 * @details Because equality comparisons `==` with NaN values always produce
 * false, we just need to compare the 64 bits of the values for equality
 * @param[in] x NaN-boxed expression 1
 * @param[in] y NaN-boxed expression 2
 * @return Non-zero if they are equal, zero otherwise
 */
static I equ(L x, L y);

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
static L atom(const char* s);

/* construct pair (x . y) returns a NaN-boxed CONS */
static L cons(L x, L y);

/* return the car of a pair or ERR if not a pair */
static L car(L p);

/* return the cdr of a pair or ERR if not a pair */
static L cdr(L p);

/* construct a pair to add to environment e, returns the list ((v . x) . e) */
static L pair(L v, L x, L e);

/* construct a closure, returns a NaN-boxed CLOS */
static L closure(L v, L x, L e);

/* look up a symbol in an environment, return its value or ERR if not found */
/** @todo Use atom name in err_msg() */
static L assoc(L v, L e);

/**
 * @brief Check if the argument is an empty list (`nil`)
 * @details Keep in mind that empty lists in Lisp are considered *false* and any
 * other values are considered *true*
 * @param[in] x Expression to check
 * @return Non-zero if NILL, zero if non-nil
 */
static I not(L x);

/* let(x) is nonzero if x is a Lisp let/let* pair */
static I let(L x);

/* return a new list of evaluated Lisp expressions t in environment e */
static L evlis(L t, L e);

/**
 * @brief Create environment by extending `e` with variables `v` bound to values
 * `t`
 * @details Description
 * @param[in] v Variables for the enviroment
 * @param[in] t List of values for the variables
 * @param[in] e Enviroment
 * @return Enviroment with `t` binded to `v`
 */
static L bind(L v, L t, L e);

/**
 * @brief Apply closure `f` to arguments `t` in environemt `e`
 * @details Called by apply() if the expression is a primitive
 * @param[in] f Closure to apply
 * @param[in] t List of arguments for closure `f`
 * @param[in] e Enviroment
 * @return Applied closure
 */
static L reduce(L f, L t, L e);

/**
 * @brief Apply closure or primitive `f` to arguments `t` in environment `e`
 * @details Will call reduce() if closure
 * @param[in] f Clousure or primitive
 * @param[in] t List of arguments
 * @param[in] e Enviroment
 * @return Applied expression if closure or primitive, `err` otherwise
 */
static L apply(L f, L t, L e);

/**
 * @brief Evaluate `x` and return its value in environment `e`
 * @param[in] x Expression to evaluate
 * @param[in] e Enviroment of the expression
 * @return Evaluated expression
 */
static L eval(L x, L e);

/**
 * @var buf
 * @brief Tokenization buffer
 */
static char buf[40];

/**
 * @brief Store the current character in `see` and advances to the next
 * character
 * @details Checks for EOF
 */
static void look();

/**
 * @brief Check if we are looking at character c
 * @details Character `' '` will be passed as parameter in case of *any*
 * whitespace
 * @param[in] c Character to check
 * @return Non-zero if we are seeing that character
 */
static inline I seeing(char c);

/**
 * @brief Return the look ahead character from standard input, advances to the
 * next
 * @return Current look ahead character
 */
static char get();

/**
 * @brief Tokenize input into buf[]
 * @return First character of buf[]
 */
static char scan();

/**
 * @brief Scan and parse an expression from standard input
 * @details Calls scan() and parse()
 * @return The Lisp expression read from standard input
 */
static L read();

/**
 * @brief Parse a Lisp list
 * @details Uses input buffer `buf`
 * @return Parsed Lisp list
 */
static L list();

/**
 * @brief Parse a Lisp expression quoted as `(quote x)`
 * @details Uses input buffer `buf`
 * @return Parsed quoted expression
 */
static L quote();

/**
 * @brief Parse ab atomic Lisp expression
 * @details Uses input buffer `buf`. An atomic expression is a number or an atom
 * @return Parsed atomic Lisp expression
 */
static L atomic();

/**
 * @brief Parse a Lisp expression
 * @details Uses input buffer `buf`
 * @return Parsed Lisp expression
 */
static L parse();

/**
 * @brief Display a Lisp expression
 * @param[in] x Expression to print
 */
static void print(L x);

/**
 * @brief Display a Lisp list
 * @param[in] t List to be printed
 */
static void printlist(L t);

/**
 * @brief Garbage collection
 * @details Removes temporary cells, keeps global environment
 */
static void gc();

/**
 * @brief Entry point of the REPL
 * @details We initialize the predefined atoms (`nil`, `err` and `tru`) and the
 * enviroment (`env`). We add the primitives to the enviroment and start the
 * main loop.
 * @param argc Number of arguments
 * @param argv Vector of string arguments
 * @return Exit code
 */
int main();

#endif    // TINYLISP_H_
