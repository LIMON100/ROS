// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
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
#include "combat_robot_msgs/msg/detail/swarm_follower_status__struct.h"
#include "combat_robot_msgs/msg/detail/swarm_follower_status__functions.h"

ROSIDL_GENERATOR_C_IMPORT
bool std_msgs__msg__header__convert_from_py(PyObject * _pymsg, void * _ros_message);
ROSIDL_GENERATOR_C_IMPORT
PyObject * std_msgs__msg__header__convert_to_py(void * raw_ros_message);

ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__swarm_follower_status__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[65];
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
    assert(strncmp("combat_robot_msgs.msg._swarm_follower_status.SwarmFollowerStatus", full_classname_dest, 64) == 0);
  }
  combat_robot_msgs__msg__SwarmFollowerStatus * ros_message = _ros_message;
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
  {  // robot_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "robot_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->robot_id = PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // leader_robot_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "leader_robot_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->leader_robot_id = PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // link_status
    PyObject * field = PyObject_GetAttrString(_pymsg, "link_status");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->link_status = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // last_heartbeat_sequence
    PyObject * field = PyObject_GetAttrString(_pymsg, "last_heartbeat_sequence");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->last_heartbeat_sequence = PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // heartbeat_age_sec
    PyObject * field = PyObject_GetAttrString(_pymsg, "heartbeat_age_sec");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->heartbeat_age_sec = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // last_operation_mode
    PyObject * field = PyObject_GetAttrString(_pymsg, "last_operation_mode");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->last_operation_mode = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // last_formation_type
    PyObject * field = PyObject_GetAttrString(_pymsg, "last_formation_type");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->last_formation_type = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // last_formation_number
    PyObject * field = PyObject_GetAttrString(_pymsg, "last_formation_number");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->last_formation_number = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // last_grouping_index
    PyObject * field = PyObject_GetAttrString(_pymsg, "last_grouping_index");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->last_grouping_index = (uint8_t)PyLong_AsUnsignedLong(field);
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

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__swarm_follower_status__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of SwarmFollowerStatus */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._swarm_follower_status");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "SwarmFollowerStatus");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__SwarmFollowerStatus * ros_message = (combat_robot_msgs__msg__SwarmFollowerStatus *)raw_ros_message;
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
  {  // robot_id
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->robot_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "robot_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // leader_robot_id
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->leader_robot_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "leader_robot_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // link_status
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->link_status);
    {
      int rc = PyObject_SetAttrString(_pymessage, "link_status", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // last_heartbeat_sequence
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->last_heartbeat_sequence);
    {
      int rc = PyObject_SetAttrString(_pymessage, "last_heartbeat_sequence", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // heartbeat_age_sec
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->heartbeat_age_sec);
    {
      int rc = PyObject_SetAttrString(_pymessage, "heartbeat_age_sec", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // last_operation_mode
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->last_operation_mode);
    {
      int rc = PyObject_SetAttrString(_pymessage, "last_operation_mode", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // last_formation_type
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->last_formation_type);
    {
      int rc = PyObject_SetAttrString(_pymessage, "last_formation_type", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // last_formation_number
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->last_formation_number);
    {
      int rc = PyObject_SetAttrString(_pymessage, "last_formation_number", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // last_grouping_index
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->last_grouping_index);
    {
      int rc = PyObject_SetAttrString(_pymessage, "last_grouping_index", field);
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

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
