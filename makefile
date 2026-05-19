.PHONY : all echo-client echo-server clean install uninstall android-install android-uninstall

all: echo-client echo-server

echo-client:
	cd tc; make; cd ..
	cp bin/tc bin/echo-client

echo-server:
	cd ts; make; cd ..
	cp bin/ts bin/echo-server

clean:
	cd tc; make clean; cd ..
	cd ts; make clean; cd ..
	rm -f bin/echo-client bin/echo-server

install: all
	sudo cp bin/echo-client /usr/local/sbin
	sudo cp bin/echo-server /usr/local/sbin

uninstall:
	sudo rm -f /usr/local/sbin/echo-client /usr/local/sbin/echo-server

android-install: all
	adb push bin/echo-client bin/echo-server /data/local/tmp
	adb exec-out "su -c 'mount -o rw,remount /'"
	adb exec-out "su -c 'cp /data/local/tmp/echo-client /data/local/tmp/echo-server /system/xbin'"
	adb exec-out "su -c 'chmod 755 /system/xbin/echo-client'"
	adb exec-out "su -c 'chmod 755 /system/xbin/echo-server'"
	adb exec-out "su -c 'mount -o ro,remount /'"
	adb exec-out "su -c 'rm /data/local/tmp/echo-client /data/local/tmp/echo-server'"

android-uninstall:
	adb exec-out "su -c 'mount -o rw,remount /'"
	adb exec-out "su -c 'rm -f /system/xbin/echo-client /system/xbin/echo-server'"
	adb exec-out "su -c 'mount -o ro,remount /'"