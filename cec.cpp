/* cec.cpp
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
 * This file provides the python cec module, which is a python wrapper
 *  around libcec
 *
 * Author: Austin Hendrix <namniart@gmail.com>
 */

// request the std format macros
#define __STDC_FORMAT_MACROS

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libcec/cec.h>
#include <algorithm>

#include "cec.h"
#include "adapter.h"
#include "device.h"

using namespace CEC;

// Basic design/usage:
//
// ( cec.add_callback(event, handler) )
//    ???
//
// ( cec.remove_callback(event, handler) )
//    ???
//
// ( cec.transmit( ??? ) )
//    call ICECAdapter::Transmit()
//
// Events:
//    - log message
//       message.message[1024]
//       message.level (bitfield)
//       message.time (int64)
//    - key press
//       key.keycode (int/enum)
//       key.duration (int)
//    - command
//       command.initiator (logical address)
//       command.destination (logical address)
//       command.ack
//       command.eom
//       command.opcode (enum)
//       command.parameters
//       command.opcode_set (flag for when opcode is set; should probably use
//          this to set opcode to None)
//       command.transmit_timeout
//    - cec configuration changed (adapter/library config change)
//    - alert
//    - menu state changed
//       this is potentially thorny; see libcec source and CEC spec
//       ???
//    - source activated
//

int parse_physical_addr(const char * addr) {
   int a, b, c, d;
   if( sscanf(addr, "%x.%x.%x.%x", &a, &b, &c, &d) == 4 ) {
      if( a > 0xF || b > 0xF || c > 0xF || d > 0xF ) return -1;
      if( a < 0 || b < 0 || c < 0 || d < 0 ) return -1;
      return (a << 12) | (b << 8) | (c << 4) | d;
   } else {
      return -1;
   }
}

void parse_test() {
   assert(parse_physical_addr("0.0.0.0") == 0);
   assert(parse_physical_addr("F.0.0.0") == 0xF000);
   assert(parse_physical_addr("0.F.0.0") == 0x0F00);
   assert(parse_physical_addr("0.0.F.0") == 0x00F0);
   assert(parse_physical_addr("0.0.0.F") == 0x000F);
   assert(parse_physical_addr("-1.0.0.0") == -1);
   assert(parse_physical_addr("0.-1.0.0") == -1);
   assert(parse_physical_addr("0.0.-1.0") == -1);
   assert(parse_physical_addr("0.0.0.-1") == -1);
   assert(parse_physical_addr("foo") == -1);
   assert(parse_physical_addr("F.F.F.F") == 0xFFFF);
   assert(parse_physical_addr("f.f.f.f") == 0xFFFF);
}

static PyObject * list_adapters(PyObject * self, PyObject * args) {
   PyObject * result = NULL;

   if( PyArg_ParseTuple(args, ":list_adapters") ) {

      libcec_configuration config;
      ICECAdapter * adapter;

      config.Clear();
      config.deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);

      Py_BEGIN_ALLOW_THREADS
      adapter = CECInitialise(&config);
      Py_END_ALLOW_THREADS

      if (!adapter) {
        PyErr_SetString(PyExc_IOError, "Failed to initialize adapter");
        return NULL;
      }

      std::list<CEC_ADAPTER_TYPE> dev_list = get_adapters(adapter);

      CECDestroy(adapter);

      // set up our result list
      result = PyList_New(0);

      // populate our result list
      std::list<CEC_ADAPTER_TYPE>::const_iterator itr;
      for( itr = dev_list.begin(); itr != dev_list.end(); itr++ ) {
#if HAVE_CEC_ADAPTER_DESCRIPTOR
         PyList_Append(result, Py_BuildValue("s", itr->strComName));
         /* Convert all of the fields
         PyList_Append(result, Py_BuildValue("sshhhhii",
                  itr->strComName,
                  itr->strComPath,
                  itr->iVendorId,
                  itr->iProductId,
                  itr->iFirmwareVersion,
                  itr->iPhysicalAddress,
                  itr->iFirmwareBuildDate,
                  itr->adapterType
                  ));
                  */
#else
         PyList_Append(result, Py_BuildValue("s", itr->comm));
         /* Convert all of the fields
         PyList_Append(result, Py_BuildValue("sshhhhii",
                  itr->comm,
                  itr->strComPath,
                  itr->iVendorId,
                  itr->iProductId,
                  itr->iFirmwareVersion,
                  itr->iPhysicalAddress,
                  itr->iFirmwareBuildDate,
                  itr->adapterType
                  ));
                  */
#endif
      }
   }

   return result;
}

