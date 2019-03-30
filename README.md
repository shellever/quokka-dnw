## Description

Current Supported: jz2440v2 board


## Installation

```
1. install depend
$ sudo apt-get install libusb-dev

2. build source
$ make

3. configure udev rules
//$ sudo cp -fp 51-dnw.rules /etc/udev/rules.d/
//$ sudo service udev restart
$ sudo make install

4. unplug usb and plug it in again

5. run dnw2
$ ./dnw2 -f u-boot.bin
```


## Reference

[Ubuntu: udev rules for USB scanner](http://pigeonsnest.co.uk/stuff/ubuntu-udev-scanner.html)

