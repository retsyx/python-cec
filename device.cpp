/* device.cpp
 *
 * Copyright (C) 2013 Austin Hendrix <namniart@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Implementation of CEC Device class for Python
 *
 * Author: Austin Hendrix <namniart@gmail.com>
 */

// request the std format macros
#define __STDC_FORMAT_MACROS

#include "cec.h"
#include "adapter.h"
#include "device.h"

#include <inttypes.h>

using namespace CEC;

static PyObject * Device_getAdapter(Device * self, void * closure) {
   Py_INCREF(self->adapter);
   return (PyObject *)self->adapter;
}

static PyObject * Device_getAddr(Device * self, void * closure) {
   return Py_BuildValue("b", self->addr);
}

static PyObject * Device_getPhysicalAddress(Device * self, void * closure) {
   Py_INCREF(self->physicalAddress);
   return self->physicalAddress;
}

static PyObject * Device_getVendor(Device * self, void * closure) {
   Py_INCREF(self->vendorId);
   return self->vendorId;
}

static PyObject * Device_getOsdString(Device * self, void * closure) {
   Py_INCREF(self->osdName);
   return self->osdName;
}

static PyObject * Device_getCECVersion(Device * self,
      void * closure) {
   Py_INCREF(self->cecVersion);
   return self->cecVersion;
}

static PyObject * Device_getLanguage(Device * self, void * closure) {
   Py_INCREF(self->lang);
   return self->lang;
}

#pragma GCC diagnostic ignored "-Wwrite-strings"
static PyGetSetDef Device_getset[] = {
   {"adapter", (getter)Device_getAdapter, (setter)NULL, "CEC Adapter"},
   {"address", (getter)Device_getAddr, (setter)NULL, "Logical Address"},
   {"physical_address", (getter)Device_getPhysicalAddress, (setter)NULL, "Physical Addresss"},
   {"vendor", (getter)Device_getVendor, (setter)NULL, "Vendor ID"},
   {"osd_string", (getter)Device_getOsdString, (setter)NULL, "OSD String"},
   {"cec_version", (getter)Device_getCECVersion, (setter)NULL, "CEC Version"},
   {"language", (getter)Device_getLanguage, (setter)NULL, "Language"},
   {NULL}
};

static PyObject * Device_is_on(Device * self) {
   cec_power_status power;
   Py_BEGIN_ALLOW_THREADS
   power = self->adapter->adapter->GetDevicePowerStatus(self->addr);
   Py_END_ALLOW_THREADS
   PyObject * ret;
   switch(power) {
      case CEC_POWER_STATUS_ON:
      case CEC_POWER_STATUS_IN_TRANSITION_ON_TO_STANDBY:
         ret = Py_True;
         break;
      case CEC_POWER_STATUS_STANDBY:
      case CEC_POWER_STATUS_IN_TRANSITION_STANDBY_TO_ON:
         ret = Py_False;
         break;
      case CEC_POWER_STATUS_UNKNOWN:
      default:
         PyErr_SetString(PyExc_IOError, "Power status not found");
         return NULL;
   }
   Py_INCREF(ret);
   return ret;
}

static PyObject * Device_power_on(Device * self) {
   bool success;
   Py_BEGIN_ALLOW_THREADS
   success = self->adapter->adapter->PowerOnDevices(self->addr);
   Py_END_ALLOW_THREADS
   if( success ) {
      Py_RETURN_TRUE;
   } else {
      Py_RETURN_FALSE;
   }
}

static PyObject * Device_standby(Device * self) {
   bool success;
   Py_BEGIN_ALLOW_THREADS
   success = self->adapter->adapter->StandbyDevices(self->addr);
   Py_END_ALLOW_THREADS
   if( success ) {
      Py_RETURN_TRUE;
   } else {
      Py_RETURN_FALSE;
   }
}

static PyObject * Device_is_active(Device * self) {
   bool success;
   Py_BEGIN_ALLOW_THREADS
   success = self->adapter->adapter->IsActiveSource(self->addr);
   Py_END_ALLOW_THREADS
   if( success ) {
      Py_RETURN_TRUE;
   } else {
      Py_RETURN_FALSE;
   }
}

static PyObject * Device_av_input(Device * self, PyObject * args) {
   unsigned char input;
   if( PyArg_ParseTuple(args, "b:set_av_input", &input) ) {
      cec_command data;
      bool success;
      Py_BEGIN_ALLOW_THREADS
      data.initiator = self->adapter->adapter->GetLogicalAddresses().primary;
      data.destination = self->addr;
      data.opcode = CEC_OPCODE_USER_CONTROL_PRESSED;
      data.opcode_set = 1;
      data.PushBack(0x69);
      data.PushBack(input);
      success = self->adapter->adapter->Transmit(data);
      Py_END_ALLOW_THREADS
      if( success ) {
         Py_RETURN_TRUE;
      } else {
         Py_RETURN_FALSE;
      }
   } else {
      return NULL;
   }
}

