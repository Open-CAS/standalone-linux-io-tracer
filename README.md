# Standalone Linux IO Tracer

## NEWS
Standalone Linux IO Tracer switched to using **eBPF** for capturing traces.
Previously, the tracer ran custom loadable kernel module for that. eBPF tracing
method is more secure for user. The version which uses the kernel module will
be obsoleted. In case you want to run old version, switch to this branch:
[master-kernel](https://github.com/Open-CAS/standalone-linux-io-tracer/tree/master-kernel)

# Description

Standalone Linux IO Tracer (iotrace) is a tool for block device I/O tracing
and management of created traces

For each I/O to target device(s) basic metadata information is captured
(IO operation type, address, size), supplemented with extended
classification. Extended classification contains information about I/O type
(direct / filesystem metadata / file) and target file attributes(e.g. file
size).

iotrace is based on [Open CAS Telemetry Framework (OCTF)](https://github.com/Open-CAS/open-cas-telemetry-framework).
Collected traces are stored in OCTF trace location. Traces can later be
converted to JSON or CSV format.

The iotrace executable (iotrace command line application) includes an eBPF
program which is loaded to the Linux kernel during tracing. The eBPF program
captures trace information and shares them to the userspace iotrace application.
This is serialized to the OCTF IO trace.

# In this readme:

* [Supported OS](#os_support)
* [Source Code](#source)
* [Deployment](#deployment)
* [Theory Of Operation](#theory_of_operation)
* [Examples](#examples)
* [Contributing](#contributing)
* [Tests](#tests)

<a id="os_support"></a>

## Supported OS

Right now the compilation of Standalone Linux IO Tracer is tested on the
following OSes:

| OS     | Version | Kernel Version |
| ------ | ------- | -------------- |
| Fedora | 36      | 5.18.10        |
| Ubuntu | 22.04   | 5.15.0         |

<a id="source"></a>

## Source Code

Source code is available in the official Standalone Linux IO Tracer GitHub repository:

~~~{.sh}
git clone https://github.com/open-cas/standalone-linux-io-tracer
cd standalone-linux-io-tracer
~~~

<a id="deployment"></a>

## Deployment

### Prerequisites

* To build and use Standalone Linux IO Tracer, setup prerequisites first in the following way:

  ~~~{.sh}
  git submodule update --init --recursive
  sudo ./setup_dependencies.sh
  ~~~

  Installed dependencies include [OCTF](https://github.com/Open-CAS/open-cas-telemetry-framework),   Google Protocol Buffers, CMake and Google
  Test. The dependencies are either installed with dnf/yum/apt or installed
  to a dedicated directory /opt/octf/ to avoid overwriting already installed ones.

### Build

To build the iotrace executable invoke:
~~~{.sh}
make
~~~

You can try to create rpm/deb installation package.
~~~{.sh}
make package
~~~
For example in case of Fedora OS, the generated installation package is located
in build/release/iotrace-XX.YY.ZZ-1.x86_64.rpm.

### Installation

To install iotrace call:
~~~{.sh}
sudo make install
~~~

Also you can try to install iotrace using rpm/deb package:
~~~{.sh}
rpm -Uvh iotrace-XX.YY.ZZ-1.x86_64.rpm.
~~~

<a id="theory_of_operation"></a>

## Theory of operation

Standalone Linux IO Tracer captures request data by registering to multiple
trace points surfaced by the Linux kernel (e.g. BIO queueing, BIO splitting,
BIO completion). This allows for gathering of IO metadata at the request level
and passing it between kernel and userspace.

A perf buffer is allocated and shared between the eBPF program and the userspace
application. The below example shows a recorded traces event.

```c
struct iotrace_event_hdr {
    /** Event sequence ID */
    log_sid_t sid;

    /** Time stamp */
    uint64_t timestamp;

    /** Trace event type, iotrace_event_type enunerator */
    uint32_t type;

    /** Size of this event, including header */
    uint32_t size;
} __attribute__((packed, aligned(8)));

...

struct iotrace_event {
    /** Trace event header */
    struct iotrace_event_hdr hdr;
    /**
     * @brief IO ID
     *
     * This ID can be used by the tracing environment to assign an ID to the IO.
     *
     * @note Zero means not set.
     */
    uint64_t id;

    /** Address of IO in sectors */
    uint64_t lba;

    /** Size of IO in sectors */
    uint32_t len;

    /** IO class of IO */
    uint32_t io_class;

    /** Device ID */
    uint32_t dev_id;

    /** Operation flags: flush, fua, ... .
     * Values according to iotrace_event_flag_t enum
     * are summed (OR-ed) together. */
    uint32_t flags;

    /** Operation type: read, write, discard
     * (iotrace_event_operation_t enumerator) **/
    uint8_t operation;

    /** Write hint associated with IO */
    uint8_t write_hint;
} __attribute__((packed, aligned(8)));
```

The events declaration file can be found [here](https://github.com/Open-CAS/open-cas-telemetry-framework/blob/master/source/octf/trace/iotrace_event.h).

The userspace part of the Standalone Linux IO Tracer reads the entries from
the perf buffer and translates them into Google Protocol Buffer format
(see example below), for easier portability. The data is then serialized in
trace files in a per CPU basis (e.g. octf.trace.0).


```protobuf
message EventHeader {
    /** Event sequence ID */
    uint64 sid = 1;

    /** Time stamp */
    uint64 timestamp = 2;
}

...

enum IoType {
    UnknownIoType = 0;
    Read = 1;
    Write = 2;
    Discard = 3;
}

...

message EventIo {
    /** Address of IO in sectors */
    uint64 lba = 1;

    /** Size of IO in sectors */
    uint32 len = 2;

    /** IO class of IO */
    uint32 ioClass = 3;

    /** Device ID */
    uint64 deviceId = 4;

    /** Operation type: read, write, trim */
    IoType operation = 5;

    /** Flush flag */
    bool flush = 6;

    /** FUA flag */
    bool fua = 7;

    /** Write (lifetime) hint */
    uint32 writeHint = 8;

    /**
     * This ID can be used by the tracing environment to assign an ID to the IO.
     * Zero means not set.
     */
    uint64 id = 9;
}

...

message Event {
    /** Trace event header */
    EventHeader header = 1;

    oneof EventType {
        EventIo io = 2;
        EventDeviceDescription deviceDescription = 3;
        EventIoFilesystemMeta filesystemMeta = 4;
        EventIoCompletion ioCompletion = 5;
        EventIoFilesystemFileName filesystemFileName = 6;
        EventIoFilesystemFileEvent filesystemFileEvent = 7;
    }
}
```

The protobuf events declaration file can be found [here](https://github.com/Open-CAS/open-cas-telemetry-framework/blob/master/source/octf/proto/trace.proto).

You may see the results of translating into the above protobuf format,
by executing the following
command:
~~~{.sh}
iotrace --trace-parser --io --path "kernel/2020-07-02_08:52:51" --raw
~~~

Output:
~~~{.sh}
...
{"header":{"sid":"1","timestamp":"14193058940837"},"deviceDescription":{"id":"271581186","name":"nvme0n1","size":"732585168","model":"INTEL SSDPED1K375GA"}}
{"header":{"sid":"73","timestamp":"14196894550578"},"io":{"lba":"1652296","len":256,"ioClass":19,"deviceId":"271581186","operation":"Write","flush":false,"fua":false,"writeHint":0,"id":"110842991263647"}}
{"header":{"sid":"74","timestamp":"14196894550696"},"filesystemMeta":{"refSid":"110842991263647","fileId":{"partitionId":"271581186","id":"76","creationDate":"2020-07-02T06:52:55.712990641Z"},"fileOffset":"0","fileSize":"241960"}}
...
~~~
After tracing is complete these singular trace events may be parsed, combined
and translated into different Google Protocol Buffer messages (or other formats,
such as CSV) when executing Standalone Linux IO Tracer trace parser commands.

For example the **--trace-parser --io** command analyzes multiple submission,
split and completion events to give a more complete view of a given IO request
such as: its latency, queue depth, file size and path (if applicable) etc.

<a id="examples"></a>

## Examples

* Start tracing two block devices for 1 hour, or until trace file is 1GiB:
  ~~~{.sh}
  sudo iotrace --start-tracing --devices /dev/sda,/dev/sdb1 --time 3600 --size 1024
  ~~~

  * The below output example is based on sample traces found [here](https://github.com/Open-CAS/standalone-linux-io-tracer/blob/master/doc/resources/sample_trace.tar.xz).
  The traces were captured during YCSB workload type A benchmark on RocksDB.

  Traces can be unpacked using the following command:
  ~~~{.sh}
  tar -xf sample_trace.tar.xz
  ~~~
  They can then be moved to the trace repository directory, the path of which
  can be extracted using:

  ~~~{.sh}
  iotrace --trace-config --get-trace-repository-path
  ~~~

* List created traces:

  ~~~{.sh}
  iotrace --trace-management --list-traces
  ~~~

  Output:

  ~~~{.sh}
  {
  "trace": [
    {
     "tracePath": "kernel/2020-07-02_08:52:51",
     "state": "COMPLETE"
     "tags": {}
    }
  ]
  }
  ~~~

* Parse traces (note usage of path returned in --list-traces):

  ~~~{.sh}
  iotrace --trace-parser --io --path "kernel/2020-07-02_08:52:51" --format json
  ~~~

  Output:

  ~~~{.sh}
  {"header":{"sid":"1","timestamp":"3835590186"},"io":{"lba":"1652296","len":256,"ioClass":19,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"83797","qd":"1","writeHint":0},"device":{"id":"271581186","name":"nvme0n1","partition":"271581186","model":"INTEL SSDPED1K375GA"},"file":{"id":"76","offset":"0","size":"241960","path":"/000014.sst","eventType":"Access","creationDate":"2020-07-02T06:52:55.712990641Z"}}
  {"header":{"sid":"2","timestamp":"3835625267"},"io":{"lba":"1652552","len":256,"ioClass":19,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"95069","qd":"2","writeHint":0},"device":{"id":"271581186","name":"nvme0n1","partition":"271581186","model":"INTEL SSDPED1K375GA"},"file":{"id":"76","offset":"256","size":"241960","path":"/000014.sst","eventType":"Access","creationDate":"2020-07-02T06:52:55.712990641Z"}}
  {"header":{"sid":"3","timestamp":"3835638717"},"io":{"lba":"1652808","len":256,"ioClass":19,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"130094","qd":"3","writeHint":0},"device":{"id":"271581186","name":"nvme0n1","partition":"271581186","model":"INTEL SSDPED1K375GA"},"file":{"id":"76","offset":"512","size":"241960","path":"/000014.sst","eventType":"Access","creationDate":"2020-07-02T06:52:55.712990641Z"}}
  {"header":{"sid":"4","timestamp":"3835652180"},"io":{"lba":"1653064","len":256,"ioClass":19,"operation":"Write","flush":false,"fua":false,"error":false,"latency":"203209","qd":"4","writeHint":0},"device":{"id":"271581186","name":"nvme0n1","partition":"271581186","model":"INTEL SSDPED1K375GA"},"file":{"id":"76","offset":"768","size":"241960","path":"/000014.sst","eventType":"Access","creationDate":"2020-07-02T06:52:55.712990641Z"}}
  ...
  ~~~

  > **NOTE:**  Any mention of LBA assumes a 512B sector size, even
  if the underlying drive is formatted to 4096B sectors. Similarly
  a 512B sector is the unit of length.

* Show trace statistics:
  ~~~{.sh}
  iotrace --trace-parser --statistics -p "kernel/2020-07-02_08:52:51/"
  ~~~

  Output:

  <pre>
  "statistics": [
  {
   "desc": {
    "device": {
     "id": "271581186",
     "name": "nvme0n1",
     "size": "732585168" > <b> NOTE: </b> In sectors
    }
   },
   "duration": "24650525234", <b> NOTE: </b> In nanoseconds
   ...
   "write": { <b> NOTE: </b> Statistics for writes
    "size": {
     "unit": "sector",
     "average": "254", <b> NOTE: </b> Average write I/O size in sectors
     "min": "1", <b> NOTE: </b> Minimum write I/O size in sectors
     "max": "256", <b> NOTE: </b> Maximum write I/O size in sectors
     "total": "16338524", <b> NOTE: </b> Total write I/O size in sectors
     ...
    "latency": {
     "unit": "ns",
     "average": "15071365", <b> NOTE: </b> Average write I/O latency in ns
     "min": "9942", <b> NOTE: </b> Minimum write I/O latency in ns
     "max": "63161202", <b> NOTE: </b> Maximum write I/O latency in ns
    ...
    "count": "64188", <b> NOTE: </b> Number of write operations
    "metrics": {
     "throughput": {
      "unit": "IOPS",
      "value": 2603.9201757643164 <b> NOTE: </b> Average write IOPS
     },
     "workset": {
      "unit": "sector",
      "value": 2237616 <b> NOTE: </b> Number of distinct sectors written
     },
     "bandwidth": {
      "unit": "MiB/s",
      "value": 323.63590009317039 <b> NOTE: </b> Average write bandwidth
     }
  ...
  </pre>

  > **NOTE:**  Similar statistics exist for any read, discard or flush operations.
  There's a section for the combined statistics as well (Total).


* Show file system statistics:
  ~~~{.sh}
  iotrace --trace-parser --fs-statistics -p "kernel/2020-07-02_08:52:51/"
  ~~~

  Output:

  <pre>
  "entries": [
   {
    "deviceId": "271581186",
    "partitionId": "271581186",
    "statistics": {
   ...
   "write": {
     "size": {
      "unit": "sector",
      "average": "255", <b> NOTE: </b> Average write I/O size in sectors
      "min": "8", <b> NOTE: </b> Minimum write I/O size in sectors
      "max": "256", <b> NOTE: </b> Maximum write I/O size in sectors
      "total": "16336216", <b> NOTE: </b> Total write I/O size in sectors
      "percentiles": {}
     },
     "count": "63984",
     "metrics": {
      "throughput": {
       "unit": "IOPS",
       "value": 2602.9390810897676
      },
      "workset": {
       "unit": "sector",
       "value": 2237096 <b> NOTE: </b> Number of distinct sectors written
      },
      "write invalidation factor": {
       "unit": "",
       "value": 7.3024206381845032 <b> NOTE: </b> Average number of times a given sector in the workset is written to
      },
      "bandwidth": {
       "unit": "MiB/s",
       "value": 324.49957478019985 <b> NOTE: </b> Average write bandwidth
      }
     },
     ...
     "directory": "/" <b> NOTE: </b> Directories are relative to the filesystem mountpoint
  </pre>

  > **NOTE:**  Similar statistics exist for any read, discard or flush operations.
  There's a section for the combined statistics as well (Total).

  > **NOTE:**  File system statistics are gathered for detected groups of related
  I/O requests with common attributes like directory, file extension, file name
  prefix or IO class.


* Show latency histogram:
  ~~~{.sh}
  iotrace --trace-parser --latency-histogram -p "kernel/2020-07-02_08:52:51/"
  ~~~

  Output:

  <pre>
  ...
   "write": {
    "unit": "ns",
    "range": [
     {
      "begin": "0",
      "end": "0",
      "count": "0"
     },
     ...
     {
      "begin": "8192", <b> NOTE: </b> Minimum bound of latency histogram bucket in nanoseconds
      "end": "16383", <b> NOTE: </b> Maximum bound of latency histogram bucket in nanoseconds
      "count": "95" <b> NOTE: </b> Number of I/O requests in the bucket
     },
  ...
  </pre>

  > **NOTE:**  In the above example 95 write requests had latency between 8192 and 16383 ns.

  > **NOTE:**  Similar statistics exist for any read, discard or flush operations.
  There's a section for the combined statistics as well (Total).

* Show latency histogram:
  ~~~{.sh}
  iotrace --trace-parser --lba-histogram -p "kernel/2020-07-02_08:52:51/"
  ~~~

  Output:


  <pre>
  ...
   "write": {
    "unit": "sector",
    "range": [
     {
      "begin": "0",  <b> NOTE: </b> Minimum disk LBA bound of latency histogram bucket
      "end": "20479", <b> NOTE: </b> Maximum disk LBA bound of latency histogram bucket
      "count": "177" <b> NOTE: </b> Number of I/O requests in the bucket
     },
  ...
  </pre>

  > **NOTE:**  In the above example 177 write requests were issued to disk LBAs 0 through 20479.

  > **NOTE:**  Similar statistics exist for any read, discard or flush operations.
  There's a section for the combined statistics as well (Total).
<a id="tests"></a>

## Tests

See our tests [README](tests/README.md)

<a id="contributing"></a>

## Contributing

Please refer to the [OCTF contributing guide](https://github.com/Open-CAS/open-cas-telemetry-framework/blob/master/CONTRIBUTING.md).

<a id="related_projects"></a>

## Related Projects
Please explore related projects:
* [Open CAS Telemetry Framework](https://github.com/Open-CAS/open-cas-telemetry-framework) framework containing the building blocks for the development of a telemetry and monitoring environment
* [Open CAS Framework](https://github.com/Open-CAS/ocf) - high performance block
storage caching meta-library
* [Open CAS Linux](https://github.com/Open-CAS/open-cas-linux) - Linux block storage cache

<a id="notice"></a>

## Notice
[NOTICE](https://github.com/Open-CAS/standalone-linux-io-tracer/blob/master/NOTICE) contains more information
