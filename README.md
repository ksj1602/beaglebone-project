# Embedded Systems

This C code is designed to be run on an embedded system running a distribution based on Debian Linux.

It was tested on a BeagleBone Green Wireless.

It makes use of the [MRAA](https://iotdk.intel.com/docs/master/mraa/) library to get analog and digital input from sensors/buttons.

In this program, data from a temperature sensor is read at fixed intervals and then reported along with a timestamp to standard output. The default unit used is Fahrenheit.

It utilizes the following pin numbers:

```
Temperature Sensor: ANALOG PIN 0
Button: GPIO (Digital) PIN 73
```

These pin numbers can be changed by editing them in the program.

If a button is connected to the correct pin number, pressing the button terminates the program immediately.

The program also accepts the following commands while running:

```
SCALE=UNIT : Changes the unit of measurement to Fahrenheit or Celsius. UNIT must be one of F or C
PERIOD=LENGTH : LENGTH is a positive integer that defines (in seconds) the fixed interval at 
                which temperature data will be reported
STOP : Pauses the program
START : Resumes the program if it has been paused
OFF : Terminates the program. This is essentially the same as pressing the button.
```

The program supports the following command line arguments:

```
--period=LENGTH : As above, LENGTH is a positive integer for interval of measurement
--scale=UNIT : As above, can be used to set the unit to Fahrenheit or Celsius before 
               starting the program.
--log=FILE : This argument enables logging of data to FILE.
```

The program includes a "dummy" implementation of sensor functionality in case it is run on an arbitrary Linux system. In this, a constant temperature is reported every time. This option was included for testing purposes.

There are two more implementations: both of which enable sending the data to a server. One is implemented using standard TCP without any encryption and the other is a TLS implementation supporting encrypted data communication.

For these versions, the `log` and `host` command line options are mandatory, as is a port number. Check details on how to run the program below.

To run the program, first make sure that you have the required libraries installed.

Reminder: This program has been developed and tested on a Linux system running a debian based distribution. All the instructions below are mentioned in that context. It is unlikely that it runs on other platforms without potentially changing large parts of it. This issue arises from the fact that it was written to run on a specific embedded system and thus, portability was not a factor in its development.

Follow this [link](https://upm.mraa.io/Documentation/mraa.html) for instructions on how to install the MRAA library.

Then for `libcrypto` and `libssl` use the following commands:

```
$ sudo apt-get update -y
$ sudo apt-get install -y libssl-dev
```

Then clone this repository using:

```
$ git clone https://github.com/ksj1602/beaglebone-project.git
```
Once you have the files, you must specify which version of the program you want to run.

For the basic version without any server communication use:

```
$ make
$ ./temp-std [--scale=UNIT] [--period=LENGTH] [--log=FILE]
```

You may also use `$ make std` instead of just `$ make` above.

For the tcp version (no encryption) use:

```
$ make tcp
$ ./temp-tcp --log=FILE --host=SERVER PORT_NUMBER [--scale=UNIT] [--period=LENGTH]
```

The tls version is similar:

```
$ make tls
$ ./temp-tls --log=FILE --host=SERVER PORT_NUMBER [--scale=UNIT] [--period=LENGTH]
```

To clean up the directory by removing all executables, run:

```
$ make clean
```

Note: This project was made as part of Computer Science coursework at UCLA (CS 111: Operating Systems Principles) taken Summer 2020. Please do not copy any part of this for any future offerings. It has been uploaded here to showcase the project. I am not responsible in any way if you are caught for cheating.
