## Programming Assignment 3

Akira Youngblood, 2017-03-17

#### Building and Running

This project is set up to build and run on OS X. To build and run with the course-provided test files, simply run `make test`. There are two additional make targets provided: `make test-med` and `make test-big`. These will build a run the resolver on larger input sets (`-med` is 6 files, 100 domains each; `-big` is 6 files, 1000 domains each), as the input set provided by the assignment is not sufficient to accurately benchmark the program.

In order to build and run on Linux, a few small modifications to the makefile are in order:

* Add `-pthread` to `LIBS` (clang warns about an unused argument on OS X)
* Remove all flags except for `-Wall -Wextra` from `CFLAGS` (due to issues in `util.c/.h`, stricter warnings will prevent compilation on Linux)

When the program is finished, the elapsed CPU time (from `clock()`, provided by `time.h`) is displayed, as well as the number of resolver threads and the queue size used.

#### Benchmark Results

Using `input-med` test file group (six files, 100 domains per file, from Alexa's top 1M), on rMBP (2.6 GHz i5). `queueSize = 20`.

| Threads | Run 1 | Run 2 | Run 3 | Avg   |
|--------:|------:|------:|------:|------:|
| 1       | 28.93 | 26.97 | 23.10 | 26.33 |
| 2       | 13.83 | 12.96 | 12.96 | 13.25 |
| 4       | 6.86  | 3.59  | 3.67  | 4.70  |
| 8       | 1.47  | 0.67  | 0.99  | 1.04  |
| 16      | 0.57  | 0.47  | 0.45  | 0.49  |
| 32      | 0.49  | 0.45  | 0.46  | 0.46  |
| 64      | 0.66  | 0.53  | 0.51  | 0.56  |
| 128     | 0.45  | 0.74  | 0.54  | 0.57  |
