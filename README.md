# HLPtool
A program for finding solutions to the Hex Layer Problem. 

# Usage
First, download the latest release for your system. Note that as of right now, only 64-bit x86 systems with AVX2 are supported. If you don't know what that means, most modern computers should be fine. Once you have downloaded it, simply navigate to the folder it is downloaded in and run it with `./hlpsolve` for Linux or `hlpsolve.exe` (in command prompt) for Windows.

The primary use is the hex solver, which can be accessed with `hlpt hex` or `hlpt hlp`. To input the function you want solved, type out the desired outputs in order using hexadecimal. For example:

```ShellSession
$ # square root, rounded down:
$ ./hlpt hex 0111222223333333
searching for 0111 2222 2333 3333
result found, length 4:  3^, 1>*;  9^, 4>*;  F^*, 6>*;  4^*, 3>*

$ # digits of pi:
$ ./hlpt hex 3141592653589793
searching for 3141 5926 5358 9793
result found, length 13:  9^, 8>*;  4^, 4>*;  E^, F>*;  5^, 5>*;  3^, 3>*;  E^, F>*;  7^, 8>*;  4^, 2>;  6^, 7>*;  A^, D>*;  F^, F>*;  7>, D>*;  3^*, 3>

$ # rotate top bit to bottom:
$ ./hlpt hex 02468ace13579bdf
searching for 0246 8ACE 1357 9BDF
result found, length 15:  8^, 7>*;  F^, E>*;  E^, D>*;  D^, C>*;  C^, B>*;  B^, A>*;  A^, 9>*;  9^, 8>*;  8^, 7>*;  7^, 6>*;  6^, 5>*;  5^, 4>*;  4^, 3>*;  3^, 2>*;  2^, 1>*
```

You can also insert spaces between any numbers without consequense. The program also repeats what you typed in, to help you make sure you typed it correctly.

The program can output in two different notations: star notation and V-notation. The default is V-notation.

```ShellSession
$ ./hlpt -n star hex 3141592653589793
searching for 3141 5926 5358 9793
result found, length 13:  9, *8;  4, *4;  E, *F;  5, *5;  3, *3;  E, *F;  7, *8;  4, 2;  6, *7;  A, *D;  F, *F;  ^7, *D;  *3, 3
```

# Lua Scripting
Possibly the most exciting new feature, you can perform a lot of automated solving using lua scripts (Lua 5.4 to be exact). See `test/test.lua` for a few examples on how to do this, and what options are available. Running it is simple:
```ShellSession
$ ./hlpt lua test/test.lua
[several lines of output]
```

# Ambiguous Searching

Sometimes, you may have an input that can be mapped to any output. This can easily be specified:
```ShellSession
$ ./hlpt hex 3.1.4.1.5.9.2.6.
searching for 3X1X 4X1X 5X9X 2X6X
result found, length 6 (3219 4511 5491 2365):  0^, D>*;  4^*, 7>;  A^, 9>*;  4^, 3>*;  7^*, 9>*;  9^, 9>*

$ ./hlpt hex 31415926
searching for 3141 5926 XXXX XXXX
result found, length 8 (3141 5926 2951 4133):  0^, E>*;  9^, 9>*;  B^, B>*;  9>, F>*;  2^, 0>;  E^, C>*;  5^*, D>*;  4^*, 4>
```

Previous versions also allowed for specifying a range of possibilities, however this functionality has been removed as it has since been found to be very flawed in how it attempts to handle such cases.

## Optimal solutions
The solutions found are almost always the shortest possible length. However, at times it produces a solution a layer or two longer than the true minimum. This is intentional but can be prevented by passing in `-p` or `--perfect`. However, this generally makes it take significantly longer to find a solution, and most of the time it's the same solution it would've found otherwise, which is why this is not the default behaviour. Though, it can, in very specific situations, be way off:

```ShellSession
$ ./hlpt hex 02468ace02468ace
searching for 0246 8ACE 0246 8ACE
result found, length 15:  8^, 7>*;  0^, F>*;  9^, 0>;  A^, 2>;  0^, 1>;  0^, 4>*;  3^, 0>;  B^, 5>*;  0^, 9>*;  E^, D>*;  F^, B>*;  B^, D>*;  4^, 0>;  1^*, 1>;  A^, 9>*

$ ./hlpt hex 02468ace02468ace -p
searching for 0246 8ACE 0246 8ACE
result found, length 10:  8^, 7>*;  0^, F>*;  C^, B>*;  D^, 8>*;  7^, B>*;  F^, F>*;  7>, D>*;  C^, E>*;  C^, D>*;  4^, 2>
```

## Dual Binary
The tool is also equipped with a dual binary solver, which can be accessed using `hlpt 2bin`. The main format is to list out all the first bits (starting at 0), then the second bits. However, this can be changed with `-t` to group the input by pairs instead of by output index, and `-s` to swap the bits, as if the chain was built mirrored. Like the hex solver, `.`, `x`, and leaving out the end can be used for wildcards. Unlike the hex solver, there is no `-p`, as the solver always produces optimal length solutions (barring unfound bugs).

# Compiling
honestly, i forgor. something with cmake.
