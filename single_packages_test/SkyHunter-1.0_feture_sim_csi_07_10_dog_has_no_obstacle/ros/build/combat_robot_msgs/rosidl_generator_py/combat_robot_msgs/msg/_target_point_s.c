// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/TargetPoint.idl
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
#include "combat_robot_msgs/msg/detail/target_point__struct.h"
#include "combat_robot_msgs/msg/detail/target_point__functions.h"

ROSIDL_GENERATOR_C_IMPORT
bool std_msgs__msg__header__convert_from_py(PyObject * _pymsg, void * _ros_message);
ROSIDL_GENERATOR_C_IMPORT
PyObject * std_msgs__msg__header__convert_to_py(void * raw_ros_message);
bool combat_robot_msgs__msg__bounding_box2d__convert_from_py(PyObject * _pymsg, void * _ros_message);
PyObject * combat_robot_msgs__msg__bounding_box2d__convert_to_py(void * raw_ros_message);

ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__target_point__convert_from_py(PyObject * _pymsg, void * _ros_message)
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
    assert(strncmp("combat_robot_msgs.msg._target_point.TargetPoint", full_classname_dest, 47) == 0);
  }
  combat_robot_msgs__msg__TargetPoint * ros_message = _ros_message;
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
  {  // is_locked
    PyObject * field = PyObject_GetAttrString(_pymsg, "is_locked");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->is_locked = (Py_True == field);
    Py_DECREF(field);
  }
  {  // x
    PyObject * field = PyObject_GetAttrString(_pymsg, "x");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->x = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // y
    PyObject * field = PyObject_GetAttrString(_pymsg, "y");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->y = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // height
    PyObject * field = PyObject_GetAttrString(_pymsg, "height");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->height = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // class_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "class_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->class_id = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // box
    PyObject * field = PyObject_GetAttrString(_pymsg, "box");
    if (!field) {
      return false;
    }
    if (!combat_robot_msgs__msg__bounding_box2d__convert_from_py(field, &ros_message->box)) {
      Py_DECREF(field);
      return false;
    }
    Py_DECREF(field);
  }
  {  // track_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "track_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->track_id = (int32_t)PyLong_AsLong(field);
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__target_point__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of TargetPoint */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._target_point");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "TargetPoint");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__TargetPoint * ros_message = (combat_robot_msgs__msg__TargetPoint *)raw_ros_message;
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
  {  // is_locked
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->is_locked ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "is_locked", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // x
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->x);
    {
      int rc = PyObject_SetAttrString(_pymessage, "x", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // y
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->y);
    {
      int rc = PyObject_SetAttrString(_pymessage, "y", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // height
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->height);
    {
      int rc = PyObject_SetAttrString(_pymessage, "height", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // class_id
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->class_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "class_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // box
    PyObject * field = NULL;
    field = combat_robot_msgs__msg__bounding_box2d__convert_to_py(&ros_message->box);
    if (!field) {
      return NULL;
    }
    {
      int rc = PyObject_SetAttrString(_pymessage, "box", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // track_id
    PyObject * field = NULL;
    field = PyLong_FromLong(ros_message->track_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "track_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
