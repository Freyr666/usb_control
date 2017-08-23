//#include "ctrl_connection.hpp"
#include <stdio.h>
#include <Python.h>
#include <structmember.h>
#include <malloc.h>
#include "../include/cyusb.h"

#define OUT_POINT 0x02
#define IN_POINT 0x86

#define MAX_BUF 8192

#define CTRL_RETURN_NULL(msg)do{printf(msg);return -1;}while(0)

typedef struct {
  PyObject_HEAD
  cyusb_handle* handle;
  unsigned int out_point;
  unsigned int in_point;
  int max_size_out;
  int max_size_in;
} Connection;

static void
Connection_dealloc(Connection* self)
{
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
Connection_new(PyTypeObject* type,
		  PyObject* args,
		  PyObject* kwds)
{
  Connection* self;
  self = (Connection*)type->tp_alloc(type, 0);
  return (PyObject*)self;
}

static int
Connection_init(Connection* self,
		   PyObject* args,
		   PyObject* kwds)
{
  int desc;
  cyusb_handle* tmp_handle = NULL;
  
  printf("Getting device handle\n");
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

  self->in_point = IN_POINT;
  self->out_point = OUT_POINT;
  self->max_size_in = cyusb_get_max_iso_packet_size(tmp_handle, self->in_point);
  self->max_size_out = cyusb_get_max_iso_packet_size(tmp_handle, self->out_point);
  self->handle = tmp_handle;
  return 0;
}

static PyMemberDef Connection_members[] = {
  {(char*)"max_size_in", T_INT, offsetof(Connection, max_size_in), 0,
   (char*)"max_size_in"},
  {(char*)"max_size_out", T_INT, offsetof(Connection, max_size_out), 0,
   (char*)"max_size_out"},
  {NULL}  /* Sentinel */
};

static PyObject*
Connection_send(Connection* self,
		   PyObject* arg)
{
  size_t len = 0;
  size_t message = 0;
  int passed = 0;
  int transd = 0;
  int rval;
  PyBytesObject* bytes;
  char* buf = NULL;
  if (! PyArg_ParseTuple(arg, "S", &bytes))
    return Py_None;
  len = PyBytes_GET_SIZE(bytes);
  buf = PyBytes_AsString((PyObject*)bytes);
 
  message = len; 
  while (message != 0) {
    size_t tmp_size;
    if (message > (unsigned)self->max_size_out) {
      tmp_size = self->max_size_out;
      message -= self->max_size_out;
    }
    else {
      tmp_size = message;
      message = 0;
    }
    rval = cyusb_bulk_transfer(self->handle,
			       self->out_point,
			       (u_char*)(buf+(passed*self->max_size_out)),
			       tmp_size,
			       &transd,
			       500);
    if ( rval != 0 ) {
      cyusb_error(rval);
    }
    passed++;
  }
  return Py_None;
}

static PyObject*
Connection_recv(Connection* self)
{
  int transd = 1;
  size_t size = 0;
  int rval;
  PyObject* bytes = NULL;
  char* buf = (char*)calloc(MAX_BUF, 1);
  while ((transd != 0) && (size < MAX_BUF)) {
    rval = cyusb_bulk_transfer(self->handle,
			       self->in_point,
			       (unsigned char*)(buf+size),
			       self->max_size_in,
			       &transd,
			       500);
    if ( rval != 0 ) {
      cyusb_error(rval);
    }
    size += transd;
  }
  bytes = PyBytes_FromStringAndSize(buf, size);
  free(buf);
  return bytes;
}

static PyMethodDef Connection_methods[] = {
    {"send", (PyCFunction)Connection_send, METH_VARARGS,
     "Send data"},
    {"recv", (PyCFunction)Connection_recv, METH_NOARGS,
     "Receive data"},
    {NULL}  /* Sentinel */
};

static PyTypeObject ConnectionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "cyusb.Connection",             /* tp_name */
    sizeof(Connection),             /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)Connection_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
    Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "Connection objects",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Connection_methods,             /* tp_methods */
    Connection_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Connection_init,      /* tp_init */
    0,                         /* tp_alloc */
    Connection_new,                 /* tp_new */
};

static PyObject*
cyusb_init(PyObject* self,
		PyObject* args)
{
  int desc;
  desc = cyusb_open();
  if ( desc < 0 ) {
    printf("Error opening library\n");
    cyusb_close();
    return Py_False;
  }
  if ( desc == 0 ) {
    printf("No device found\n");
    cyusb_close();
    return Py_False;
  }
  if ( desc > 1 ) {
    printf("More than 1 devices of interest found. Disconnect unwanted devices\n");
    cyusb_close();
    return Py_False;
  }
  return Py_True;
}

static PyObject*
cyusb_close(PyObject* self,
		 PyObject* args)
{
  printf("Cyusb closed\n");
  cyusb_close();
  return Py_None;
}


static PyMethodDef cyusb_methods[] =
  {
    {"init", cyusb_init, METH_NOARGS, "init-ing connections"},
    {"close", cyusb_close, METH_NOARGS, "closing connections"},
    {NULL, NULL, 0, NULL}
  };


static PyModuleDef cyusb_module = {
    PyModuleDef_HEAD_INIT,
    "cyusb",
    "cyusb connection module",
    -1,
    cyusb_methods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_cyusb(void)
{
    PyObject* m;

    if (PyType_Ready(&ConnectionType) < 0)
        return NULL;

    m = PyModule_Create(&cyusb_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&ConnectionType);
    PyModule_AddObject(m, "Connection", (PyObject *)&ConnectionType);
    return m;
}
