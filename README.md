# HLPtool
A progam for finding solutions to the Hex Layer Problem. 

# Usage
First, download the latest release for your system. Note that as of right now, only 64-bit x86 systems with AVX2 are supported. If you don't know what that means, most modern computers should be fine. Once you have downloaded it, simply navigate to the folder it is downloaded in and run it with `./hlpsolve` for Linux or `hlpsolve.exe` (in command prompt) for Windows.

The primary use is the hex solver, which can be accessed with `hlpt hex` or `hlpt hlp`. To input the function you want solved, type out the desired outputs in order using hexadecimal. For example:

```ShellSession
$ # square root, rounded down:
$ ./hlpt hex 0111222223333333
searching for 0111 2222 2333 3333
result found, length 4:  3, *1;  9, *4;  *F, *6;  *4, *3

$ # digits of pi:
$ ./hlpt hex 3141592653589793
searching for 3141 5926 5358 9793
result found, length 13:  9, *8;  4, *4;  E, *F;  5, *5;  3, *3;  E, *F;  7, *8;  4, 2;  6, *7;  A, *D;  F, *F;  ^7, *D;  *3, 3

$ # rotate top bit to bottom:
$ ./hlpt hex 02468ace13579bdf
searching for 0246 8ACE 1357 9BDF
result found, length 15:  8, *7;  F, *E;  E, *D;  D, *C;  C, *B;  B, *A;  A, *9;  9, *8;  8, *7;  7, *6;  6, *5;  5, *4;  4, *3;  3, *2;  2, *1
```

You can also insert spaces between any numbers without consequense. The program also repeats what you typed in, to help you make sure you typed it correctly.

Sometimes, you may have multiple possible maps that will all work equally fine. Currently, the solver can accept ranges for each desired output by inserting a `-`. Again, spaces (and `[]` brackets) are completely optional, though maybe don't actually type it out like the following:
```ShellSession
$ ./hlpt hex 0-10-21-21-3 2-32-43-43-5 4-54-65-65-7 6-76-87-87-9
searching for [0-1][0-2][1-2][1-3] [2-3][2-4][3-4][3-5] [4-5][4-6][5-6][5-7] [6-7][6-8][7-8][7-9]
result found, length 5 (0011 2343 4565 6789):  0, 1;  3, *3;  7, *6;  B, *A;  *6, *6

$ ./hlpt hex 00-111-222-333-444-555-666-777-8
searching for 0[0-1]1[1-2] 2[2-3]3[3-4] 4[4-5]5[5-6] 6[6-7]7[7-8]
result found, length 7 (0011 2233 4455 6678):  3, 1;  5, *4;  7, *6;  9, *8;  B, *A;  D, *C;  *7, *6
```

However, by far the most common case of this is to allow any value at all, as that input value is simply not used. For this, you can use `.` or `x` as a shorthand for `[0-f]`. For extra convenience, if you do not provide all 16 values, the rest will be automatically filled in with `X`s. However, do note that this also means that there is no check to make sure you did provide all 16 values, aside from looking at the repeated request.
```ShellSession
$ ./hlpt hex 3.1.4.1.5.9.2.6.
searching for 3X1X 4X1X 5X9X 2X6X
result found, length 6 (3219 4511 5491 2365):  0, *D;  *4, 7;  A, *9;  4, *3;  *7, *9;  9, *9

$ ./hlpt hex 31415926
searching for 3141 5926 XXXX XXXX
result found, length 8 (3141 5926 2951 4133):  0, *E;  9, *9;  B, *B;  ^9, *F;  2, 0;  E, *C;  *5, *D;  *4, 4
```

## Optimal solutions
The solutions found are almost always the shortest possible length. However, at times it produces a solution a layer or two longer than the true minimum. This is intentional but can be prevented by passing in `-p` or `--perfect`. However, this generally makes it take significantly longer to find a solution, and most of the time it's the same solution it would've found otherwise, which is why this is not the default behaviour.

(There would be an example of this behaviour here, but a recent change made it better at finding optimal solutions and now the known unoptimal cases are optimal)

## Dual Binary
The tool is also equipped with a dual binary solver, which can be accessed using `hlpt 2bin`. The main format is to list out all the first bits (starting at 0), then the second bits. However, this can be changed with `-t` to group the input by pairs instead of by output index, and `-s` to swap the bits, as if the chain was built mirrored. Like the hex solver, `.`, `x`, and leaving out the end can be used for wildcards. Unlike the hex solver, there is no `-p`, as the solver always produces optimal length solutions (barring unfound bugs).

# Compiling
To compile, make sure you have `autoconf` installed and run the following (skip the `CFLAGS="-O4"` for debug build):

```ShellSession
$ autoreconf
$ ./configure CFLAGS="-O4" # Linux
$ ./configure CFLAGS="-O4" --build x86_64-linux-gnu --host x86_64-w64-mingw32 # Windows cross-compile
$ make
```

there's a chance you may need to use `gnulib-tool --import argp malloc-gnu` from the `gnulib` package.
