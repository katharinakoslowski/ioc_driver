# ioc_driver
Demo char device driver for input and output of text.

### Environment
Tested with:
- Linux Desktop: Ubuntu 22.04.4 LTS
- Kernel version: 6.5.0-26-generic
- Architecture: x86_64

### Usage
To automatically build and initialize the module, use `sudo bash init.sh`.

When manually starting the module, the following parameters can be set:
- `ioc_major=int`: major number (default: dynamic)
-  `ioc_dev_count=int`: number of devices (default: 4)
-  `ioc_timer=int`: log output timer (default: 1000ms)
-  `ioc_word_dlm=int`: if > 0, whitespace is used as a word delimiter; otherwise each char is a word (default: 1)

### Log file
To recreate the output in x, initialize the module with `init.sh` and use `sudo bash io_calls.sh` to write data to the device files.

### Bugs
- Module verification fails 
- Unable to restart module: exit function fails to properly clean up devices, restart of module fails due to device class already existing