static PyObject * Device_audio_input(Device * self, PyObject * args) {
   unsigned char input;
   if( PyArg_ParseTuple(args, "b:set_audio_input", &input) ) {
      cec_command data;
      bool success;
      Py_BEGIN_ALLOW_THREADS
      data.initiator = self->adapter->adapter->GetLogicalAddresses().primary;
      data.destination = self->addr;
      data.opcode = CEC_OPCODE_USER_CONTROL_PRESSED;
      data.opcode_set = 1;
      data.PushBack(0x6a);
      data.PushBack(input);
      success = self->adapter->adapter->Transmit(data);
      Py_END_ALLOW_THREADS
      if( success ) {
         Py_RETURN_TRUE;
      } else {
         Py_RETURN_FALSE;
      }
   } else {
      return NULL;
   }
}

static PyObject * Device_transmit(Device * self, PyObject * args) {
   unsigned char opcode;
   const char * params = NULL;
   Py_ssize_t param_count = 0;
   if( PyArg_ParseTuple(args, "b|s#:transmit", &opcode,
         &params, &param_count) ) {
      if( param_count > CEC_MAX_DATA_PACKET_SIZE ) {
         char errstr[1024];
         snprintf(errstr, 1024, "Too many parameters, maximum is %d",
            CEC_MAX_DATA_PACKET_SIZE);
         PyErr_SetString(PyExc_ValueError, errstr);
         return NULL;
      }
      cec_command data;
      bool success;
      Py_BEGIN_ALLOW_THREADS
      data.initiator = self->adapter->adapter->GetLogicalAddresses().primary;
      data.destination = self->addr;
      data.opcode = (cec_opcode)opcode;
      data.opcode_set = 1;
      if( params ) {
         for( Py_ssize_t i=0; i<param_count; i++ ) {
            data.PushBack(((uint8_t *)params)[i]);
         }
      }
      success = self->adapter->adapter->Transmit(data);
      Py_END_ALLOW_THREADS
      if( success ) {
         Py_RETURN_TRUE;
      } else {
         Py_RETURN_FALSE;
      }
   } else {
      return NULL;
   }
}

static PyObject * Device_new(PyTypeObject * type, PyObject * args, PyObject * kwds) {
   Device * self;
   Adapter * adapter;
   unsigned char addr;

#if CEC_LIB_VERSION_MAJOR >= 4
   std::string name;
   std::string lang;
#else
   cec_osd_name name;
   cec_menu_language lang;
#endif

   if( !PyArg_ParseTuple(args, "Ob:Device new", &adapter, &addr) ) {
      return NULL;
   }
   if (!PyObject_IsInstance((PyObject *)adapter, (PyObject *)AdapterType())) {
      PyErr_SetString(PyExc_ValueError, "Adapter parameter is not an instance of cec.Adapter");
      return NULL;
   }
   if( addr < 0 ) {
      PyErr_SetString(PyExc_ValueError, "Logical address should be >= 0");
      return NULL;
   }
   if( addr > 15 ) {
      PyErr_SetString(PyExc_ValueError, "Logical address should be < 16");
      return NULL;
   }

   self = (Device *)type->tp_alloc(type, 0);
   if (!self) {
      return NULL;
   }

   Py_INCREF(adapter);
   self->adapter = adapter;
   self->addr = (cec_logical_address)addr;
   uint64_t vendor;
   Py_BEGIN_ALLOW_THREADS
   vendor = adapter->adapter->GetDeviceVendorId(self->addr);
   Py_END_ALLOW_THREADS
   char vendor_str[7];
   snprintf(vendor_str, 7, "%06" PRIX64, vendor);
   vendor_str[6] = '\0';
   if (!(self->vendorId = Py_BuildValue("s", vendor_str))) {
      goto fail;
   }

   char strAddr[8];
   Py_BEGIN_ALLOW_THREADS
   uint16_t physicalAddress = adapter->adapter->GetDevicePhysicalAddress(self->addr);
   snprintf(strAddr, 8, "%x.%x.%x.%x",
         (physicalAddress >> 12) & 0xF,
         (physicalAddress >> 8) & 0xF,
         (physicalAddress >> 4) & 0xF,
         physicalAddress & 0xF);
   Py_END_ALLOW_THREADS
   self->physicalAddress = Py_BuildValue("s", strAddr);

   const char * ver_str;
   Py_BEGIN_ALLOW_THREADS
   cec_version ver = adapter->adapter->GetDeviceCecVersion(self->addr);
   switch(ver) {
      case CEC_VERSION_1_2:
         ver_str = "1.2";
         break;
      case CEC_VERSION_1_2A:
         ver_str = "1.2a";
         break;
      case CEC_VERSION_1_3:
         ver_str = "1.3";
         break;
      case CEC_VERSION_1_3A:
         ver_str = "1.3a";
         break;
      case CEC_VERSION_1_4:
         ver_str = "1.4";
         break;
      case CEC_VERSION_UNKNOWN:
      default:
         ver_str = "Unknown";
         break;
   }
   Py_END_ALLOW_THREADS

   if (!(self->cecVersion = Py_BuildValue("s", ver_str))) {
      goto fail;
   }

#if CEC_LIB_VERSION_MAJOR >= 4
   Py_BEGIN_ALLOW_THREADS
   name = adapter->adapter->GetDeviceOSDName(self->addr);
   Py_END_ALLOW_THREADS
   if( !(self->osdName = Py_BuildValue("s#", name.c_str(), name.length())) ) {
      goto fail;
   }
#else
   Py_BEGIN_ALLOW_THREADS
   name = adapter->adapter->GetDeviceOSDName(self->addr);
   Py_END_ALLOW_THREADS
   if( !(self->osdName = Py_BuildValue("s", name.name)) ) {
      goto fail;
   }
#endif

#if CEC_LIB_VERSION_MAJOR >= 4
   Py_BEGIN_ALLOW_THREADS
   lang = adapter->adapter->GetDeviceMenuLanguage(self->addr);
   Py_END_ALLOW_THREADS
   if( !(self->lang = Py_BuildValue("s#", lang.c_str(), lang.length())) ) {
      goto fail;
   }
#else
   Py_BEGIN_ALLOW_THREADS
   adapter->adapter->GetDeviceMenuLanguage(self->addr, &lang);
   Py_END_ALLOW_THREADS
   if( !(self->lang = Py_BuildValue("s", lang.language)) ) {
      goto fail;
   }
#endif

   return (PyObject *)self;

fail:
   Py_DECREF(adapter);
   type->tp_free((PyObject *)self);
   return NULL;

}

