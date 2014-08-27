Arane
=====

Arane is a bytecode compiler and interpreter for the Perl 6 language written in
C++11 that I am currently working on.

Features
--------

The compiler is still in its early infancy, and so there's not a lot in it
yet. But the following is currently supported:

   * Simple type constraints
   * Arrays, strings, and big integers.
   * 'if' statement.
   * 'while', 'loop', and 'for' loop statements.
   * Basic subroutines (with optionally typed parameters and return value)
   * Packages and modules (all subroutines are exported at the moment)
   * A simple incremental mark-sweep garbage collector.
   
Here's a small code snippet that computes and prints the factorial of 100:
```perl6
package Foo {
  
  #= Computes N'th factorial.
  sub fact (int $n --> Int) {
    $n <= 1 ?? 1 !! $n * fact($n - 1)
  }
}

say Foo::fact 100;
```
   
Compiling and Running
---------------------

To compile Arane, you will need a relatively new GCC C++ compiler with support
for C++11, and a copy of CMake.  In the project's root directory, type:
```
cmake -G "Unix Makefiles"
make
```

The resulting executable could be then found inside the newly created `build`
directory (named `arane`).  To run a Perl file, type:
```
./arane <file>
```

And that's it!

