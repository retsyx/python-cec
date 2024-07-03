/* adapter.h
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
 * CEC adapter interface for Python
 *
 * Author: retsyx <retsyx@gmail.com>
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <list>

#include <libcec/cec.h>

struct Callback {
   public:
      long int event;
      PyObject * cb;

      Callback(long int e, PyObject * c) : event(e), cb(c) {}
};

typedef std::list<Callback> cb_list;

struct Adapter {
    PyObject_HEAD
    char dev[1024];
    CEC::libcec_configuration config;
    CEC::ICECCallbacks cec_callbacks;
    CEC::ICECAdapter * adapter;
    cb_list callbacks;

    Adapter() : adapter(NULL) {}
    ~Adapter() {}
};

PyTypeObject * AdapterTypeInit();
PyTypeObject * AdapterType();

std::list<CEC::CEC_ADAPTER_TYPE> get_adapters(CEC::ICECAdapter * adapter);

/*
 * Compat for libcec 3.x
 */
#if CEC_LIB_VERSION_MAJOR < 4
  #define CEC_MAX_DATA_PACKET_SIZE (16 * 4)
#endif
