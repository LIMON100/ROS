// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from skyhunter_msgs:msg/ElectionVote.idl
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
#include "skyhunter_msgs/msg/detail/election_vote__struct.h"
#include "skyhunter_msgs/msg/detail/election_vote__functions.h"

#include "rosidl_runtime_c/string.h"
#include "rosidl_runtime_c/string_functions.h"


ROSIDL_GENERATOR_C_EXPORT
bool skyhunter_msgs__msg__election_vote__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[47];
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
    assert(strncmp("skyhunter_msgs.msg._election_vote.ElectionVote", full_classname_dest, 46) == 0);
  }
  skyhunter_msgs__msg__ElectionVote * ros_message = _ros_message;
  {  // term
    PyObject * field = PyObject_GetAttrString(_pymsg, "term");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->term = PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // candidate_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "candidate_id");
    if (!field) {
      return false;
    }
    assert(PyUnicode_Check(field));
    PyObject * encoded_field = PyUnicode_AsUTF8String(field);
    if (!encoded_field) {
      Py_DECREF(field);
      return false;
    }
    rosidl_runtime_c__String__assign(&ros_message->candidate_id, PyBytes_AS_STRING(encoded_field));
    Py_DECREF(encoded_field);
    Py_DECREF(field);
  }
  {  // voter_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "voter_id");
    if (!field) {
      return false;
    }
    assert(PyUnicode_Check(field));
    PyObject * encoded_field = PyUnicode_AsUTF8String(field);
    if (!encoded_field) {
      Py_DECREF(field);
      return false;
    }
    rosidl_runtime_c__String__assign(&ros_message->voter_id, PyBytes_AS_STRING(encoded_field));
    Py_DECREF(encoded_field);
    Py_DECREF(field);
  }
  {  // fitness_score
    PyObject * field = PyObject_GetAttrString(_pymsg, "fitness_score");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->fitness_score = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * skyhunter_msgs__msg__election_vote__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of ElectionVote */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("skyhunter_msgs.msg._election_vote");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "ElectionVote");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  skyhunter_msgs__msg__ElectionVote * ros_message = (skyhunter_msgs__msg__ElectionVote *)raw_ros_message;
  {  // term
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->term);
    {
      int rc = PyObject_SetAttrString(_pymessage, "term", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // candidate_id
    PyObject * field = NULL;
    field = PyUnicode_DecodeUTF8(
      ros_message->candidate_id.data,
      strlen(ros_message->candidate_id.data),
      "replace");
    if (!field) {
      return NULL;
    }
    {
      int rc = PyObject_SetAttrString(_pymessage, "candidate_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // voter_id
    PyObject * field = NULL;
    field = PyUnicode_DecodeUTF8(
      ros_message->voter_id.data,
      strlen(ros_message->voter_id.data),
      "replace");
    if (!field) {
      return NULL;
    }
    {
      int rc = PyObject_SetAttrString(_pymessage, "voter_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // fitness_score
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->fitness_score);
    {
      int rc = PyObject_SetAttrString(_pymessage, "fitness_score", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
