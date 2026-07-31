// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
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
#include "combat_robot_msgs/msg/detail/swarm_robot_command__struct.h"
#include "combat_robot_msgs/msg/detail/swarm_robot_command__functions.h"

#include "rosidl_runtime_c/string.h"
#include "rosidl_runtime_c/string_functions.h"

#include "rosidl_runtime_c/primitives_sequence.h"
#include "rosidl_runtime_c/primitives_sequence_functions.h"

ROSIDL_GENERATOR_C_IMPORT
bool std_msgs__msg__header__convert_from_py(PyObject * _pymsg, void * _ros_message);
ROSIDL_GENERATOR_C_IMPORT
PyObject * std_msgs__msg__header__convert_to_py(void * raw_ros_message);

ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__swarm_robot_command__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[61];
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
    assert(strncmp("combat_robot_msgs.msg._swarm_robot_command.SwarmRobotCommand", full_classname_dest, 60) == 0);
  }
  combat_robot_msgs__msg__SwarmRobotCommand * ros_message = _ros_message;
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
  {  // sequence
    PyObject * field = PyObject_GetAttrString(_pymsg, "sequence");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->sequence = PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // command_type
    PyObject * field = PyObject_GetAttrString(_pymsg, "command_type");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->command_type = (uint8_t)PyLong_AsUnsignedLong(field);
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
  {  // target_robot_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "target_robot_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->target_robot_id = PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // operation_mode
    PyObject * field = PyObject_GetAttrString(_pymsg, "operation_mode");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->operation_mode = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // estop_requested
    PyObject * field = PyObject_GetAttrString(_pymsg, "estop_requested");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->estop_requested = (Py_True == field);
    Py_DECREF(field);
  }
  {  // path_command
    PyObject * field = PyObject_GetAttrString(_pymsg, "path_command");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->path_command = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // num_waypoints
    PyObject * field = PyObject_GetAttrString(_pymsg, "num_waypoints");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->num_waypoints = (uint16_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // path_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "path_id");
    if (!field) {
      return false;
    }
    assert(PyUnicode_Check(field));
    PyObject * encoded_field = PyUnicode_AsUTF8String(field);
    if (!encoded_field) {
      Py_DECREF(field);
      return false;
    }
    rosidl_runtime_c__String__assign(&ros_message->path_id, PyBytes_AS_STRING(encoded_field));
    Py_DECREF(encoded_field);
    Py_DECREF(field);
  }
  {  // path_json
    PyObject * field = PyObject_GetAttrString(_pymsg, "path_json");
    if (!field) {
      return false;
    }
    assert(PyUnicode_Check(field));
    PyObject * encoded_field = PyUnicode_AsUTF8String(field);
    if (!encoded_field) {
      Py_DECREF(field);
      return false;
    }
    rosidl_runtime_c__String__assign(&ros_message->path_json, PyBytes_AS_STRING(encoded_field));
    Py_DECREF(encoded_field);
    Py_DECREF(field);
  }
  {  // formation_type
    PyObject * field = PyObject_GetAttrString(_pymsg, "formation_type");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->formation_type = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // formation_number
    PyObject * field = PyObject_GetAttrString(_pymsg, "formation_number");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->formation_number = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // grouping_index
    PyObject * field = PyObject_GetAttrString(_pymsg, "grouping_index");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->grouping_index = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // slot_index
    PyObject * field = PyObject_GetAttrString(_pymsg, "slot_index");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->slot_index = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // selected_robot_count
    PyObject * field = PyObject_GetAttrString(_pymsg, "selected_robot_count");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->selected_robot_count = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // selected_robot_ids
    PyObject * field = PyObject_GetAttrString(_pymsg, "selected_robot_ids");
    if (!field) {
      return false;
    }
    {
      // TODO(dirk-thomas) use a better way to check the type before casting
      assert(field->ob_type != NULL);
      assert(field->ob_type->tp_name != NULL);
      assert(strcmp(field->ob_type->tp_name, "numpy.ndarray") == 0);
      PyArrayObject * seq_field = (PyArrayObject *)field;
      Py_INCREF(seq_field);
      assert(PyArray_NDIM(seq_field) == 1);
      assert(PyArray_TYPE(seq_field) == NPY_UINT32);
      Py_ssize_t size = 8;
      uint32_t * dest = ros_message->selected_robot_ids;
      for (Py_ssize_t i = 0; i < size; ++i) {
        uint32_t tmp = *(npy_uint32 *)PyArray_GETPTR1(seq_field, i);
        memcpy(&dest[i], &tmp, sizeof(uint32_t));
      }
      Py_DECREF(seq_field);
    }
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__swarm_robot_command__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of SwarmRobotCommand */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._swarm_robot_command");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "SwarmRobotCommand");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__SwarmRobotCommand * ros_message = (combat_robot_msgs__msg__SwarmRobotCommand *)raw_ros_message;
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
  {  // sequence
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->sequence);
    {
      int rc = PyObject_SetAttrString(_pymessage, "sequence", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // command_type
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->command_type);
    {
      int rc = PyObject_SetAttrString(_pymessage, "command_type", field);
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
  {  // target_robot_id
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->target_robot_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "target_robot_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // operation_mode
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->operation_mode);
    {
      int rc = PyObject_SetAttrString(_pymessage, "operation_mode", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // estop_requested
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->estop_requested ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "estop_requested", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // path_command
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->path_command);
    {
      int rc = PyObject_SetAttrString(_pymessage, "path_command", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // num_waypoints
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->num_waypoints);
    {
      int rc = PyObject_SetAttrString(_pymessage, "num_waypoints", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // path_id
    PyObject * field = NULL;
    field = PyUnicode_DecodeUTF8(
      ros_message->path_id.data,
      strlen(ros_message->path_id.data),
      "replace");
    if (!field) {
      return NULL;
    }
    {
      int rc = PyObject_SetAttrString(_pymessage, "path_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // path_json
    PyObject * field = NULL;
    field = PyUnicode_DecodeUTF8(
      ros_message->path_json.data,
      strlen(ros_message->path_json.data),
      "replace");
    if (!field) {
      return NULL;
    }
    {
      int rc = PyObject_SetAttrString(_pymessage, "path_json", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // formation_type
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->formation_type);
    {
      int rc = PyObject_SetAttrString(_pymessage, "formation_type", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // formation_number
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->formation_number);
    {
      int rc = PyObject_SetAttrString(_pymessage, "formation_number", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // grouping_index
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->grouping_index);
    {
      int rc = PyObject_SetAttrString(_pymessage, "grouping_index", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // slot_index
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->slot_index);
    {
      int rc = PyObject_SetAttrString(_pymessage, "slot_index", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // selected_robot_count
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->selected_robot_count);
    {
      int rc = PyObject_SetAttrString(_pymessage, "selected_robot_count", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // selected_robot_ids
    PyObject * field = NULL;
    field = PyObject_GetAttrString(_pymessage, "selected_robot_ids");
    if (!field) {
      return NULL;
    }
    assert(field->ob_type != NULL);
    assert(field->ob_type->tp_name != NULL);
    assert(strcmp(field->ob_type->tp_name, "numpy.ndarray") == 0);
    PyArrayObject * seq_field = (PyArrayObject *)field;
    assert(PyArray_NDIM(seq_field) == 1);
    assert(PyArray_TYPE(seq_field) == NPY_UINT32);
    assert(sizeof(npy_uint32) == sizeof(uint32_t));
    npy_uint32 * dst = (npy_uint32 *)PyArray_GETPTR1(seq_field, 0);
    uint32_t * src = &(ros_message->selected_robot_ids[0]);
    memcpy(dst, src, 8 * sizeof(uint32_t));
    Py_DECREF(field);
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
