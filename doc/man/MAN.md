# iotrace - collect and manage block I/O traces

```
iotrace --start-trace -d device[,device,...] [-b buffer_size] [-s max_file_size] [-t seconds]
iotrace --parse-trace [--format json] --path path
iotrace --get-trace-summary --path path
iotrace --list-traces [--prefix prefix]
iotrace --remove-traces --prefix prefix
```


# Description

iotrace provides block device IO tracing and trace management capabilities. For each IO to target device(s) basic metadata information is captured (IO operation type, address, size), supplemented with extended  classification. Extended classification contains information about I/O type (direct / filesystem metadata / file) and target file attributes (e.g. file size). iotrace is based on Open CAS Tracing Framework (OCTF), see https://github.com/Open-CAS/open-cas-telemetry-framework project page for more details. Collected traces are stored in OCTF trace location. Traces can later be converted to human readable form using --parse-trace command.


# Commands


* --start-trace, -S
  Start collecting IO traces.
* --parse-trace, -P
  Convert collected trace to human readable form.
* --get-trace-summary, -G
  Display summary of specified trace.
* --list-traces, -L
  List collected traces.
* --remove-traces, -R
  Remove specified traces.
* --version, -V
  Print version info.
* --help, -H
  Display command help.


# Options


* **-d, --devices &lt;DEV1*&gt;[,DEV2,...]**
  List of one or more block devices


* **-b, --buffer &lt;BUFFER_SIZE&gt;**
  Size of internal circular buffer for IO traces (in units of MiB). Increasing this parameter reduces risk of dropping traces at the cost of increased memory usage. Default value is 100. Accepted value range is [1..1024].


* **-s, --size &lt;SIZE&gt;**
  Maximum size of trace file (in units of MiB). Tracing is stopped after output file reaches specified maximum size. Default value is 1000. Accepted value range is [1..10^9].


* **-t, --time &lt;SECONDS&gt;**
  Maximum tracing duration (in seconds). Tracing is stopped after specified time elapses. Default value is 1000. Accepted value range is [1..2^32-1].


* **--format json**
  Parsing output format. Currently the only supported format is json.


* **-p --path &lt;PATH&gt;**
  Trace file path, relative to OCTF trace location.


* **-p --prefix &lt;PREFIX&gt;**
  Trace file prefix, relative to OCTF trace location. End prefix with asterisk ("*") to match multiple files or provide full relative path for a single trace.


* **-f --force**
  Enforces removing specified traces in any state. Without this flag only
completed and failed traces can be removed.

# Files

Traces are stored in location specified in OCTF configuration file /etc/octf/octf.conf, default is /var/lib/octf/trace.


# Example

~~~{.sh}
# Start tracing
iotrace --start-trace -d /dev/vda,/dev/vdb -b 10 -s 100 -t 20

# List all traces
iotrace --list-traces

# List traces prefixed by "kernel" 
iotrace --list-traces --prefix "kernel/*"

# Parse the trace
iotrace --parse-trace --path kernel/2019-05-15_18:40:34 --format json

# Get the trace summary
iotrace --get-trace-summary --path kernel/2019-05-15_18:40:34

# Remove the specific trace 
iotrace --remove-traces --prefix kernel/2019-05-15_18:40:34

# Remove traces prefixed by 'kernel' and created in 2019
iotrace --remove-traces --prefix "kernel/2019*"

# Remove all completed and failed traces
iotrace --remove-traces --prefix "*"

# Remove all traces (including running and inconsistent)
iotrace --remove-traces --prefix "*" --force
~~~