static PyMethodDef CecMethods[] = {
   {"list_adapters", list_adapters, METH_VARARGS, "List available adapters"},
   {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
   PyModuleDef_HEAD_INIT,
   "cec",
   NULL,
   -1,
   CecMethods,
   NULL,
   NULL,
   NULL,
   NULL
};
#endif

#if PY_MAJOR_VERSION >= 3
#define INITERROR return NULL
#else
#define INITERROR return
#endif

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit_cec(void) {
#else
PyMODINIT_FUNC initcec(void) {
#endif
   // Make sure threads are enabled in the python interpreter
   // this also acquires the global interpreter lock
   PyEval_InitThreads();

   // set up python module
   PyTypeObject * adapter = AdapterTypeInit();
   if (PyType_Ready(adapter) < 0) INITERROR;
   PyTypeObject * dev = DeviceTypeInit();
   if (PyType_Ready(dev) < 0) INITERROR;

#if PY_MAJOR_VERSION >= 3
   PyObject * m = PyModule_Create(&moduledef);
#else
   PyObject * m = Py_InitModule("cec", CecMethods);
#endif

   if( m == NULL ) INITERROR;

   Py_INCREF(dev);
   PyModule_AddObject(m, "Device", (PyObject *)dev);
   Py_INCREF(adapter);
   PyModule_AddObject(m, "Adapter", (PyObject *)adapter);

   // constants for event types
   PyModule_AddIntMacro(m, EVENT_LOG);
   PyModule_AddIntMacro(m, EVENT_KEYPRESS);
   PyModule_AddIntMacro(m, EVENT_COMMAND);
   PyModule_AddIntMacro(m, EVENT_CONFIG_CHANGE);
   PyModule_AddIntMacro(m, EVENT_ALERT);
   PyModule_AddIntMacro(m, EVENT_MENU_CHANGED);
   PyModule_AddIntMacro(m, EVENT_ACTIVATED);
   PyModule_AddIntMacro(m, EVENT_ALL);

   // constants for alert types
   PyModule_AddIntConstant(m, "CEC_ALERT_SERVICE_DEVICE",
         CEC_ALERT_SERVICE_DEVICE);
   PyModule_AddIntConstant(m, "CEC_ALERT_CONNECTION_LOST",
         CEC_ALERT_CONNECTION_LOST);
   PyModule_AddIntConstant(m, "CEC_ALERT_PERMISSION_ERROR",
         CEC_ALERT_PERMISSION_ERROR);
   PyModule_AddIntConstant(m, "CEC_ALERT_PORT_BUSY",
         CEC_ALERT_PORT_BUSY);
   PyModule_AddIntConstant(m, "CEC_ALERT_PHYSICAL_ADDRESS_ERROR",
         CEC_ALERT_PHYSICAL_ADDRESS_ERROR);
   PyModule_AddIntConstant(m, "CEC_ALERT_TV_POLL_FAILED",
         CEC_ALERT_TV_POLL_FAILED);

   // constants for menu events
   PyModule_AddIntConstant(m, "CEC_MENU_STATE_ACTIVATED",
         CEC_MENU_STATE_ACTIVATED);
   PyModule_AddIntConstant(m, "CEC_MENU_STATE_DEACTIVATED",
         CEC_MENU_STATE_DEACTIVATED);

   // constants for device types
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_TV",
         CEC_DEVICE_TYPE_TV);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_RECORDING_DEVICE",
         CEC_DEVICE_TYPE_RECORDING_DEVICE);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_RESERVED",
         CEC_DEVICE_TYPE_RESERVED);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_TUNER",
         CEC_DEVICE_TYPE_TUNER);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_PLAYBACK_DEVICE",
         CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
   PyModule_AddIntConstant(m, "CEC_DEVICE_TYPE_AUDIO_SYSTEM",
         CEC_DEVICE_TYPE_AUDIO_SYSTEM);

   // constants for logical addresses
   PyModule_AddIntConstant(m, "CECDEVICE_UNKNOWN",
         CECDEVICE_UNKNOWN);
   PyModule_AddIntConstant(m, "CECDEVICE_TV",
         CECDEVICE_TV);
   PyModule_AddIntConstant(m, "CECDEVICE_RECORDINGDEVICE1",
         CECDEVICE_RECORDINGDEVICE1);
   PyModule_AddIntConstant(m, "CECDEVICE_RECORDINGDEVICE2",
         CECDEVICE_RECORDINGDEVICE2);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER1",
         CECDEVICE_TUNER1);
   PyModule_AddIntConstant(m, "CECDEVICE_PLAYBACKDEVICE1",
         CECDEVICE_PLAYBACKDEVICE1);
   PyModule_AddIntConstant(m, "CECDEVICE_AUDIOSYSTEM",
         CECDEVICE_AUDIOSYSTEM);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER2",
         CECDEVICE_TUNER2);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER3",
         CECDEVICE_TUNER3);
   PyModule_AddIntConstant(m, "CECDEVICE_PLAYBACKDEVICE2",
         CECDEVICE_PLAYBACKDEVICE2);
   PyModule_AddIntConstant(m, "CECDEVICE_RECORDINGDEVICE3",
         CECDEVICE_RECORDINGDEVICE3);
   PyModule_AddIntConstant(m, "CECDEVICE_TUNER4",
         CECDEVICE_TUNER4);
   PyModule_AddIntConstant(m, "CECDEVICE_PLAYBACKDEVICE3",
         CECDEVICE_PLAYBACKDEVICE3);
   PyModule_AddIntConstant(m, "CECDEVICE_RESERVED1",
         CECDEVICE_RESERVED1);
   PyModule_AddIntConstant(m, "CECDEVICE_RESERVED2",
         CECDEVICE_RESERVED2);
   PyModule_AddIntConstant(m, "CECDEVICE_FREEUSE",
         CECDEVICE_FREEUSE);
   PyModule_AddIntConstant(m, "CECDEVICE_UNREGISTERED",
         CECDEVICE_UNREGISTERED);
   PyModule_AddIntConstant(m, "CECDEVICE_BROADCAST",
         CECDEVICE_BROADCAST);

   // constants for opcodes
   PyModule_AddIntConstant(m, "CEC_OPCODE_ACTIVE_SOURCE",
         CEC_OPCODE_ACTIVE_SOURCE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_IMAGE_VIEW_ON",
         CEC_OPCODE_IMAGE_VIEW_ON);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TEXT_VIEW_ON",
         CEC_OPCODE_TEXT_VIEW_ON);
   PyModule_AddIntConstant(m, "CEC_OPCODE_INACTIVE_SOURCE",
         CEC_OPCODE_INACTIVE_SOURCE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REQUEST_ACTIVE_SOURCE",
         CEC_OPCODE_REQUEST_ACTIVE_SOURCE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_ROUTING_CHANGE",
         CEC_OPCODE_ROUTING_CHANGE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_ROUTING_INFORMATION",
         CEC_OPCODE_ROUTING_INFORMATION);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_STREAM_PATH",
         CEC_OPCODE_SET_STREAM_PATH);
   PyModule_AddIntConstant(m, "CEC_OPCODE_STANDBY",
         CEC_OPCODE_STANDBY);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_OFF",
         CEC_OPCODE_RECORD_OFF);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_ON",
         CEC_OPCODE_RECORD_ON);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_STATUS",
         CEC_OPCODE_RECORD_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_RECORD_TV_SCREEN",
         CEC_OPCODE_RECORD_TV_SCREEN);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CLEAR_ANALOGUE_TIMER",
         CEC_OPCODE_CLEAR_ANALOGUE_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CLEAR_DIGITAL_TIMER",
         CEC_OPCODE_CLEAR_DIGITAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CLEAR_EXTERNAL_TIMER",
         CEC_OPCODE_CLEAR_EXTERNAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_ANALOGUE_TIMER",
         CEC_OPCODE_SET_ANALOGUE_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_DIGITAL_TIMER",
         CEC_OPCODE_SET_DIGITAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_EXTERNAL_TIMER",
         CEC_OPCODE_SET_EXTERNAL_TIMER);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_TIMER_PROGRAM_TITLE",
         CEC_OPCODE_SET_TIMER_PROGRAM_TITLE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TIMER_CLEARED_STATUS",
         CEC_OPCODE_TIMER_CLEARED_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TIMER_STATUS",
         CEC_OPCODE_TIMER_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CEC_VERSION",
         CEC_OPCODE_CEC_VERSION);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GET_CEC_VERSION",
         CEC_OPCODE_GET_CEC_VERSION);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_PHYSICAL_ADDRESS",
         CEC_OPCODE_GIVE_PHYSICAL_ADDRESS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GET_MENU_LANGUAGE",
         CEC_OPCODE_GET_MENU_LANGUAGE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_PHYSICAL_ADDRESS",
         CEC_OPCODE_REPORT_PHYSICAL_ADDRESS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_MENU_LANGUAGE",
         CEC_OPCODE_SET_MENU_LANGUAGE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_DECK_CONTROL",
         CEC_OPCODE_DECK_CONTROL);
   PyModule_AddIntConstant(m, "CEC_OPCODE_DECK_STATUS",
         CEC_OPCODE_DECK_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_DECK_STATUS",
         CEC_OPCODE_GIVE_DECK_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_PLAY",
         CEC_OPCODE_PLAY);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS",
         CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SELECT_ANALOGUE_SERVICE",
         CEC_OPCODE_SELECT_ANALOGUE_SERVICE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SELECT_DIGITAL_SERVICE",
         CEC_OPCODE_SELECT_DIGITAL_SERVICE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TUNER_DEVICE_STATUS",
         CEC_OPCODE_TUNER_DEVICE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TUNER_STEP_DECREMENT",
         CEC_OPCODE_TUNER_STEP_DECREMENT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_TUNER_STEP_INCREMENT",
         CEC_OPCODE_TUNER_STEP_INCREMENT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_DEVICE_VENDOR_ID",
         CEC_OPCODE_DEVICE_VENDOR_ID);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_DEVICE_VENDOR_ID",
         CEC_OPCODE_GIVE_DEVICE_VENDOR_ID);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_COMMAND",
         CEC_OPCODE_VENDOR_COMMAND);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_COMMAND_WITH_ID",
         CEC_OPCODE_VENDOR_COMMAND_WITH_ID);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN",
         CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN);
   PyModule_AddIntConstant(m, "CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP",
         CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_OSD_STRING",
         CEC_OPCODE_SET_OSD_STRING);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_OSD_NAME",
         CEC_OPCODE_GIVE_OSD_NAME);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_OSD_NAME",
         CEC_OPCODE_SET_OSD_NAME);
   PyModule_AddIntConstant(m, "CEC_OPCODE_MENU_REQUEST",
         CEC_OPCODE_MENU_REQUEST);
   PyModule_AddIntConstant(m, "CEC_OPCODE_MENU_STATUS",
         CEC_OPCODE_MENU_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_USER_CONTROL_PRESSED",
         CEC_OPCODE_USER_CONTROL_PRESSED);
   PyModule_AddIntConstant(m, "CEC_OPCODE_USER_CONTROL_RELEASE",
         CEC_OPCODE_USER_CONTROL_RELEASE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_DEVICE_POWER_STATUS",
         CEC_OPCODE_GIVE_DEVICE_POWER_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_POWER_STATUS",
         CEC_OPCODE_REPORT_POWER_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_FEATURE_ABORT",
         CEC_OPCODE_FEATURE_ABORT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_ABORT",
         CEC_OPCODE_ABORT);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_AUDIO_STATUS",
         CEC_OPCODE_GIVE_AUDIO_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS",
         CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_AUDIO_STATUS",
         CEC_OPCODE_REPORT_AUDIO_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_SYSTEM_AUDIO_MODE",
         CEC_OPCODE_SET_SYSTEM_AUDIO_MODE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST",
         CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS",
         CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS);
   PyModule_AddIntConstant(m, "CEC_OPCODE_SET_AUDIO_RATE",
         CEC_OPCODE_SET_AUDIO_RATE);
   PyModule_AddIntConstant(m, "CEC_OPCODE_START_ARC",
         CEC_OPCODE_START_ARC);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_ARC_STARTED",
         CEC_OPCODE_REPORT_ARC_STARTED);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REPORT_ARC_ENDED",
         CEC_OPCODE_REPORT_ARC_ENDED);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REQUEST_ARC_START",
         CEC_OPCODE_REQUEST_ARC_START);
   PyModule_AddIntConstant(m, "CEC_OPCODE_REQUEST_ARC_END",
         CEC_OPCODE_REQUEST_ARC_END);
   PyModule_AddIntConstant(m, "CEC_OPCODE_END_ARC",
         CEC_OPCODE_END_ARC);
   PyModule_AddIntConstant(m, "CEC_OPCODE_CDC",
         CEC_OPCODE_CDC);
   PyModule_AddIntConstant(m, "CEC_OPCODE_NONE",
         CEC_OPCODE_NONE);

   // expose whether or not we're using the new cec_adapter_descriptor API
   // this should help debugging by exposing which version was detected and
   // which adapter detection API was used at compile time
   PyModule_AddIntMacro(m, HAVE_CEC_ADAPTER_DESCRIPTOR);

#if PY_MAJOR_VERSION >= 3
   return m;
#endif
}
