#include "ctrl_connection.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define CTRL_RETURN_NULL(msg)do{printf(msg);cyusb_close();return NULL;}while(0)

CTRL_CONNECTION*
ctrl_connection_new()
{
  int desc;
  cyusb_handle* tmp_handle;
  
  desc = cyusb_open();
  if ( desc < 0 )
    CTRL_RETURN_NULL("Error opening library\n");
  if ( desc == 0 ) 
    CTRL_RETURN_NULL("No device found\n");
  if ( desc > 1 )
    CTRL_RETURN_NULL("More than 1 devices of interest found. Disconnect unwanted devices\n");
  
  tmp_handle = cyusb_gethandle(0);
  if ( cyusb_getvendor(tmp_handle) != 0x04b4 )
    CTRL_RETURN_NULL("Cypress chipset not detected\n");
  printf("VID=%04x,PID=%04x,BusNum=%02x,Addr=%d\n",
	 cyusb_getvendor(tmp_handle), cyusb_getproduct(tmp_handle),
	 cyusb_get_busnumber(tmp_handle), cyusb_get_devaddr(tmp_handle));

  desc = cyusb_kernel_driver_active(tmp_handle, 0);
  if ( desc != 0 )
    CTRL_RETURN_NULL("kernel driver active. Exitting\n");

  desc = cyusb_claim_interface(tmp_handle, 0);
  if ( desc != 0 )
    CTRL_RETURN_NULL("Error in claiming interface\n");

  CTRL_CONNECTION* rval = (CTRL_CONNECTION*)malloc(sizeof(CTRL_CONNECTION));
  rval->handle = tmp_handle;
  rval->in_point = IN_POINT;
  rval->out_point = OUT_POINT;
  rval->max_size_in = cyusb_get_max_iso_packet_size(rval->handle, rval->in_point);
  rval->max_size_out = cyusb_get_max_iso_packet_size(rval->handle, rval->out_point);

  return rval;
}

void
ctrl_connection_delete(CTRL_CONNECTION* this)
{
  cyusb_close();
  free(this);
}

int
ctrl_connection_send(CTRL_CONNECTION* this,
		     unsigned char* buf)
{
  int transferred = 0;
  int rval;
  if (buf == NULL) return -1;
  rval = cyusb_bulk_transfer(this->handle,
			     this->out_point,
			     buf,
			     this->max_size_out,
			     &transferred,
			     1000);
  if ( rval != 0 ) {
    cyusb_error(rval);
    return -1;
  }
  else
    return transferred;
}

int
ctrl_connection_recv(CTRL_CONNECTION* this,
		     unsigned char* buf)
{
  int transferred = 0;
  int rval;
  if (buf == NULL) return -1;
  rval = cyusb_bulk_transfer(this->handle,
			     this->in_point,
			     buf,
			     this->max_size_in,
			     &transferred,
			     1000);
  if ( rval != 0 ) {
    cyusb_error(rval);
    return -1;
  }
  else
    return transferred;
}


