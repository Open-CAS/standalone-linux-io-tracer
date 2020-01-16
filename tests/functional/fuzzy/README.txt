Fuzzy test are executed using AFL (American Fuzzy Lop) fuzzer.
To use the tool the source code is compiled using
supplied compiler - 'afl-g++' - to instrument the binary.

Then the fuzzing starts by passing the tested binary path to 'afl-fuzz'
binary as well as other parameters. The fuzzed values are fed to
iotrace's stdin, and are automatically changing in such a way to take as
many code branch paths as possible.

Because the values are fed to stdin by default we apply a code patch in tests
which redirects the stdin to argv or files. We also apply an another patch
to end the tracing immediately after starting it to verify complete code
paths (with deinitialization).

Fuzzy tests can take some time to complete, you can ignore them by adding
the following to pytest invocation:
'--ignore=functional/fuzzy/'