static void Device_dealloc(Device * self) {
   Py_DECREF(self->adapter);
   Py_DECREF(self->vendorId);
   Py_DECREF(self->physicalAddress);
   Py_DECREF(self->cecVersion);
   Py_DECREF(self->osdName);
   Py_DECREF(self->lang);
   Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * Device_str(Device * self) {
   char addr[16];
   snprintf(addr, 16, "CEC Device %d", self->addr);
   return Py_BuildValue("s", addr);
}

static PyObject * Device_repr(Device * self) {
   char addr[16];
   snprintf(addr, 16, "Device(%d)", self->addr);
   return Py_BuildValue("s", addr);
}

static PyMethodDef Device_methods[] = {
   {"is_on", (PyCFunction)Device_is_on, METH_NOARGS,
      "Get device power status"},
   {"power_on", (PyCFunction)Device_power_on, METH_NOARGS,
      "Power on this device"},
   {"standby", (PyCFunction)Device_standby, METH_NOARGS,
      "Put this device into standby"},
   {"is_active", (PyCFunction)Device_is_active, METH_VARARGS,
      "Check if this device is the active source on the bus"},
   {"set_av_input", (PyCFunction)Device_av_input, METH_VARARGS,
      "Select AV Input"},
   {"set_audio_input", (PyCFunction)Device_audio_input, METH_VARARGS,
      "Select Audio Input"},
   {"transmit", (PyCFunction)Device_transmit, METH_VARARGS,
      "Transmit a raw CEC command to this device"},
   {NULL}
};

static PyTypeObject _DeviceType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "cec.Device",              /*tp_name*/
   sizeof(Device),            /*tp_basicsize*/
   0,                         /*tp_itemsize*/
   (destructor)Device_dealloc, /*tp_dealloc*/
   0,                         /*tp_print*/
   0,                         /*tp_getattr*/
   0,                         /*tp_setattr*/
   0,                         /*tp_compare*/
   (reprfunc)Device_repr,     /*tp_repr*/
   0,                         /*tp_as_number*/
   0,                         /*tp_as_sequence*/
   0,                         /*tp_as_mapping*/
   0,                         /*tp_hash */
   0,                         /*tp_call*/
   (reprfunc)Device_str,      /*tp_str*/
   0,                         /*tp_getattro*/
   0,                         /*tp_setattro*/
   0,                         /*tp_as_buffer*/
   Py_TPFLAGS_DEFAULT,        /*tp_flags*/
   "CEC Device objects",      /* tp_doc */
};

PyTypeObject * DeviceTypeInit() {
   _DeviceType.tp_new = Device_new;
   _DeviceType.tp_methods = Device_methods;
   _DeviceType.tp_getset = Device_getset;
   return &_DeviceType;
}

PyTypeObject * DeviceType() {
   return &_DeviceType;
}
