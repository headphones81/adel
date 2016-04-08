
#include <stdint.h>

#ifndef ADEL_V2
#define ADEL_V2


/** Adel state
 *
 * This class holds all the runtime information for a single function
 * invocation. The "line" is the current location in the function; the
 * "wait" is the time we're waiting for (in a delay); the "i" is 
 * a loop variable for use in afor. */
class Astate
{
public:
  uint16_t line;
  uint16_t wait;
  uint16_5 i;
  Astate() : line(0), wait(0), i(0) {}
};

/** Adel stack
 *  Stack of function states. Since Adel is emulating concurrency, the
 *  stack is not a linear data structure, but a tree of currently running
 *  functions. This implementation limits the tree to a binary tree, so
 *  that we can use a heap representation. This restriction means that we
 *  must use a fork-join model of parallelism. */
extern Astate adel_stack[1 << MAX_DEPTH];

/** Current function */
extern uint16_t adel_current;

/** Child function index
 * Using the heap structure, this is really fast. */
#define achild(c) ((a_my_index << 1) + c)

/** Start a new function */
#define ainit_child(c) adel_stack[achild(c)].line = 0

/** Call a function */


#define ADEL_FINALLY 99999

/** adel status
 * 
 *  All Adel functions return an enum that indicates whether the routine is
 *  done or has more work to do.
 */
class Adel
{
public:
  typedef enum { NONE, DONE, CONT } _status;
  
private:
  _status m_status;
  
public:
  Adel(_status s) : m_status(s) {}
  Adel() : m_status(NONE) {}
  Adel(const Adel& other) : m_status(other.m_status) {}
  explicit Adel(bool b) : m_status(NONE) {}
  
  bool done() const { return m_status == DONE; }
  bool cont() const { return m_status == CONT; }
};
  
/** astart
 *
 *  Use astart in the Arduino loop function to initiate the Adel function f
 *  (and run all Adel functions below it).
 */
#define astart( f ) \
  adel_current = 0; \
  f;

/** abegin
 *
 * Always add abegin and aend to every adel function
 */
#define abegin						\
  Adel f_status, g_status;				\
  int a_my_index = adel_current;			\
  Astate& a_me = adel_stack[a_my_index];		\
  switch (a_me.line) {					\
  case 0:

#define aend						\
  case ADEL_FINALLY: ;					\
  }							\
  a_me.line = ADEL_FINALLY;				\
  return Adel::DONE;

/** afinally
 *
 *  Optionally, end with afinally to do some action whenever the function
 *  returns (for any reason)
 */
#define afinally( f )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
  case ADEL_FINALLY:					\
    adel_current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return Adel::CONT;

/** adelay
 *
 *  Semantics: delay this function for t milliseconds
 */
#define adelay(t)					\
    a_me.line = __LINE__;				\
    a_me.wait = millis() + t;				\
  case __LINE__:					\
  if (millis() < a_me.wait) return Adel::CONT;

/** andthen
 *
 *  Semantics: execute f synchronously, until it is done (returns false)
 *  Example use:
 *     andthen( turn_on_light() );
 *     andthen( turn_off_light() );
 */
#define andthen( f )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
 case __LINE__:						\
    adel_current = achild(1);				\
    f_status = f;					\
    if ( f_status.cont() ) return Adel::CONT;

/** await
 *  Wait asynchronously for a condition to become true.
 */
#define await( c )					\
    a_me.line = __LINE__;				\
 case __LINE__:						\
    if ( ! ( c ) ) return Adel::CONT

/** adountil
 *
 *  Semantics: do f until it completes, or until the timeout
 */
#define adountil( t, f )				\
    a_me.line = __LINE__;				\
    a_me.wait = millis() + t;				\
  case __LINE__:					\
    f_status = f;					\
    if (f_status.cont() && millis() < a_me.wait) return Adel::CONT;

/** aboth
 *
 *  Semantics: execute f and g asynchronously, until *both* are done
 *  (both return false). Example use:
 *      atogether( flash_led(), play_sound() );
 */
#define aboth( f , g )					\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
    ainit_child(2);					\
  case __LINE__: {					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (f_status.cont() || g_status.cont())		\
      return Adel::CONT;   }

/** auntileither
 *
 *  Semantics: execute c and f asynchronously until either one of them
 *  finishes (contrast with aboth). This construct behaves like a
 *  conditional statement: it should be followed by a true and option false
 *  statement, which are executed depending on whether the first function
 *  finished first or the second one did. Example use:
 *     auntil( button(), flash_led() ) { 
 *       // button finished first 
 *     } else {
 *       // light finished first
 *     }
 */
#define auntileither( f , g )				\
    a_me.line = __LINE__;				\
    ainit_child(1);					\
    ainit_child(2);					\
  case __LINE__: 					\
    adel_current = achild(1);				\
    f_status = f;					\
    adel_current = achild(2);				\
    g_status = g;					\
    if (f_status.cont() && g_status.cont()) return Adel::CONT;	\
    if (f_status.done())

/** afor
 * 
 * Adel-friendly for loop. The issue is that the Adel execution model makes
 * it hard to have local variables, like the loop control variable. 
 */
#define afor( v, cond, inc )				\
    a_me.line = __LINE__;				\
    a_me.wait = millis() + t;				\
  case __LINE__:					\
    if (millis() < a_me.wait) return Adel::CONT;
  

/** areturn
 * 
 *  Semantics: leave the function immediately, and communicate to the
 *  caller that it is done.
 */
#define areturn				   \
    a_me.line = ADEL_FINALLY;		   \
    return Adel::CONT;

#endif
