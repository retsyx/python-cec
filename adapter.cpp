/* adapter.cpp
 *
 * Copyright (C) 2024 retsyx <retsyx@gmail.com>
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
 * Implementation of CEC Adapter class for Python
 *
 * Author: retsyx <retsyx@gmail.com>
 */

#define __STDC_FORMAT_MACROS

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <inttypes.h>
#include <libcec/cec.h>
#include <list>
#include <stdlib.h>

#include "cec.h"
#include "adapter.h"
#include "device.h"

using namespace CEC;

static PyObject * make_bound_method_args(PyObject * self, PyObject * args) {
    Py_ssize_t count = 0;
    if( PyTuple_Check(args) ) {
        count = PyTuple_Size(args);
    }
    PyObject * result = PyTuple_New(count+1);
    if( result == NULL ) {
        return NULL;
    }
    assert(self != NULL);
    Py_INCREF(self);
    PyTuple_SetItem(result, 0, self);
    for( Py_ssize_t i=0; i<count; i++ ) {
        PyObject * arg = PyTuple_GetItem(args, i);
        if( arg == NULL ) {
            Py_DECREF(result);
            return NULL;
        }
        Py_INCREF(arg);
        PyTuple_SetItem(result, i+1, arg);
    }
    return result;
}

static PyObject * trigger_event(void * param, long int event, PyObject * args) {
    Adapter * self = (Adapter *)param;
    assert(event & EVENT_ALL);
    Py_INCREF(Py_None);
    PyObject * result = Py_None;

    //debug("Triggering event %ld\n", event);

    int i=0;
    for (cb_list::const_iterator itr = self->callbacks.begin();
            itr != self->callbacks.end();
            ++itr) {
        //debug("Checking callback %d with events %ld\n", i, itr->event);
        if (itr->event & event) {
            //debug("Calling callback %d\n", i);
            PyObject * callback = itr->cb;
            PyObject * arguments = args;
            if ( PyMethod_Check(itr->cb)) {
                callback = PyMethod_Function(itr->cb);
                PyObject * self = PyMethod_Self(itr->cb);
                if( self ) {
                // bound method, prepend self/cls to argument tuple
                arguments = make_bound_method_args(self, args);
                }
            }
            // see also: PyObject_CallFunction(...) which can take C args
            PyObject * temp = PyObject_CallObject(callback, arguments);
            if (arguments != args) {
                Py_XDECREF(arguments);
            }
            if (temp) {
                debug("Callback succeeded\n");
                Py_DECREF(temp);
            } else {
                debug("Callback failed\n");
                Py_DECREF(Py_None);
                return NULL;
            }
        }
        i++;
    }

    return result;
}

// CEC callback implementations

