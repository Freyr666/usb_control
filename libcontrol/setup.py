from distutils.core import setup, Extension

cyusb = Extension('cyusb',
     	          sources=['cyusb.cpp'],
                  libraries=['cyusb'])

setup(ext_modules=[cyusb])
