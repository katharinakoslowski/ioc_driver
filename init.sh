#!/bin/bash

function check_res {
  ret=$?
  if [[ $ret != 0 ]]; then
    echo "ERROR. End script"
    exit $ret
  fi
}

cd ./src
make clean
check_res
make
check_res
rmmod ioc_driver
insmod ioc_driver.ko
check_res
for i in {0..3}
do
	chmod 666 /dev/ioc_device$i
done
dmesg --follow
exit 0