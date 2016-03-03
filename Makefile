all:
	make -C ./libcyusb
	make -C ./libcontrol

install:
	make install -C ./libcyusb
	make install -C ./libcontrol

uninstall:
	make uninstall -C ./libcyusb
