#!/bin/sh

cd /system
if [ -f /system/.upgrade ]; then
    cd /backupa
    echo "init upgrading!!!!!!!!!!!!"
    ./upgrade.sh
    rm /system/.upgrade
fi
cd /system

insmod /driver/tx-isp.ko isp_clk=100000000
insmod /driver/exfat.ko
insmod /driver/sample_motor.ko
insmod /driver/audio.ko spk_gpio=-1 sign_mode=0
insmod /driver/sinfo.ko
insmod /driver/sample_pwm_core.ko
insmod /driver/sample_pwm_hal.ko
insmod /driver/rtl8189ftv.ko


echo 63 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio63/direction
echo 1 > /sys/class/gpio/gpio63/value

/system/bin/singleBoadTest
dropbear -r /system/etc/dropbear/dropbear_rsa_host_key &
devmem -a 0x10010124 -w 0x01000000 & # set MSK bit for GPIO 56
getjoystick -a 192.168.2.233 &
/system/bin/iCamera
