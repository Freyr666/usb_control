#include "../include/cyusb.h"

#define OUT_POINT 0x01
#define IN_POINT 0x81

typedef struct __ctrl_connection
{
  cyusb_handle* handle;
  unsigned int out_point;
  unsigned int in_point;
  int max_size_out;
  int max_size_in;
} CTRL_CONNECTION;

CTRL_CONNECTION* ctrl_connection_new();
void             ctrl_connection_delete(CTRL_CONNECTION* self);
int              ctrl_connection_send(CTRL_CONNECTION* self,
				      unsigned char* buf);
int              ctrl_connection_recv(CTRL_CONNECTION* self,
				      unsigned char* buf);
