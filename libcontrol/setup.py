from distutils.core import setup, Extension

cyusb = Extension('cyusb',
     	          sources=['cyusb.cpp'],
                  libraries=['cyusb'])

setup(name="cyusb",
      version='0.9',
      description='Cypress usb connection module',
      author='freyr',
      author_email='sky_rider_93@mail.ru',
      ext_modules=[cyusb])
