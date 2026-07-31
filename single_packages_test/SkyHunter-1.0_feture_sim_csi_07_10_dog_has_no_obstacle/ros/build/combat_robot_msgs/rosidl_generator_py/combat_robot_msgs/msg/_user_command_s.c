// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/UserCommand.idl
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
#include "combat_robot_msgs/msg/detail/user_command__struct.h"
#include "combat_robot_msgs/msg/detail/user_command__functions.h"

ROSIDL_GENERATOR_C_IMPORT
bool std_msgs__msg__header__convert_from_py(PyObject * _pymsg, void * _ros_message);
ROSIDL_GENERATOR_C_IMPORT
PyObject * std_msgs__msg__header__convert_to_py(void * raw_ros_message);

ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__user_command__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[48];
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
    assert(strncmp("combat_robot_msgs.msg._user_command.UserCommand", full_classname_dest, 47) == 0);
  }
  combat_robot_msgs__msg__UserCommand * ros_message = _ros_message;
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
  {  // command_from
    PyObject * field = PyObject_GetAttrString(_pymsg, "command_from");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->command_from = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // command_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "command_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->command_id = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // target_x
    PyObject * field = PyObject_GetAttrString(_pymsg, "target_x");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->target_x = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // target_y
    PyObject * field = PyObject_GetAttrString(_pymsg, "target_y");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->target_y = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // drone_target_lat
    PyObject * field = PyObject_GetAttrString(_pymsg, "drone_target_lat");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->drone_target_lat = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // drone_target_lon
    PyObject * field = PyObject_GetAttrString(_pymsg, "drone_target_lon");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->drone_target_lon = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // drone_target_valid
    PyObject * field = PyObject_GetAttrString(_pymsg, "drone_target_valid");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->drone_target_valid = (Py_True == field);
    Py_DECREF(field);
  }
  {  // gun_trigger
    PyObject * field = PyObject_GetAttrString(_pymsg, "gun_trigger");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->gun_trigger = (Py_True == field);
    Py_DECREF(field);
  }
  {  // gun_trigger_permission
    PyObject * field = PyObject_GetAttrString(_pymsg, "gun_trigger_permission");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->gun_trigger_permission = (Py_True == field);
    Py_DECREF(field);
  }
  {  // pan_speed
    PyObject * field = PyObject_GetAttrString(_pymsg, "pan_speed");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->pan_speed = (int8_t)PyLong_AsLong(field);
    Py_DECREF(field);
  }
  {  // tilt_speed
    PyObject * field = PyObject_GetAttrString(_pymsg, "tilt_speed");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->tilt_speed = (int8_t)PyLong_AsLong(field);
    Py_DECREF(field);
  }
  {  // zoom_command
    PyObject * field = PyObject_GetAttrString(_pymsg, "zoom_command");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->zoom_command = (int8_t)PyLong_AsLong(field);
    Py_DECREF(field);
  }
  {  // stream_command
    PyObject * field = PyObject_GetAttrString(_pymsg, "stream_command");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->stream_command = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__user_command__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of UserCommand */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._user_command");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "UserCommand");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__UserCommand * ros_message = (combat_robot_msgs__msg__UserCommand *)raw_ros_message;
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
  {  // command_from
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->command_from);
    {
      int rc = PyObject_SetAttrString(_pymessage, "command_from", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // command_id
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->command_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "command_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // target_x
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->target_x);
    {
      int rc = PyObject_SetAttrString(_pymessage, "target_x", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // target_y
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->target_y);
    {
      int rc = PyObject_SetAttrString(_pymessage, "target_y", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // drone_target_lat
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->drone_target_lat);
    {
      int rc = PyObject_SetAttrString(_pymessage, "drone_target_lat", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // drone_target_lon
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->drone_target_lon);
    {
      int rc = PyObject_SetAttrString(_pymessage, "drone_target_lon", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // drone_target_valid
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->drone_target_valid ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "drone_target_valid", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // gun_trigger
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->gun_trigger ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "gun_trigger", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // gun_trigger_permission
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->gun_trigger_permission ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "gun_trigger_permission", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // pan_speed
    PyObject * field = NULL;
    field = PyLong_FromLong(ros_message->pan_speed);
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
    field = PyLong_FromLong(ros_message->tilt_speed);
    {
      int rc = PyObject_SetAttrString(_pymessage, "tilt_speed", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // zoom_command
    PyObject * field = NULL;
    field = PyLong_FromLong(ros_message->zoom_command);
    {
      int rc = PyObject_SetAttrString(_pymessage, "zoom_command", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // stream_command
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->stream_command);
    {
      int rc = PyObject_SetAttrString(_pymessage, "stream_command", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
