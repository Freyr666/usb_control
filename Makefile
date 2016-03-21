VERSION = 0.1.9

all:
	make -C ./libcyusb
	make -C ./libcontrol

install:
	make install -C ./libcyusb
	make install -C ./libcontrol

uninstall:
	make uninstall -C ./libcyusb

tarboll:
	tar czvf /tmp/usb_control-$(VERSION).tar.gz ../usb_control/libcyusb ../usb_control/include ../usb_control/configs
	mv /tmp/usb_control-$(VERSION).tar.gz ./
