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
* [Documentation](#documentation)
* [Source Code](#source)
* [Deployment](#deployment)
* [Examples](#examples)
* [Contributing](#contributing)

<a id="os_support"></a>

## Supported OS

Right now the compilation of Standalone Linux IO Tracer is tested on the
following OSes:

|OS                            | Version           | Comment           
|------------------------------|-------------------|-------------------
|RHEL/CentOS                   | 7.6               |               
|Ubuntu                        | 18.04             | Experimental             
|Fedora                        | 30                | Experimental             

<a id="documentation"></a>

## Documentation

You can find the Markdown version of the man page for iotrace [here](https://github.com/Open-CAS/standalone-linux-io-tracer/blob/master/doc/man/MAN.md).
The man page is also installed during installation.

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

* To build and use Standalone Linux IO Tracer, setup [OCTF](https://github.com/Open-CAS/open-cas-telemetry-framework) in the following way, from this repository:

  ~~~{.sh}
  git submodule update --init --recursive
  sudo ./setup_dependencies.sh
  ~~~

  Installed dependencies include Google Protocol Buffers, CMake and Google Test.
  The dependencies are either installed with yum/apt or installed to a dedicated directory /opt/octf/ to avoid overwriting already installed ones.

  OCTF will then be installed along with iotrace in the next steps.

  > **NOTE:**  Alternatively OCTF can also be installed separately, using instructions found in it's [README](https://github.com/Open-CAS/open-cas-telemetry-framework/blob/master/README.md). This option however right now only allows building the cli binary, and not the kernel module


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

<a id="examples"></a>

## Examples

Make sure you removed old version of iotrace kernel module: modprobe -r iotrace
~~~{.sh}
modprobe -r iotrace
~~~

To allow tracing of block devices, tracing module needs to be loaded first:
~~~{.sh}
modprobe iotrace
~~~

* Start tracing two block devices for 1 hour, or until trace file is 1GiB:
  ~~~{.sh}
  sudo iotrace --start-trace --devices /dev/sda,/dev/sdb1 --time 3600 --size 1024
  ~~~

* List created traces:

  ~~~{.sh}
  iotrace --list-traces
  ~~~

  Output:

  ~~~{.sh}
  {
  "trace": [
    {
     "tracePath": "kernel/2019-05-10_15:24:21",
     "state": "COMPLETE"
    }
  ]
  }
  ~~~

* Parse traces (note usage of path returned in --list-traces):

  ~~~{.sh}
  iotrace --parse-trace --path "kernel/2019-05-10_15:24:21" --format json
  ~~~

  Output:

  ~~~{.sh}
  {"header":{"sid":"1","timestamp":"253654431680"},"deviceDescription":{"id":"1","name":"sda","size":62914560}}
  {"header":{"sid":"2","timestamp":"253654431680"},"deviceDescription":{"id":"2","name":"sdb","size":62914560}}
  {"header":{"sid":"3","timestamp":"254719353975"},"io":{"lba":"29664032","len":8,"ioClass":1,"deviceId":"1","operation":"Write","flush":false,"fua":false}}
  {"header":{"sid":"4","timestamp":"254719353975"},"io":{"lba":"29664032","len":8,"ioClass":1,"deviceId":"2","operation":"Write","flush":false,"fua":false}}
  ...
  ~~~


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
[NOTICE](https://github.com/Open-CAS/standalone-linux-io-tracer/blob/master/doc/NOTICE) contains more information
