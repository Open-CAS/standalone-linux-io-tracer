Fuzzy test are executed using AFL (American Fuzzy Lop) fuzzer.
To use the tool the source code needs to be compiled using
supplied compiler - 'afl-g++' - to instrument the binary.

Then the fuzzing starts by passing the binary path to 'afl-fuzz'
binary as well as other parameters. Then the fuzzed values are fed to
iotrace's stdin, and are automatically changing in such a way to take as
many code branch paths as possible.

Because the values are fed to stdin we apply a code patch in tests
which redirects the stdin to argv or files. We also apply an another patch
to end the tracing immediately after starting it to verify complete code
paths (with deinitialization).