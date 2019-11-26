## Running tests
Test controller (machine from which the tests are started) requirements:
  * python >= 3.6
  * python requirements from the file "modules/test-framework/requirements.txt"

Device Under Test (DUT) requirements:
  * packages specified in file [dut_requirements.txt](../tests/dut_requirements.txt)

To install the latter type:
```
pip3 install -r [requirements.txt](../modules/test-framework/requirements.txt)
```

To start tests call the following command from 'tests' directory:
```
python3 -m pytest --dut-config="path/to/config" --log-path="path/to/logs"
```

The command executes pytest module. All tests in the working directory are then
discovered and executed on the DUT specified in the dut config.

## The DUT config file

DUT config file is a yaml file which configures where the tests are run.
It can contain a list of drives available for testing.

'ip' field should be filled with valid IP string to use remote ssh executor
or it should be commented out when user want to execute tests on local machine

An example of DUT config can be found in [example_config.yml](tests/config/example_config.yml)
