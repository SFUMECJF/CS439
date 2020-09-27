In any given program specification directory, there exists a specific `tests/`
directory which holds the expected return code, standard output, and standard
error in files called `n/rc`, `n/out`, and `n/err` (respectively) for each
test `n`. The testing framework just starts at `1` and keeps incrementing
tests until it can't find any more or encounters a failure. Thus, adding new
tests is easy; just add the relevant files to the tests directory at the
lowest available number.

The files needed to describe a test number `n` are:
- `rc`: The return code the program should return (usually 0 or 1)
- `out`: The standard output expected from the test
- `err`: The standard error expected from the test
- `run`: How to run the test (which arguments it needs, etc.)
- `desc`: A short text description of the test
- `pre` (optional): Code to run before the test, to set something up
- `post` (optional): Code to run after the test, to clean something up

The options for `run-tests.sh` include:
* `-h` (the help message)
* `-v` (verbose: print what each test is doing)
* `-t n` (run only test `n`)
* `-c` (continue even after a test fails)
* `-d` (run tests not from `tests/` directory but from this directory instead)
* `-s` (suppress running the one-time set of commands in `pre` file)
