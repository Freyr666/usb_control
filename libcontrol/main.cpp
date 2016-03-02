#include "ctrl_connection.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
  int rval;
  unsigned char buf[64];
  int packet_size_out;
  
  CTRL_CONNECTION* conn = ctrl_connection_new();
  /* sending data */
  packet_size_out = conn->max_size_out;
  printf("Max size: %d\n", packet_size_out);
  rval = ctrl_connection_send(conn, buf);
  printf("Bytes sent to device = %d\n", rval);
  /* recv-ing data */
  packet_size_out = conn->max_size_in;
  printf("Max size: %d\n", packet_size_out);
  rval = ctrl_connection_send(conn, buf);
  printf("Bytes received from device = %d\n", rval);

  ctrl_connection_delete(conn);
  return 0;
}
