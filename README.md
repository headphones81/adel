# adel
A new way to program microcontrollers

**NOTE:** This code is under active development, so the API could still change.

Adel is a library that makes it easier to program microcontrollers, such as the Arduino. The main idea is to provide a simple kind of concurrency, similar to coroutines. Using Adel, any function can be made to behave in an asynchronous way, allowing it to run concurrently with other Adel functions without interfering with them. The library is implemented entirely as a set of C/C++ macros, so it requires no new compiler tools or flags. Just download the Adel directory and install it in your Arduino IDE libraries folder.

## Overview and examples

Adel came out of my frustration with microcontroller programming. In particular, that seemingly simple behavior can be very hard to implement. As an example, consider a function that blinks an LED attached to some pin every N milliseconds:

```{c++}
void blink(int some_pin, int N) {
   digitalWrite(some_pin, HIGH);
   delay(N);
   digitalWrite(some_pin, LOW);
   delay(N);
}
```

OK, that's easy enough. I can call it with two different values, say 500ms and 300ms:

```{c++}
for (int i = 0; i < 100; i++) blink(3, 500);
for (int i = 0; i < 100; i++) blink(4, 300);
```

But what if I want to blink them **at the same time**? Now, suddenly, I have lost composability. This code doesn't work:

```{c++}
for (int i = 0; i < 100; i++) {
  blink(3, 500);
  blink(4, 300);
}
```

To get **concurrent** behavior I have to write completely different code, which makes the scheduling explicit:

```{c++}
int last300, last500;
loop() {
  uint32_t t = millis();
  if (t - last300 > 300) {
    if (pin3_on) digitalWrite(3, LOW);
    else digitalWrite(3, HIGH);
    last300 = t;
  }
  if (t - last500 > 500) {
    ... etc ...
 }
 ```

Aside from the obvious complexity of this code, there are a couple of specific problems. First, all behaviors that might occur concurrently must be part of the same loop with the same timing control. The modularity is completely gone. Second, we need to introduce a global variable for each behavior that remembers the last time it executed. The code would become significantly more complex if we wanted to blink the lights for a specific amount of time, or if we had other modes where the lights are not blinking. Similar problems arise with input as well. Imagine if we want to blink a light until a button is pressed (inluding debouncing the button signal). 

The central problem is the `delay()` function, which makes timing easy for individual behaviors, but blocks the whole processor. The key feature of Adel, therefore, is an asynchronous delay function called `adelay` (hence the name Adel). The `adelay` function works just like `delay`, but allows other code to run concurrently. 

Concurrency in Adel is specified at the function granularity, using a fork-join style of parallelism. Functions are designated as "Adel functions" by defining them in a stylized way. The body of the function can use any of the Adel library routines shown below:

* `adelay( T )` : asynchronously delay the current function for T milliseconds.
* `andthen( f )` : run Adel function `f` to completion before continuing.
* `await( c )` : wait asynchronously until condition `c` is true (`c` must *not* be an Adel function).
* `aforatmost( T, f )` : run Adel function `f` until it completes, or T milliseconds (whichever comes first)
* `atogether( f , g )` : run Adel functions `f` and `g` concurrently until they **both** finish.
* `auntil( f , g )` : run Adel function `g` until `f` completes.
* `auntileither( f , g ) { ... } else { ... }` : run Adel functions `f` and `g` concurrently until **one** of them finishes. Executes the true branch if `f` finishes first or the false branch if `g` finishes first.
* `afinish` : finish executing the current function (like a return)
* `alternate( f , g )` : run `f` continuously until it yields by calling `ayourturn`; then run `g` until it yields. Continue back and forth until either function completes.
* `ayourturn( v )` : use in a function being called by `alternate` to yield control to the other function. The value `v` is made available to the other function.
* `amyturn` : gets the value passed through by `ayourturn`.

Using these routines we can rewrite the blink routine (below). Notice that I added a `while (1)` infinite loop -- this function will blink the light forever, or until it is stopped by its caller. *And that's ok* because it will not stop other code from running.

```{c++}
adel blink(int some_pin, int N) 
{
  abegin:
  while (1) {
    digitalWrite(some_pin, HIGH);
    adelay(N);
    digitalWrite(some_pin, LOW);
    adelay(N);
  }
  aend;
}
```

Every Adel function contains a minimum of three things: return type `adel`, and macros `abegin:` and `aend` at the begining and end of the function. But otherwise, the code is almost identical. The key feature is that we can run blink concurrently, like this:

```{c++}
atogether( blink(3, 500), blink(4, 500) );
```

This code does exactly what we want: it blinks the two lights at different intervals at the same time. The `atogether` macro waits until both functions are complete, which is not always desirable. For example, we might want to blink a light until a button is pressed. Assuming we have a `button` function (shown later), we can use the `auntil` construct:

```{c++}
auntil( button(pin), blink(3, 350) );
```

The semantics are simple: when the `button` routine completes, `auntil` simply stops calling the `blink` routine, in effect interrupting it at the last point it yielded. Currently, `blink` has no opportunity to respond to this interruption or clean up in any way.

The same construct could be use to implement a timeout by defining a function that simply delays for a specified amount of time:

```{c++}
adel timeout(int ms) {
  abegin:
  adelay(ms);
  aend;
}
```

