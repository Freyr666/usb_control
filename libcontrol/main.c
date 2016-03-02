#include <stdio.h>
#include <string.h>

#include "../include/cyusb.h"

int main(int argc, char *argv[])
{
  cyusb_handle* handle;
  int desc, rval;
  unsigned char buf[64];
  int transferred = 0;
  int packet_size_out;
  
  desc = cyusb_open();
  if ( desc < 0 ) {
    printf("Error opening library\n");
    return -1;
  }
  else if ( desc == 0 ) {
    printf("No device found\n");
    return 0;
  }
  if ( desc > 1 ) {
    printf("More than 1 devices of interest found. Disconnect unwanted devices\n");
    return 0;
  }
  handle = cyusb_gethandle(0);
  if ( cyusb_getvendor(handle) != 0x04b4 ) {
    printf("Cypress chipset not detected\n");
    cyusb_close();
    return 0;
  }
  printf("VID=%04x,PID=%04x,BusNum=%02x,Addr=%d\n",
	 cyusb_getvendor(handle), cyusb_getproduct(handle),
	 cyusb_get_busnumber(handle), cyusb_get_devaddr(handle));
  desc = cyusb_kernel_driver_active(handle, 0);
  if ( desc != 0 ) {
    printf("kernel driver active. Exitting\n");
    cyusb_close();
    return 0;
  }
  
  desc = cyusb_claim_interface(handle, 0);
  if ( desc != 0 ) {
    printf("Error in claiming interface\n");
    cyusb_close();
    return 0;
  }
  /* sending data */
  packet_size_out = cyusb_get_max_iso_packet_size(handle, 0x01);
  printf("Max size: %d\n", packet_size_out);
  rval = cyusb_bulk_transfer(handle, 0x01, buf, 64, &transferred, 1000);
  printf("Bytes sent to device = %d\n",transferred);
  if ( rval == 0 ) {
    printf("%s", buf);
  }
  else {
    cyusb_error(rval);
  }
  packet_size_out = cyusb_get_max_iso_packet_size(handle, 0x81);
  printf("Max size: %d\n", packet_size_out);
  rval = cyusb_bulk_transfer(handle, 0x81, buf, 64, &transferred, 1000);
  printf("Bytes received from device = %d\n",transferred);
  if ( rval == 0 ) {
    printf("%s", buf);
  }
  else {
    cyusb_error(rval);
  }
  cyusb_close();
  
  return 0;
}
