## Programming Assignment 3

Akira Youngblood, 2017-03-17

#### Building and Running

This project is set up to build and run on OS X. To build and run with the course-provided test files, simply run `make test`. There are two additional make targets provided: `make test-med` and `make test-big`. These will build a run the resolver on larger input sets (`-med` is 6 files, 100 domains each; `-big` is 6 files, 1000 domains each), as the input set provided by the assignment is not sufficient to accurately benchmark the program.

In order to build and run on Linux, a few small modifications to the makefile are in order:

* Add `-pthread` to `LIBS` (clang warns about an unused argument on OS X)
* Remove all flags except for `-Wall -Wextra` from `CFLAGS` (due to issues in `util.c/.h`, `-std=c11` will throw warnings and errors)

When the program is finished, the elapsed CPU time (from `clock()`, provided by `time.h`) is displayed, as well as the number of resolver threads and the queue size used.

#### Limits

MAX_INPUT_FILES: The number of input files is unbounded, but due to architectural limitations stops being unlimited once the number of files exceeds INT_MAX less two.

MAX_RESOLVER_THREADS: The number of resolver threads is set to four times the logical number of cores, as benchmarking has shown this to be performant.

MIN_RESOLVER_THREADS: As stated above, the minimum resolver thread count used is four (on a single core machine).

MAX_NAME_LENGTH: 1024 characters (1025 including null terminator). Domains are restricted to 253 characters (see [Restrictions on valid host names](https://en.wikipedia.org/wiki/Hostname#Restrictions_on_valid_host_names)), and therefore 1024 ought to be plenty.

MAX_IP_LENGTH: INET6_ADDRSTRLEN. This program only does IPv4, and therefore this is plenty.

#### Benchmark Results

Using `input-med` test file group on rMBP (2.6 GHz i5). `queueSize = 20`.

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

Using `input-big` test file group on elra-02

| Threads | Run 1 | Run 2 |
|--------:|------:|------:|
| 4       | 33.02 | 30.86 |
| 8       |
| 16      |
| 32      |
| 64      |
| 128     |
| 256     |
| 512     |
