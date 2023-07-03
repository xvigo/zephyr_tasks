# Zephyr samples
Custom Zephyr app samples extending the official [Zephyr samples](https://docs.zephyrproject.org/latest/samples/index.html).

## Installation
Please follow the official Zephyr [getting started guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

Once installed the `calc\` and `sync\` can be copied to the samples folder.
The samples can be executed from the zephyrproject/zephyr/ directory as described in Usage sections.

## calc
Command line integer calculator using UART. 
Processes given expression with two integer operands and prints result.
Supported operands are: +, -, *, /, %.

### Usage
```
$ west build -p always -b qemu_cortex_m3 samples/calc
$ west build -t run
```
The app can be stopped using `ctrl+a x`.

## sync_3p
A simple application that demonstrates basic sanity of the kernel.
Three threads take turns printing a greeting message to the console.

### Usage
```
$ west build -p always -b qemu_cortex_m3 samples/calc
$ west build -t run
```
The app can be stopped using `ctrl+a x`.
