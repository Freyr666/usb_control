all:
	g++ -fPIC -o libcyusb.o -c libcyusb.c
	g++ -shared -Wl,-soname,libcyusb.so -o libcyusb.so.1 libcyusb.o -l usb-1.0 -l rt
	ln -sf libcyusb.so.1 libcyusb.so
	rm -f libcyusb.o
clean:
	rm -f libcyusb.so libcyusb.so.1
