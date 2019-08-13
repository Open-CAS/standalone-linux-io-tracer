# Standalone Linux IO Tracer Case Study

## Introduction

Standalone Linux IO Tracer (iotrace) is a tool for block device I/O tracing
and management of created traces. In this document you will find out detailed
description how to use it and a few examples of collected traces analytics.

From high-level perspective, for each I/O to target device(s) basic metadata
information is captured (IO operation type, address, size), supplemented with
extended classification. Extended classification contains information about I/O
type (direct / filesystem metadata / file) and target file attributes(e.g. file
size). Afterwards traces can later be converted to JSON or CSV format and
consumed by any further analytics. iotrace does it and extends outputs with
post-processing fields like IO latency, IO queue depth, and more.

iotrace is based on [Open CAS Telemetry Framework (OCTF)](https://github.com/Open-CAS/open-cas-telemetry-framework). Collected traces are stored in format defined by OCTF using Google
Protocol Buffers description. You can find this definition [here](https://github.com/Open-CAS/open-cas-telemetry-framework/blob/master/source/octf/proto/trace.proto). Such trace format allows to analyze
it by any kind of tool written in Java, C++, Python, or other languages
(supported by Protocol Buffer)

The goodness of OCTF can be reused by other storage applications for instance
SPDK, QEMU. They can produce IO traces in the same OCTF traces format, which can
be used by the same analytics tools. But these are next steps.

## Getting traces

The iotrace [README](https://github.com/Open-CAS/standalone-linux-io-tracer/blob/master/README.md)
contains basic information how to install iotrace on your machine. Please follow
this instruction. Let's get a trace. Any time you can call the tool's help:

~~~{.sh}
iotrace --help
Usage: iotrace command [options...]

Available commands:
           --get-trace-repository-path           Returns location of trace repository path
     -T    --get-trace-statistics                Get trace statistics
     -G    --get-trace-summary                   Returns summary of specified trace
     -H    --help                                Print help
     -L    --list-traces                         Lists available traces
     -P    --parse-trace                         Parse trace files to human readable form
     -R    --remove-traces                       Removes specified trace(s)
           --set-trace-repository-path           Sets location of trace repository path
     -S    --start-trace                         Start IO tracing
     -V    --version                             Print version
~~~

We have to invoke _--start-trace_ command. Its help is: 

~~~{.sh}
Usage: iotrace --start-trace  --devices <VALUE>[,VALUE]  [options...]

Start IO tracing

Options that are valid with {-S | --start-trace}
     -b    --buffer <1-1024>                     Size of the internal trace buffer (in MiB) (default: 100)
     -d    --devices <VALUE>[,VALUE]             Paths of devices to be traced
     -l    --label <VALUE>                       User defined label
     -s    --size <1-100000000>                  Max size of trace file (in MiB) (default: 1000)
     -t    --time <1-4294967295>                 Max trace duration time (in seconds) (default: 4294967295)
~~~

Let's say, your workload/application is running on top of two block devices.
We would like to trace them for 1 hour, or until trace file is 1GiB. Also you
can label the trace with your name. We are going to call:

~~~{.sh}
sudo iotrace --start-trace --devices /dev/nvme0n1,/dev/nvme1n1 --time 3600 --size 1024 --label "My cool first trace"
~~~

But any time you can interrupt the trace collection by pressing Ctrl+C or sending 
termination signal to the iotrace process. After the tracing has stopped you
should get a trace summary: 

~~~{.sh}
{
 "tracePath": "kernel/2019-08-13_12:35:22",
 "state": "COMPLETE",
 "sourceNode": {
  "node": [
   {
    "id": "kernel"
   }
  ]
 },
 "traceStartDateTime": "2019-08-13 12:35",
 "traceDuration": "174",
 "traceSize": 3,
 "tracedEvents": "82906",
 "droppedEvents": "0",
 "queueCount": "16",
 "label": "My cool first trace"
}
~~~

Remember your trace path **kernel/2019-08-13_12:35:22**. We will use it for further
processing.

## Parsing traces

The iotrace parser converts IO traces from binary format to CSV or JSON format.
To parse you trace we are going to invoke _--parse-trace_ command.

~~~{.sh}
iotrce --parse-trace --help
Usage: iotrace --parse-trace  --path <VALUE>  [options...]

Parse trace files to human readable form

Options that are valid with {-P | --parse-trace}
     -f    --format {json|csv}                   Format of printed output, json - JSON output (default), csv - CSV output
     -p    --path <VALUE>                        Path to trace
     -r    --raw                                 Present trace as it had been recorded without post processing
~~~
  
To get CSV, call:

~~~{.sh}
iotrace --parse-trace --path kernel/2019-08-13_12:35:22 --format csv
~~~

You should get the CSV output:

~~~{.sh}
device.id,device.name,file.id,file.offset,file.size,header.sid,header.timestamp,io.error,io.flush,io.fua,io.ioClass,io.latency,io.lba,io.len,io.operation,io.qd
271581185,nvme0n1,,,,23,33493819292,false,false,false,1,144925,1953524992,8,Read,1
271581185,nvme0n1,,,,25,33493993812,false,false,false,1,113330,1953525152,8,Read,1
271581185,nvme0n1,,,,27,33494133732,false,false,false,1,100530,0,8,Read,1
...
271581184,nvme1n1,537233997,0,3,80572,160822194559,false,false,false,11,12530,366967080,8,Write,1
271581184,nvme1n1,268439476,0,12,80575,160822223349,false,false,false,12,13985,183234024,16,Write,1
271581184,nvme1n1,268439477,0,9,80578,160822252644,false,false,false,12,20540,183234040,16,Write,1
...
~~~

And in case of JSON the output is:

~~~{.sh}
{"header":{"sid":"23","timestamp":"33493819292"},"io":{"lba":"1953524992","len":8,"ioClass":1,"operation":"Read","flush":false,"fua":false,"error":false,"latency":"144925","qd":"1"},"device":{"id":"271581185","name":"nvme0n1"}}
{"header":{"sid":"25","timestamp":"33493993812"},"io":{"lba":"1953525152","len":8,"ioClass":1,"operation":"Read","flush":false,"fua":false,"error":false,"latency":"113330","qd":"1"},"device":{"id":"271581185","name":"nvme0n1"}}
{"header":{"sid":"27","timestamp":"33494133732"},"io":{"lba":"0","len":8,"ioClass":1,"operation":"Read","flush":false,"fua":false,"error":false,"latency":"100530","qd":"1"},"device":{"id":"271581185","name":"nvme0n1"}}
...
{"header":{"sid":"18834","timestamp":"134120569337"},"io":{"lba":"1465145088","len":88,"ioClass":14,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"32495","qd":"2"},"device":{"id":"271581185","name":"nvme0n1"},"file":{"id":"1610612801","offset":"112","size":"192"}}
{"header":{"sid":"18838","timestamp":"134120638297"},"io":{"lba":"904","len":120,"ioClass":14,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"40076","qd":"1"},"device":{"id":"271581185","name":"nvme0n1"},"file":{"id":"73","offset":"0","size":"203"}}
{"header":{"sid":"18840","timestamp":"134120664253"},"io":{"lba":"1024","len":88,"ioClass":14,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"30424","qd":"2"},"device":{"id":"271581185","name":"nvme0n1"},"file":{"id":"73","offset":"120","size":"203"}}
{"header":{"sid":"18844","timestamp":"134120707517"},"io":{"lba":"488382416","len":48,"ioClass":14,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"20330","qd":"1"},"device":{"id":"271581185","name":"nvme0n1"},"file":{"id":"536870985","offset":"0","size":"193"}}
...
~~~

## What do we collect, What do we process?

The below table contains telemetry content which is traced by iotrace. We also
included our future plans for tracing.

<table class="tg">
  <tr>
    <th>Category</th>
    <th>Telemetry Content</th>
    <th>Post processing information</th>
    <th>Status</th>
  </tr>
  <tr>
    <td>Basic IO metadata</td>
    <td>- Operation Type (Read/Write/Discard/Flush)<br>- IO Address (LBA)<br>- IO Length<br>- IO Result<br>- IO write Hint<br>- IO Class</td>
    <td>- IO Latency<br>- IO Queue Depth</td>
    <td>Implemented</td>
  </tr>
  <tr>
    <td>Filesystem metadata</td>
    <td>- File Id<br>- File Offset<br>- File Name</td>
    <td>- File Path</td>
    <td>In Progress</td>
  </tr>
  <tr>
    <td>Filesystem events</td>
    <td>- Dirs and files creation, renaming, moving, removal<br>- Dirs and files attributes<br><br></td>
    <td></td>
    <td>Future</td>
  </tr>
  <tr>
    <td>Application metadata</td>
    <td>- Process ID<br>- Process Name<br>- Application IO context</td>
    <td></td>
    <td>Future</td>
  </tr>
  <tr>
    <td>System Calls</td>
    <td>- Read/Write/Flush/etc. system calls<br></td>
    <td></td>
    <td>Future</td>
  </tr>
  <tr>
    <td>And many more</td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
</table> 

## IO Trace Analysis

### Statistics

Having IO trace, you can get basic IO statistics. To get it, please call:

~~~{.sh}
iotrace --get-trace-statistics --path kernel/2019-08-13_12:35:22 --format json
~~~

The JSON output as following:

~~~{.sh}
{
 "statistics": [
  {
   "desc": {
    "device": {
     "id": "271581184",
     "name": "nvme1n1",
     "size": 732585168
    }
   },
   "duration": "72732345281",
   "read": {
    "size": {
     "unit": "sector",
     "avarage": "18",
     "min": "1",
     "max": "256",
     "count": "243",
     "total": "4610"
    },
    "latency": {
     "unit": "ns",
     "avarage": "21683",
     "min": "8040",
     "max": "81775",
     "count": "243",
     "total": "5269187"
    },
    "metrics": [
     {
      "name": "throughput",
      "unit": "IOPS",
      "value": 3.3410169720387017
     },
     {
      "name": "bandwidth",
      "unit": "MiB/s",
      "value": 0.030948769131579568
     }
    ],
    "errors": "0"
   },
   "write": {
    "size": {
     "unit": "sector",
     "avarage": "53",
     "min": "1",
     "max": "256",
     "count": "20214",
     "total": "1091471"
    },
    "latency": {
     "unit": "ns",
     "avarage": "3700245",
     "min": "9928",
     "max": "32243381",
     "count": "20214",
     "total": "74796756022"
    },
    "metrics": [
     {
      "name": "throughput",
      "unit": "IOPS",
      "value": 277.92311552588609
     },
     {
      "name": "bandwidth",
      "unit": "MiB/s",
      "value": 7.3274802587449637
     }
    ],
    "errors": "0"
   },
...
~~~

If you wish you can print CSV output as well. In future we plan to provide more
advanced statistics like: histograms, percentiles, heat maps, timeseries graphs. 

### Other Analytics

TBD 

## Integration with other tools

TBD