#if CEC_LIB_VERSION_MAJOR >= 4
static void log_cb(void * self, const cec_log_message* message) {
#else
static int log_cb(void * self, const cec_log_message message) {
#endif
    debug("got log callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
#if CEC_LIB_VERSION_MAJOR >= 4
    int level = message->level;
    long int time = message->time;
    const char * msg = message->message;
#else
    int level = message.level;
    long int time = message.time;
    const char* msg = message.message;
#endif
    // decode message ignoring invalid characters
    PyObject * umsg = PyUnicode_DecodeASCII(msg, strlen(msg), "ignore");
    PyObject * args = Py_BuildValue("(iilO)", EVENT_LOG,
            level,
            time,
            umsg);
    if (args) {
        trigger_event(self, EVENT_LOG, args);
        Py_DECREF(args);
    }
    Py_XDECREF(umsg);
    PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
    return;
#else
    return 1;
#endif
}


#if CEC_LIB_VERSION_MAJOR >= 4
    static void keypress_cb(void * self, const cec_keypress* key) {
#else
    static int keypress_cb(void * self, const cec_keypress key) {
#endif
    debug("got keypress callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
#if CEC_LIB_VERSION_MAJOR >= 4
    cec_user_control_code keycode = key->keycode;
    unsigned int duration = key->duration;
#else
    cec_user_control_code keycode = key.keycode;
    unsigned int duration = key.duration;
#endif
    PyObject * args = Py_BuildValue("(iBI)", EVENT_KEYPRESS,
            keycode,
            duration);
    if( args ) {
        trigger_event(self, EVENT_KEYPRESS, args);
        Py_DECREF(args);
    }
    PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
    return;
#else
    return 1;
#endif
}

static PyObject * convert_cmd(const cec_command* cmd) {
#if PY_MAJOR_VERSION >= 3
    return Py_BuildValue("{sBsBsOsOsBsy#sOsi}",
#else
    return Py_BuildValue("{sBsBsOsOsBss#sOsi}",
#endif
            "initiator", cmd->initiator,
            "destination", cmd->destination,
            "ack", cmd->ack ? Py_True : Py_False,
            "eom", cmd->eom ? Py_True : Py_False,
            "opcode", cmd->opcode,
            "parameters", cmd->parameters.data, cmd->parameters.size,
            "opcode_set", cmd->opcode_set ? Py_True : Py_False,
            "transmit_timeout", cmd->transmit_timeout);
    }

#if CEC_LIB_VERSION_MAJOR >= 4
static void command_cb(void * self, const cec_command* command) {
#else
static int command_cb(void * self, const cec_command command) {
#endif
    debug("got command callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
#if CEC_LIB_VERSION_MAJOR >= 4
    const cec_command * cmd = command;
#else
    const cec_command * cmd = &command;
#endif
    PyObject * args = Py_BuildValue("(iO&)", EVENT_COMMAND, convert_cmd, cmd);
    if( args ) {
        trigger_event(self, EVENT_COMMAND, args);
        Py_DECREF(args);
    }
    PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
    return;
#else
    return 1;
#endif
}

#if CEC_LIB_VERSION_MAJOR >= 4
static void config_cb(void * self, const libcec_configuration*) {
#else
static int config_cb(void * self, const libcec_configuration) {
#endif
    debug("got config callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    // TODO: figure out how to pass these as parameters
    // yeah... right.
    //  we'll probably have to come up with some functions for converting the
    //  libcec_configuration class into a python Object
    //  this will probably be _lots_ of work and should probably wait until
    //  a later release, or when it becomes necessary.
    PyObject * args = Py_BuildValue("(i)", EVENT_CONFIG_CHANGE);
    if (args) {
        // don't bother triggering an event until we can actually pass arguments
        //trigger_event(self, EVENT_CONFIG_CHANGE, args);
        Py_DECREF(args);
    }
    PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
    return;
#else
    return 1;
#endif
}

#if CEC_LIB_VERSION_MAJOR >= 4
static void alert_cb(void * self, const libcec_alert alert, const libcec_parameter p) {
#else
static int alert_cb(void * self, const libcec_alert alert, const libcec_parameter p) {
#endif
    debug("got alert callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    PyObject * param = Py_None;
    if ( p.paramType == CEC_PARAMETER_TYPE_STRING) {
        param = Py_BuildValue("s", p.paramData);
    } else {
        Py_INCREF(param);
    }
    PyObject * args = Py_BuildValue("(iiN)", EVENT_ALERT, alert, param);
    if (args) {
        trigger_event(self, EVENT_ALERT, args);
        Py_DECREF(args);
    }
    PyGILState_Release(gstate);
#if CEC_LIB_VERSION_MAJOR >= 4
    return;
#else
    return 1;
#endif
}

static int menu_cb(void * self, const cec_menu_state menu) {
    debug("got menu callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    PyObject * args = Py_BuildValue("(ii)", EVENT_MENU_CHANGED, menu);
    if (args) {
        trigger_event(self, EVENT_MENU_CHANGED, args);
        Py_DECREF(args);
    }
    PyGILState_Release(gstate);
    return 1;
}

static void activated_cb(void * self, const cec_logical_address logical_address,
        const uint8_t state) {
    debug("got activated callback\n");
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    PyObject * active = (state == 1) ? Py_True : Py_False;
    PyObject * args = Py_BuildValue("(iOi)", EVENT_ACTIVATED, active,
        logical_address);
    if (args) {
        trigger_event(self, EVENT_ACTIVATED, args);
        Py_DECREF(args);
    }
    PyGILState_Release(gstate);
    return;
}

// Python methods

static PyObject * list_devices(Adapter * self, PyObject * args) {
    PyObject * result = NULL;

    if (!PyArg_ParseTuple(args, ":list_devices")) {
        return NULL;
    }

    cec_logical_addresses devices;
    Py_BEGIN_ALLOW_THREADS
    devices = self->adapter->GetActiveDevices();
    Py_END_ALLOW_THREADS

    result = PyDict_New();
    for (uint8_t i=0; i<32; i++) {
        if (devices[i]) {
            PyObject * ii = Py_BuildValue("Ob", self, i);
            PyObject * dev = PyObject_CallObject((PyObject *)DeviceType(), ii);

            Py_DECREF(ii);
            if (dev) {
                PyDict_SetItem(result, Py_BuildValue("b", i), dev);
            } else {
                Py_DECREF(result);
                PyErr_SetString(PyExc_ValueError, "Failed to create Device object");
                result = NULL;
                break;
            }
        }
    }

    return result;
}

static PyObject * adapter_close(Adapter * self, PyObject * args) {
    if (self->adapter != NULL) {
        Py_BEGIN_ALLOW_THREADS
        self->adapter->Close();
        self->adapter = NULL;
        Py_END_ALLOW_THREADS
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * add_callback(Adapter * self, PyObject * args) {
    PyObject * callback;
    long int events = EVENT_ALL; // default to all events

    if (!PyArg_ParseTuple(args, "O|i:add_callback", &callback, &events)) {
        return NULL;
    }
    // check that event is one of the allowed events
    if (events & ~(EVENT_VALID)) {
        PyErr_SetString(PyExc_TypeError, "Invalid event(s) for callback");
        return NULL;
    }
    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
    }

    Py_INCREF(callback);
    Callback new_cb(events, callback);

    debug("Adding callback for event %ld\n", events);
    self->callbacks.push_back(new_cb);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * remove_callback(Adapter * self, PyObject * args) {
    PyObject * callback;
    Py_ssize_t events = EVENT_ALL; // default to all events

    if (PyArg_ParseTuple(args, "O|i:remove_callback", &callback, &events)) {
        for (cb_list::iterator itr = self->callbacks.begin(); itr != self->callbacks.end(); ++itr ) {
            if (itr->cb == callback) {
                // clear out the given events for this callback
                itr->event &= ~(events);
                if (itr->event == 0) {
                    // if this callback has no events, remove it
                    itr = self->callbacks.erase(itr);
                    Py_DECREF(callback);
                }
            }
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * transmit(Adapter * self, PyObject * args) {
    unsigned char initiator = 'g';
    unsigned char destination;
    unsigned char opcode;
    const char * params = NULL;
    Py_ssize_t param_count = 0;

    if (PyArg_ParseTuple(args, "bb|s#b:transmit", &destination, &opcode,
            &params, &param_count, &initiator)) {
        if (destination < 0 || destination > 15) {
            PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
            return NULL;
        }
        if (initiator != 'g') {
            if( initiator < 0 || initiator > 15 ) {
                PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
                return NULL;
            }
        } else {
            initiator = self->adapter->GetLogicalAddresses().primary;
        }
        if (param_count > CEC_MAX_DATA_PACKET_SIZE) {
            char errstr[1024];
            snprintf(errstr, 1024, "Too many parameters, maximum is %d",
                CEC_MAX_DATA_PACKET_SIZE);
            PyErr_SetString(PyExc_ValueError, errstr);
            return NULL;
        }
        cec_command data;
        bool success;
        Py_BEGIN_ALLOW_THREADS
        data.initiator = (cec_logical_address)initiator;
        data.destination = (cec_logical_address)destination;
        data.opcode = (cec_opcode)opcode;
        data.opcode_set = 1;
        if (params) {
            for (Py_ssize_t i=0; i<param_count; i++) {
                data.PushBack(((uint8_t *)params)[i]);
            }
        }
        success = self->adapter->Transmit(data);
        Py_END_ALLOW_THREADS
        RETURN_BOOL(success);
    }

    return NULL;
}

static PyObject * is_active_source(Adapter * self, PyObject * args) {
    unsigned char addr;

    if (PyArg_ParseTuple(args, "b:is_active_source", &addr)) {
        if (addr < 0 || addr > 15) {
            PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
            return NULL;
        } else {
            RETURN_BOOL(self->adapter->IsActiveSource((cec_logical_address)addr));
        }
    }
    return NULL;
}

static PyObject * set_active_source(Adapter * self, PyObject * args) {
    unsigned char devtype = (unsigned char)CEC_DEVICE_TYPE_RESERVED;

    if (PyArg_ParseTuple(args, "|b:set_active_source", &devtype)) {
        if (devtype < 0 || devtype > 5) {
            PyErr_SetString(PyExc_ValueError, "Device type must be between 0 and 5");
            return NULL;
        } else {
            RETURN_BOOL(self->adapter->SetActiveSource((cec_device_type)devtype));
        }
    }
    return NULL;
}

static PyObject * volume_up(Adapter * self, PyObject * args) {
    if (PyArg_ParseTuple(args, ":volume_up")) {
        RETURN_BOOL(self->adapter->VolumeUp());
    }
    return NULL;
}

static PyObject * volume_down(Adapter * self, PyObject * args) {
    if (PyArg_ParseTuple(args, ":volume_up")) {
        RETURN_BOOL(self->adapter->VolumeDown());
    }
    return NULL;
}

#if CEC_LIB_VERSION_MAJOR > 1
static PyObject * toggle_mute(Adapter * self, PyObject * args) {
    if (PyArg_ParseTuple(args, ":toggle_mute")) {
        RETURN_BOOL(self->adapter->AudioToggleMute());
    }
    return NULL;
}
#endif

static PyObject * set_stream_path(Adapter * self, PyObject * args) {
    PyObject * arg;
    const char * arg_s = NULL;

    if (PyArg_ParseTuple(args, "O:set_stream_path", &arg)) {
        Py_INCREF(arg);

#if PY_MAJOR_VERSION >= 3
        if (PyLong_Check(arg)) {
            long arg_l = PyLong_AsLong(arg);
#else
        if (PyInt_Check(arg)) {
            long arg_l = PyInt_AsLong(arg);
#endif
            Py_DECREF(arg);
            if (arg_l < 0 || arg_l > 15) {
                PyErr_SetString(PyExc_ValueError, "Logical address must be between 0 and 15");
                return NULL;
            } else {
                RETURN_BOOL(self->adapter->SetStreamPath((cec_logical_address)arg_l));
            }

#if PY_MAJOR_VERSION >= 3
        } else if (PyUnicode_Check(arg)) {
            arg_s = PyUnicode_AsUTF8(arg);
#else
        } else if (PyString_Check(arg)) {
            arg_s = PyString_AsString(arg);
#endif
        } else if (PyUnicode_Check(arg)) {
            // Convert from Unicode to ASCII
            PyObject * ascii_arg = PyUnicode_AsASCIIString(arg);
            if (NULL == ascii_arg) {
                // Means the string can't be converted to ASCII, the codec failed
                PyErr_SetString(PyExc_ValueError, "Could not convert address to ASCII");
                return NULL;
            }

            // Get the actual bytes as a C string
            arg_s = PyByteArray_AsString(ascii_arg);
        }

        if (arg_s) {
            int pa = parse_physical_addr(arg_s);
            Py_DECREF(arg);
            if (pa < 0) {
                PyErr_SetString(PyExc_ValueError, "Invalid physical address");
                return NULL;
            } else {
                RETURN_BOOL(self->adapter->SetStreamPath((uint16_t)pa));
            }
        } else {
            Py_DECREF(arg);
            PyErr_SetString(PyExc_TypeError, "parameter must be string or int");
            return NULL;
        }
    }

    return NULL;
}

static PyObject * set_physical_addr(Adapter * self, PyObject * args) {
    char * addr_s;

    if (!PyArg_ParseTuple(args, "s:set_physical_addr", &addr_s)) {
        return NULL;
    }

    int addr = parse_physical_addr(addr_s);
    if (addr >= 0) {
        RETURN_BOOL(self->adapter->SetPhysicalAddress((uint16_t)addr));
    }

    PyErr_SetString(PyExc_ValueError, "Invalid physical address");
    return NULL;
}

static PyObject * set_port(Adapter * self, PyObject * args) {
    unsigned char dev, port;
    if (!PyArg_ParseTuple(args, "bb", &dev, &port)) {
        return NULL;
    }
    if (dev > 15) {
        PyErr_SetString(PyExc_ValueError, "Invalid logical address");
        return NULL;
    }
    if (port > 15) {
        PyErr_SetString(PyExc_ValueError, "Invalid port");
        return NULL;
    }
    RETURN_BOOL(self->adapter->SetHDMIPort((cec_logical_address)dev, port));
}

PyObject * can_persist_config(Adapter * self, PyObject * args) {
    if (!PyArg_ParseTuple(args, ":can_persist_config")) {
        return NULL;
    }
#if CEC_LIB_VERSION_MAJOR >= 5
    RETURN_BOOL(self->adapter->CanSaveConfiguration());
#else
    RETURN_BOOL(self->adapter->CanPersistConfiguration());
#endif
}

PyObject * persist_config(Adapter * self, PyObject * args) {
    if (!PyArg_ParseTuple(args, ":persist_config") ) {
        return NULL;
    }
#if CEC_LIB_VERSION_MAJOR >= 5
    if (!self->adapter->CanSaveConfiguration()) {
#else
    if (!self->adapter->CanPersistConfiguration()) {
#endif
        PyErr_SetString(PyExc_NotImplementedError, "Cannot persist configuration");
        return NULL;
    }
    libcec_configuration config;
    if (!self->adapter->GetCurrentConfiguration(&config)) {
        PyErr_SetString(PyExc_IOError, "Could not get configuration");
        return NULL;
    }
#if CEC_LIB_VERSION_MAJOR >= 5
    RETURN_BOOL(self->adapter->SetConfiguration(&config));
#else
    RETURN_BOOL(self->adapter->PersistConfiguration(&config));
#endif
}


// Getters/setters

static PyObject * Adapter_getDevice(Adapter * self, void * closure) {
    return Py_BuildValue("s", self->dev);
}

static PyObject * Adapter_getAddr(Adapter * self, void * closure) {
    cec_logical_address logicalAddress;
    Py_BEGIN_ALLOW_THREADS
    logicalAddress = self->adapter->GetLogicalAddresses().primary;
    Py_END_ALLOW_THREADS
    return Py_BuildValue("i", logicalAddress);
}

static PyObject * Adapter_getPhysicalAddress(Adapter * self, void * closure) {
    cec_logical_address logicalAddress;
    uint16_t physicalAddress;
    Py_BEGIN_ALLOW_THREADS
    logicalAddress = self->adapter->GetLogicalAddresses().primary;
    physicalAddress = self->adapter->GetDevicePhysicalAddress(logicalAddress);
    Py_END_ALLOW_THREADS
    char strAddr[8];
    snprintf(strAddr, 8, "%x.%x.%x.%x",
        (physicalAddress >> 12) & 0xF,
        (physicalAddress >> 8) & 0xF,
        (physicalAddress >> 4) & 0xF,
        physicalAddress & 0xF);

    return Py_BuildValue("s", strAddr);
}

static PyObject * Adapter_getVendor(Adapter * self, void * closure) {
    cec_logical_address logicalAddress;
    uint64_t vendorId;
    Py_BEGIN_ALLOW_THREADS
    logicalAddress = self->adapter->GetLogicalAddresses().primary;
    vendorId = self->adapter->GetDeviceVendorId(logicalAddress);
    Py_END_ALLOW_THREADS
    char vendor_str[7];
    snprintf(vendor_str, 7, "%06" PRIX64, vendorId);
    vendor_str[6] = '\0';
    return Py_BuildValue("s", vendor_str);
}

static PyObject * Adapter_getOsdString(Adapter * self, void * closure) {
    return Py_BuildValue("s", self->config.strDeviceName);
}

static PyObject * Adapter_getCECVersion(Adapter * self, void * closure) {
    return Py_BuildValue("s", "1.4");
}

static PyObject * Adapter_getLanguage(Adapter * self, void * closure) {
    return Py_BuildValue("s", self->config.strDeviceLanguage);
}



// Alloc/dealloc

std::list<CEC_ADAPTER_TYPE> get_adapters(ICECAdapter * adapter) {
    std::list<CEC_ADAPTER_TYPE> res;
    // release the Global Interpreter lock
    Py_BEGIN_ALLOW_THREADS
    // get adapters
    int cec_count = 10;
    CEC_ADAPTER_TYPE * dev_list = (CEC_ADAPTER_TYPE*)malloc(
            cec_count * sizeof(CEC_ADAPTER_TYPE));
    int count = adapter->CEC_FIND_ADAPTERS(dev_list, cec_count);
    if (count > cec_count) {
        cec_count = count;
        dev_list = (CEC_ADAPTER_TYPE*)realloc(dev_list,
            cec_count * sizeof(CEC_ADAPTER_TYPE));
        count = adapter->CEC_FIND_ADAPTERS(dev_list, cec_count);
        count = (std::min)(count, cec_count);
    }

    for (int i=0; i<count; i++) {
        res.push_back(dev_list[i]);
    }

    free(dev_list);
    // acquire the GIL before returning to code that uses python objects
    Py_END_ALLOW_THREADS
    return res;
}

static void Adapter_dealloc(Adapter * self) {
    if (self->adapter) {
        CECDestroy(self->adapter);
        self->adapter = NULL;
    }
    self->~Adapter();
    Py_TYPE(self)->tp_free((PyObject *)self);
}

#pragma GCC diagnostic ignored "-Wwrite-strings"
static PyObject * Adapter_new(PyTypeObject * type, PyObject * args, PyObject * kwargs) {
    bool success = false;
    Adapter * self;
    const char * dev = NULL;
    char * device_name = "python-cec";
    cec_device_type device_type = CEC_DEVICE_TYPE_RECORDING_DEVICE;
    char * keywords[] = { "dev", "name", "type", NULL};


    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|$ssi", keywords,
            &dev, &device_name, &device_type)) {
        return NULL;
    }

    if (device_type < CEC_DEVICE_TYPE_TV ||
            device_type > CEC_DEVICE_TYPE_AUDIO_SYSTEM) {
        PyErr_SetString(PyExc_Exception, "Invalid CEC device type");
        return NULL;
    }

    void * mem = type->tp_alloc(type, 0);
    if (!mem) {
        return NULL;
    }

    self = new (mem) Adapter();

    self->adapter = NULL;

    self->config.Clear();

    strncpy(self->config.strDeviceName, device_name, LIBCEC_OSD_NAME_SIZE - 1);

    // CEC_CLIENT_VERSION_CURRENT was introduced in 2.0.4
    // just use 2.1.0 because the conditional is simpler
#if CEC_LIB_VERSION_MAJOR >= 3
    self->config.clientVersion = LIBCEC_VERSION_CURRENT;
#elif CEC_LIB_VERSION_MAJOR >= 2 && CEC_LIB_VERSION_MINOR >= 1
    self->config.clientVersion = CEC_CLIENT_VERSION_CURRENT;
#else
    // fall back to 1.6.0 since it's the lowest common denominator shipped with
    // Ubuntu
    self->config.clientVersion = CEC_CLIENT_VERSION_1_6_0;
#endif
    self->config.bActivateSource = 0;
    self->config.deviceTypes.Add(device_type);

   // libcec callbacks
#if CEC_LIB_VERSION_MAJOR > 1 || ( CEC_LIB_VERSION_MAJOR == 1 && CEC_LIB_VERSION_MINOR >= 7 )
    self->cec_callbacks.Clear();
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
    self->cec_callbacks.logMessage = log_cb;
    self->cec_callbacks.keyPress = keypress_cb;
    self->cec_callbacks.commandReceived = command_cb;
    self->cec_callbacks.configurationChanged = config_cb;
    self->cec_callbacks.alert = alert_cb;
    self->cec_callbacks.menuStateChanged = menu_cb;
    self->cec_callbacks.sourceActivated = activated_cb;
#else
    self->cec_callbacks.CBCecLogMessage = log_cb;
    self->cec_callbacks.CBCecKeyPress = keypress_cb;
    self->cec_callbacks.CBCecCommand = command_cb;
    self->cec_callbacks.CBCecConfigurationChanged = config_cb;
    self->cec_callbacks.CBCecAlert = alert_cb;
    self->cec_callbacks.CBCecMenuStateChanged = menu_cb;
    self->cec_callbacks.CBCecSourceActivated = activated_cb;
#endif
    self->config.callbackParam = self;
    self->config.callbacks = &self->cec_callbacks;

    Py_BEGIN_ALLOW_THREADS
    self->adapter = CECInitialise(&self->config);
    Py_END_ALLOW_THREADS

    if (!self->adapter) {
        PyErr_SetString(PyExc_IOError, "Failed to initialize adapter");
        goto fail;
    }

    // The description of InitVideoStandalone() implies that it can only be called once.
    // However, libcec internally ensures that it is applied only once. So we can call it
    // multiple times.
#if CEC_LIB_VERSION_MAJOR > 1 || ( CEC_LIB_VERSION_MAJOR == 1 && CEC_LIB_VERSION_MINOR >= 8 )
    Py_BEGIN_ALLOW_THREADS
    self->adapter->InitVideoStandalone();
    Py_END_ALLOW_THREADS
#endif

    if (!dev) {
        std::list<CEC_ADAPTER_TYPE> devs = get_adapters(self->adapter);
        if (devs.size() > 0) {
#if HAVE_CEC_ADAPTER_DESCRIPTOR
        dev = devs.front().strComName;
#else
        dev = devs.front().comm;
#endif
        } else {
            PyErr_SetString(PyExc_Exception, "No default adapter found");
        }
    }

    if (!dev) {
        Adapter_dealloc(self);
        return NULL;
    }

    strncpy(self->dev, dev, sizeof(self->dev) - 1);

    Py_BEGIN_ALLOW_THREADS
    success = self->adapter->Open(dev);
    Py_END_ALLOW_THREADS
    if (!success) {
        char errstr[1024];
        snprintf(errstr, 1023, "CEC failed to open %s", dev);
        errstr[1023] = '\0';
        PyErr_SetString(PyExc_IOError, errstr);
        goto fail;
    }

    return (PyObject *)self;

fail:

    Adapter_dealloc(self);
    return NULL;
}

static PyObject * Adapter_str(Adapter * self) {
    char buf[1280];
    snprintf(buf, 1280, "CEC Adapter %s [%s]",
            self->dev,
            self->config.strDeviceName);
    return Py_BuildValue("s", buf);
}

static PyObject * Adapter_repr(Adapter * self) {
    char buf[1280];
    snprintf(buf, 1280,
            "Adapter(dev='%s', name='%s', type=%d)",
            self->dev,
            self->config.strDeviceName,
            self->config.deviceTypes[0]);
    return Py_BuildValue("s", buf);
}


static PyMethodDef Adapter_methods[] = {
    {"list_devices", (PyCFunction)list_devices, METH_VARARGS, "List devices"},
    {"close", (PyCFunction)adapter_close, METH_NOARGS, "Close the adapter"},
    {"add_callback", (PyCFunction)add_callback, METH_VARARGS, "Add a callback"},
    {"remove_callback", (PyCFunction)remove_callback, METH_VARARGS, "Remove a callback"},
    {"transmit", (PyCFunction)transmit, METH_VARARGS, "Transmit a raw CEC command"},
    {"is_active_source", (PyCFunction)is_active_source, METH_VARARGS, "Check active source"},
    {"set_active_source", (PyCFunction)set_active_source, METH_VARARGS, "Set active source"},
    {"volume_up", (PyCFunction)volume_up, METH_VARARGS, "Volume Up"},
    {"volume_down", (PyCFunction)volume_down, METH_VARARGS, "Volume Down"},
#if CEC_LIB_VERSION_MAJOR > 1
    {"toggle_mute", (PyCFunction)toggle_mute, METH_VARARGS, "Toggle Mute"},
#endif
    {"set_stream_path", (PyCFunction)set_stream_path, METH_VARARGS, "Set HDMI stream path"},
    {"set_physical_addr", (PyCFunction)set_physical_addr, METH_VARARGS, "Set HDMI physical address"},
    {"set_port", (PyCFunction)set_port, METH_VARARGS, "Set upstream HDMI port"},
    {"can_persist_config", (PyCFunction)can_persist_config, METH_VARARGS,
        "return true if the current adapter can persist the CEC configuration"},
    {"persist_config", (PyCFunction)persist_config, METH_VARARGS, "persist CEC configuration to adapter"},
     {NULL, NULL, 0, NULL}
};

static PyGetSetDef Adapter_getset[] = {
   {"adapter", (getter)Adapter_getDevice, (setter)NULL, "CEC Adapter"},
   {"address", (getter)Adapter_getAddr, (setter)NULL, "Logical Address"},
   {"physical_address", (getter)Adapter_getPhysicalAddress, (setter)NULL, "Physical Addresss"},
   {"vendor", (getter)Adapter_getVendor, (setter)NULL, "Vendor ID"},
   {"osd_string", (getter)Adapter_getOsdString, (setter)NULL, "OSD String"},
   {"cec_version", (getter)Adapter_getCECVersion, (setter)NULL, "CEC Version"},
   {"language", (getter)Adapter_getLanguage, (setter)NULL, "Language"},
   {NULL}
};

static PyTypeObject _AdapterType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "cec.Adapter",              /*tp_name*/
   sizeof(Adapter),            /*tp_basicsize*/
   0,                         /*tp_itemsize*/
   (destructor)Adapter_dealloc, /*tp_dealloc*/
   0,                         /*tp_print*/
   0,                         /*tp_getattr*/
   0,                         /*tp_setattr*/
   0,                         /*tp_compare*/
   (reprfunc)Adapter_repr,     /*tp_repr*/
   0,                         /*tp_as_number*/
   0,                         /*tp_as_sequence*/
   0,                         /*tp_as_mapping*/
   0,                         /*tp_hash */
   0,                         /*tp_call*/
   (reprfunc)Adapter_str,      /*tp_str*/
   0,                         /*tp_getattro*/
   0,                         /*tp_setattro*/
   0,                         /*tp_as_buffer*/
   Py_TPFLAGS_DEFAULT,        /*tp_flags*/
   "CEC Adapter objects",      /* tp_doc */
};

PyTypeObject * AdapterTypeInit() {
   _AdapterType.tp_new = Adapter_new;
   _AdapterType.tp_methods = Adapter_methods;
   _AdapterType.tp_getset = Adapter_getset;
   return &_AdapterType;
}

PyTypeObject * AdapterType() {
    return &_AdapterType;
}
