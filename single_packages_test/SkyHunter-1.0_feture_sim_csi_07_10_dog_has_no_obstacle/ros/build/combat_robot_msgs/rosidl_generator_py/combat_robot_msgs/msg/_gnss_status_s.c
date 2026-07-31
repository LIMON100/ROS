// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <stdbool.h>
#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "numpy/ndarrayobject.h"
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif
#include "rosidl_runtime_c/visibility_control.h"
#include "combat_robot_msgs/msg/detail/gnss_status__struct.h"
#include "combat_robot_msgs/msg/detail/gnss_status__functions.h"

ROSIDL_GENERATOR_C_IMPORT
bool std_msgs__msg__header__convert_from_py(PyObject * _pymsg, void * _ros_message);
ROSIDL_GENERATOR_C_IMPORT
PyObject * std_msgs__msg__header__convert_to_py(void * raw_ros_message);

ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__gnss_status__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[46];
    {
      char * class_name = NULL;
      char * module_name = NULL;
      {
        PyObject * class_attr = PyObject_GetAttrString(_pymsg, "__class__");
        if (class_attr) {
          PyObject * name_attr = PyObject_GetAttrString(class_attr, "__name__");
          if (name_attr) {
            class_name = (char *)PyUnicode_1BYTE_DATA(name_attr);
            Py_DECREF(name_attr);
          }
          PyObject * module_attr = PyObject_GetAttrString(class_attr, "__module__");
          if (module_attr) {
            module_name = (char *)PyUnicode_1BYTE_DATA(module_attr);
            Py_DECREF(module_attr);
          }
          Py_DECREF(class_attr);
        }
      }
      if (!class_name || !module_name) {
        return false;
      }
      snprintf(full_classname_dest, sizeof(full_classname_dest), "%s.%s", module_name, class_name);
    }
    assert(strncmp("combat_robot_msgs.msg._gnss_status.GnssStatus", full_classname_dest, 45) == 0);
  }
  combat_robot_msgs__msg__GnssStatus * ros_message = _ros_message;
  {  // header
    PyObject * field = PyObject_GetAttrString(_pymsg, "header");
    if (!field) {
      return false;
    }
    if (!std_msgs__msg__header__convert_from_py(field, &ros_message->header)) {
      Py_DECREF(field);
      return false;
    }
    Py_DECREF(field);
  }
  {  // fix_status
    PyObject * field = PyObject_GetAttrString(_pymsg, "fix_status");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->fix_status = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // num_satellites
    PyObject * field = PyObject_GetAttrString(_pymsg, "num_satellites");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->num_satellites = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // latitude
    PyObject * field = PyObject_GetAttrString(_pymsg, "latitude");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->latitude = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // longitude
    PyObject * field = PyObject_GetAttrString(_pymsg, "longitude");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->longitude = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // altitude_m
    PyObject * field = PyObject_GetAttrString(_pymsg, "altitude_m");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->altitude_m = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // heading_deg
    PyObject * field = PyObject_GetAttrString(_pymsg, "heading_deg");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->heading_deg = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // ground_speed_mps
    PyObject * field = PyObject_GetAttrString(_pymsg, "ground_speed_mps");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->ground_speed_mps = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // horizontal_accuracy_m
    PyObject * field = PyObject_GetAttrString(_pymsg, "horizontal_accuracy_m");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->horizontal_accuracy_m = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // vertical_accuracy_m
    PyObject * field = PyObject_GetAttrString(_pymsg, "vertical_accuracy_m");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->vertical_accuracy_m = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__gnss_status__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of GnssStatus */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._gnss_status");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "GnssStatus");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__GnssStatus * ros_message = (combat_robot_msgs__msg__GnssStatus *)raw_ros_message;
  {  // header
    PyObject * field = NULL;
    field = std_msgs__msg__header__convert_to_py(&ros_message->header);
    if (!field) {
      return NULL;
    }
    {
      int rc = PyObject_SetAttrString(_pymessage, "header", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // fix_status
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->fix_status);
    {
      int rc = PyObject_SetAttrString(_pymessage, "fix_status", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // num_satellites
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->num_satellites);
    {
      int rc = PyObject_SetAttrString(_pymessage, "num_satellites", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // latitude
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->latitude);
    {
      int rc = PyObject_SetAttrString(_pymessage, "latitude", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // longitude
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->longitude);
    {
      int rc = PyObject_SetAttrString(_pymessage, "longitude", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // altitude_m
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->altitude_m);
    {
      int rc = PyObject_SetAttrString(_pymessage, "altitude_m", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // heading_deg
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->heading_deg);
    {
      int rc = PyObject_SetAttrString(_pymessage, "heading_deg", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // ground_speed_mps
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->ground_speed_mps);
    {
      int rc = PyObject_SetAttrString(_pymessage, "ground_speed_mps", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // horizontal_accuracy_m
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->horizontal_accuracy_m);
    {
      int rc = PyObject_SetAttrString(_pymessage, "horizontal_accuracy_m", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // vertical_accuracy_m
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->vertical_accuracy_m);
    {
      int rc = PyObject_SetAttrString(_pymessage, "vertical_accuracy_m", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
