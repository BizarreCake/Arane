P6
==

P6 is a bytecode compiler and interpreter for the Perl 6 language that I am
currently working on.  I'm not good with names, so the compiler's called P6
for now.

Features
--------

The compiler is still in its early infancy, and so there's not a lot in it
yet. But the following is currently supported:

   * Arrays and strings
   * 'if' statement.
   * 'while', 'loop', and 'for' loop statements.
   * Basic subroutines
   * Packages and modules (all subroutines are exported at the moment)
   * A simple garbage collector (see caveats)
   
The following code snippet is an example of what can be compiled right now:
```perl
package Primes {
  
  sub is_prime ($n) {
    for 2..^$n -> $i {
      if $n % $i == 0 {
        return 0;
      }
    }
    return 1;
  }

  sub gen_primes ($count) {
    my $primes = [];
    my $pc = 0;
    loop (my $i = 2; $i < $count; $i = $i + 1) {
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
   
Caveats
-------

The current garbage collector is pretty simple and inefficient (a basic
mark-and-sweep collector that gets called in every 1000 allocations).
Don't worry though! A proper moving/compacting generational incremental
garbage collector will be implemented in the future.
   

