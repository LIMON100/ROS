// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
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
#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__struct.h"
#include "combat_robot_msgs/msg/detail/pan_tilt_control_command__functions.h"

ROSIDL_GENERATOR_C_IMPORT
bool std_msgs__msg__header__convert_from_py(PyObject * _pymsg, void * _ros_message);
ROSIDL_GENERATOR_C_IMPORT
PyObject * std_msgs__msg__header__convert_to_py(void * raw_ros_message);

ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__pan_tilt_control_command__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[70];
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
    assert(strncmp("combat_robot_msgs.msg._pan_tilt_control_command.PanTiltControlCommand", full_classname_dest, 69) == 0);
  }
  combat_robot_msgs__msg__PanTiltControlCommand * ros_message = _ros_message;
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
  {  // control_mode
    PyObject * field = PyObject_GetAttrString(_pymsg, "control_mode");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->control_mode = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // horizontal_angle
    PyObject * field = PyObject_GetAttrString(_pymsg, "horizontal_angle");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->horizontal_angle = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // vertical_angle
    PyObject * field = PyObject_GetAttrString(_pymsg, "vertical_angle");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->vertical_angle = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // pan_speed
    PyObject * field = PyObject_GetAttrString(_pymsg, "pan_speed");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->pan_speed = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // tilt_speed
    PyObject * field = PyObject_GetAttrString(_pymsg, "tilt_speed");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->tilt_speed = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // pan_dir
    PyObject * field = PyObject_GetAttrString(_pymsg, "pan_dir");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->pan_dir = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // tilt_dir
    PyObject * field = PyObject_GetAttrString(_pymsg, "tilt_dir");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->tilt_dir = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__pan_tilt_control_command__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of PanTiltControlCommand */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._pan_tilt_control_command");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "PanTiltControlCommand");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__PanTiltControlCommand * ros_message = (combat_robot_msgs__msg__PanTiltControlCommand *)raw_ros_message;
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
  {  // control_mode
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->control_mode);
    {
      int rc = PyObject_SetAttrString(_pymessage, "control_mode", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // horizontal_angle
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->horizontal_angle);
    {
      int rc = PyObject_SetAttrString(_pymessage, "horizontal_angle", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // vertical_angle
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->vertical_angle);
    {
      int rc = PyObject_SetAttrString(_pymessage, "vertical_angle", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // pan_speed
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->pan_speed);
    {
      int rc = PyObject_SetAttrString(_pymessage, "pan_speed", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // tilt_speed
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->tilt_speed);
    {
      int rc = PyObject_SetAttrString(_pymessage, "tilt_speed", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // pan_dir
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->pan_dir);
    {
      int rc = PyObject_SetAttrString(_pymessage, "pan_dir", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // tilt_dir
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->tilt_dir);
    {
      int rc = PyObject_SetAttrString(_pymessage, "tilt_dir", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
