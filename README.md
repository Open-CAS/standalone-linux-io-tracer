# Standalone Linux IO Tracer

Standalone Linux IO Tracer (iotrace) is a tool for block device I/O tracing
and management of created traces

For each I/O to target device(s) basic metadata information is captured
(IO operation type, address, size), supplemented with extended
classification. Extended classification contains information about I/O type
(direct / filesystem metadata / file) and target file attributes(e.g. file
size).

iotrace is based on [Open CAS Telemetry Framework (OCTF)](https://github.com/Open-CAS/open-cas-telemetry-framework). Collected traces are stored in OCTF trace
location. Traces can later be converted to JSON or CSV format.

iotrace consists of a kernel tracing module (iotrace.ko) and an executable
(iotrace) with command line interface.

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

|OS                            | Version           | Comment
|------------------------------|-------------------|-------------------
|RHEL/CentOS                   | 7.7               |
|RHEL/CentOS                   | 8.1               |
|Ubuntu                        | 18.04             |
|Fedora                        | 31                |

<a id="source"></a>

## Source Code

Source code is available in the official Standalone Linux IO Tracer GitHub repository:

~~~{.sh}
git clone https://github.com/open-cas/standalone-linux-io-tracer
cd standalone-linux-io-tracer
~~~

<a id="deployment"></a>

## Deployment

### Checkout
To get stable version of iotrace checkout latest release:

~~~{.sh}
git clone https://github.com/Open-CAS/standalone-linux-io-tracer/
cd standalone-linux-io-tracer
git checkout $(git tag | grep "^v[[:digit:]]*.[[:digit:]]*.[[:digit:]]*$" | tail -1)
~~~

But if you are going to develop iotrace, it is ok to checkout master branch.

### Prerequisites

* To build and use Standalone Linux IO Tracer, setup prerequisites first in the following way:

  ~~~{.sh}
  git submodule update --init --recursive
  sudo ./setup_dependencies.sh
  ~~~

  Installed dependencies include [OCTF](https://github.com/Open-CAS/open-cas-telemetry-framework),
  Google Protocol Buffers, CMake and Google Test. The dependencies are either installed with yum/apt
  or installed to a dedicated directory /opt/octf/ to avoid overwriting already installed ones.

### Build

Both the executable and the kernel module (and OCTF if submodule is present) are built with:
~~~{.sh}
make
~~~

### Installation

Both the executable and the kernel module (and OCTF if submodule is present) are installed with:
~~~{.sh}
sudo make install
~~~

<a id="theory_of_operation"></a>

## Theory of operation

Standalone Linux IO Tracer captures request data by registering to multiple trace points surfaced
by the Linux kernel (e.g. BIO queueing, BIO splitting, BIO completion). This allows for gathering of
IO metadata at the request level and passing it between kernel and userspace.

A circular buffer is allocated and shared between kernel and userspace for each logical CPU core and
trace events (example shown below) are then pushed into it.

```c
struct iotrace_event_device_desc {
    /** Event header */
    struct iotrace_event_hdr hdr;

    /** Device Id */
    uint64_t id;

    /** Device size in sectors */
    uint64_t device_size;

    /** Canonical device name */
    char device_name[32];

    /** Device model */
    char device_model[256];
} __attribute__((packed, aligned(8)));
```

The userspace part of the Standalone Linux IO Tracer reads the entries from the circular buffer and
translates them into Google Protocol Buffer format (see example below), for easier portability. The
data is then serialized in trace files in a per CPU basis (e.g. octf.trace.0).


```protobuf
message EventDeviceDescription {
    /** Device Id */
    uint64 id = 1;

    /** Device Name */
    string name = 2;

    /** Device size in sectors */
    uint64 size = 3;

    /** Device Model */
    string model = 4;
}
```

After tracing is complete these singular trace events may be parsed, combined and translated into
different Google Protocol Buffer messages (or other formats, such as CSV) when executing Standalone 
Linux IO Tracer trace parser commands.

For example the **--trace-parser --io** command analyzes multiple submission, split and completion
events to give a more complete view of a given IO request such as: its latency, queue depth, file 
size and path (if applicable) etc.

<a id="examples"></a>

## Examples

* Start tracing two block devices for 1 hour, or until trace file is 1GiB:
  ~~~{.sh}
  sudo iotrace --start-tracing --devices /dev/sda,/dev/sdb1 --time 3600 --size 1024
  ~~~

  > **NOTE:**  To allow tracing of block devices, Linux kernel tracing
  module needs to be loaded first. It is done automatically. After
  collecting traces the module will be unloaded.

* The below output example is based on sample traces found [here](https://github.com/Open-CAS/standalone-linux-io-tracer/blob/master/doc/resources/sample_trace.tar.xz)

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
     "label": "ycsb;a;rocksdb;uniform;xfs;381MiB;16000000"
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
