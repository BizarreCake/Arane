Arane
=====

Arane is a bytecode compiler and interpreter for the Perl 6 language written in
C++11 that I am currently working on.

Features
--------

The compiler is still in its early infancy, and so there's not a lot in it
yet. But the following is currently supported:

   * Arrays and strings
   * 'if' statement.
   * 'while', 'loop', and 'for' loop statements.
   * Basic subroutines
   * Packages and modules (all subroutines are exported at the moment)
   * A simple incremental mark-sweep garbage collector.
   
The following code snippet is an example of what can be compiled right now:
```perl
package Primes {
  
  #= Computes the integer square root of a number.
  sub isqrt ($s) {
    my $n = $s / 2;
    for 1..15 -> $i {
      my $m = ($n + $s/$n)/2;
      if $m == $n {
        return $n;
      }
      $n = $m;
    }
    return $n;
  }
  
  #= Checks whether a number is prime.
  sub is_prime ($n) {
    for 2..isqrt($n) -> $i {
      if $n % $i == 0 {
        return 0;
      }
    }
    return 1;
  }

  #= Generates primes number up to the specified upper bound.
  sub gen_primes ($count) {
    my $primes = [];
    my $pc = 0;
    loop (my $i = 3; $i < $count; $i = $i + 2) {
      if is_prime $i {
        @{$primes}[$pc] = $i;
        $pc = $pc + 1;
      }
    }
    
    return $primes;
  }
} 


my $primes = Primes::gen_primes 100;
my @arr = @{$primes};
for @arr -> $p {
  print "$p ";
}
print "\n";
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

