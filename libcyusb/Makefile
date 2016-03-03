all:
	g++ -fPIC -o libcyusb.o -c libcyusb.c
	g++ -shared -Wl,-soname,libcyusb.so -o libcyusb.so.1 libcyusb.o -l usb-1.0 -l rt
	rm -f libcyusb.o
clean:
	rm -f libcyusb.so.1
install:
	cp ./libcyusb.so.1 /usr/lib64/libcyusb.so.1
	cp ../include/cyusb.h /usr/include/cyusb.h
	ln -s /usr/lib64/libcyusb.so.1 /usr/lib64/libcyusb.so
	cp ../configs/cyusb.conf /etc/cyusb.conf
	cp ../configs/88-cyusb.rules /etc/udev/rules.d/88-cyusb.rules
	cp ../configs/cy_renumerate.sh /usr/local/bin/cy_renumerate.sh

uninstall:
	rm /usr/include/cyusb.h
	rm /usr/lib64/libcyusb.so
	rm /usr/lib64/libcyusb.so.1
	rm /etc/cyusb.conf
	rm /etc/udev/rules.d/88-cyusb.rules
	rm /usr/local/bin/cy_renumerate.sh