To blink for 2 seconds we write:

```{c++}
auntil( timeout(2000), blink(3, 350) );
```

Timeouts are so common, though, that Adel supports this construct directly, without having to define a separate function:

```{c++}
aforatmost( 2000, blink(3, 350) );
```

One special feature of `aforatmost` is that it behaves like a conditional, where the true branch is executed only when the timeout occurs first, and the optional false branch is executed if the function finishes before the timeout:

```{c++}
aforatmost( 2000, blink(3, 350) ) {
    // -- Timeout happened before blinking finished
}
```

The `auntil` construct has a similar analogue called `auntileither`, which executes two functions concurrently (like `atogether`), but stops when **either** one finishes. The true branch is executed if the first one finishes first; the false branch is executed if the second one finishes first:

```{c++}
auntileither( button(pin), blink(3, 350) ) {
    // -- User hit the button
} else {
    // -- blink completed
}
```

Here is the `button()` function, which returns when the user presses the button. It uses the `await` construct to wait for the pin to go high or low:

```{c++}
adel button(int pin)
{
  abegin:
   await (digitalRead(pin) == HIGH);
   adelay (50);
   if (digitalRead(pin) == HIGH) {
     await (digitalRead(pin) == LOW);
   }
  aend;
}
```

## Local variables

One of the challenges in Adel is supporting local variables. From the standpoint of the underlying C runtime, control enters and exits each function many times before it finishes. Each time it exists, any local variables disappear and lose their values. The latest version of Adel allows the user to declare local variables using the `avars` construct. Syntactically, these variables look like local variables, but they are secretly held in storage on the heap. These variables must be accessed througn the `my` macro, which hides some pointer junk.

```{c++}
adel counter()
{
  avars {
    int i;
  }
  abegin:
    for (my(i) = 0; my(i) < 100; my(i)++) {
      digitalWrite(pin, my(i));
      adelay(100);
    }
  aend;
}
```

It's not pretty, but it works!

## Your turn, my turn

Classic coroutines allow a function to yield to its caller **without** losing track of where it is currently executing. Subsequent entry to the function continues where it left off. The problem with this approach is that it requires an explicit "init" to start the function, followed by repeated invocations ("next") until it is done. 

Instead, Adel limits this behavior to single `alternate` construct, which takes two functions and alternates executing each until it calls `ayourturn`, which is like "yield". A single integer value can be passed between them to communicate a value. In this example the button routine is augmented with a yield when the button is held down; the value passed is how long it has been held. We can use this version to make an LED get brighter and brighter until the button is released.

Here is the augmented button routine. It uses a local variable to keep track of how much time has elapsed since the button was initially pressed.

```{c++}
adel button(int pin)
{
  avars {
      uint32_t starttime;
  }
  abegin:
    await (digitalRead(pin) == HIGH);
    my(starttime) = millis();
    adelay (50);
    if (digitalRead(pin) == HIGH) {
      while (digitalRead(pin) != LOW) {
        ayourturn(millis() - my(starttime));
      }
    }
    
  aend;
}
```

Here is a simple function that receives these values and sets the LED brightness accordingly. Notice the use of `amyturn`, which gets the value sent by the button.

```{c++}
adel brighten(int pin)
{
  avars {
    int level;
  }
  abegin:
    while (1) {
      analogWrite(pin, my(level));
      ayourturn(0);
      my(level) = map(amyturn, 0, 10000, 0, 256);
    }
    
  aend;
}
```

Finally, we can connect these two in the caller to get the aggregate behavior:

```{c++}
alternate( button(2) , brighten(11) );
```

## Top-level loop

Since the top-level loop function in an Arduino program is not an Adel function, we need some machinery to get the whole execution process started. The simplest construct is `arepeat`, which just executed the whole Adel program over and over. For example, if your program creates an elaborate light pattern, `arepeat` will keep playing the pattern repeatedly.

```{c++}
void loop()
{ 
  arepeat( mylightshow() );
}
```

One nice thing about the top-level loop is that you can put as many of these "processes" as you want, and they all run concurrently:

```{c++}
void loop()
{ 
  arepeat( mylightshow() );
  arepeat( mysoundshow() );
}
```

If you really only want the program to run one time, you can write `aonce`, but the device will not do anything else until it is restarted.

```{c++}
void loop()
{ 
  aonce( mylightshow() );
}
```

Finally, at the top level you can schedule Adel functions to be run at a specific interval using `aevery`. For example, let's say you want to check for some user input every 500ms throughout the whole light show, you can write this:

```{c++}
void loop()
{ 
  arepeat( mylightshow() );
  aevery( 500, checkforinput() );
}
```

## WARNINGS

(1) Be very careful using `switch` and `break` inside Adel functions. The co-routine implementation encloses all function bodies in a giant switch statement to allow them to be reentrant. Adding other switch and break statements can have unpredictable results.

(2) The internal stack that keeps track of concurrent functions has a limited depth determined at compile time. The number of levels must be specified in the `adel_init` macro -- 4 or 5 is a good number for most applications.

(3) Loops, like `for` and `while`, are perfectly fine to use inside Adel functions, but make sure that there is at least one Adel function (like `adelay`) in the body, so that the loop does not stall the rest of the program.

