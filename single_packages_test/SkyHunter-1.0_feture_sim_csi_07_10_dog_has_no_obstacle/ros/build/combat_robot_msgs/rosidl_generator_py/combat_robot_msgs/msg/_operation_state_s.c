// generated from rosidl_generator_py/resource/_idl_support.c.em
// with input from combat_robot_msgs:msg/OperationState.idl
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
#include "combat_robot_msgs/msg/detail/operation_state__struct.h"
#include "combat_robot_msgs/msg/detail/operation_state__functions.h"


ROSIDL_GENERATOR_C_EXPORT
bool combat_robot_msgs__msg__operation_state__convert_from_py(PyObject * _pymsg, void * _ros_message)
{
  // check that the passed message is of the expected Python class
  {
    char full_classname_dest[54];
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
    assert(strncmp("combat_robot_msgs.msg._operation_state.OperationState", full_classname_dest, 53) == 0);
  }
  combat_robot_msgs__msg__OperationState * ros_message = _ros_message;
  {  // state
    PyObject * field = PyObject_GetAttrString(_pymsg, "state");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->state = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // active_mode_id
    PyObject * field = PyObject_GetAttrString(_pymsg, "active_mode_id");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->active_mode_id = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // mission_status
    PyObject * field = PyObject_GetAttrString(_pymsg, "mission_status");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->mission_status = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // estop_active
    PyObject * field = PyObject_GetAttrString(_pymsg, "estop_active");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->estop_active = (Py_True == field);
    Py_DECREF(field);
  }
  {  // permission_request_active
    PyObject * field = PyObject_GetAttrString(_pymsg, "permission_request_active");
    if (!field) {
      return false;
    }
    assert(PyBool_Check(field));
    ros_message->permission_request_active = (Py_True == field);
    Py_DECREF(field);
  }
  {  // crosshair_x
    PyObject * field = PyObject_GetAttrString(_pymsg, "crosshair_x");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->crosshair_x = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // crosshair_y
    PyObject * field = PyObject_GetAttrString(_pymsg, "crosshair_y");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->crosshair_y = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // current_zoom_level
    PyObject * field = PyObject_GetAttrString(_pymsg, "current_zoom_level");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->current_zoom_level = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // gps_lat
    PyObject * field = PyObject_GetAttrString(_pymsg, "gps_lat");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->gps_lat = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // gps_lon
    PyObject * field = PyObject_GetAttrString(_pymsg, "gps_lon");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->gps_lon = PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // gps_heading
    PyObject * field = PyObject_GetAttrString(_pymsg, "gps_heading");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->gps_heading = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // current_speed_mps
    PyObject * field = PyObject_GetAttrString(_pymsg, "current_speed_mps");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->current_speed_mps = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // current_waypoint_index
    PyObject * field = PyObject_GetAttrString(_pymsg, "current_waypoint_index");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->current_waypoint_index = (uint16_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // total_waypoints
    PyObject * field = PyObject_GetAttrString(_pymsg, "total_waypoints");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->total_waypoints = (uint16_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }
  {  // progress_ratio
    PyObject * field = PyObject_GetAttrString(_pymsg, "progress_ratio");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->progress_ratio = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // distance_to_next_wp_m
    PyObject * field = PyObject_GetAttrString(_pymsg, "distance_to_next_wp_m");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->distance_to_next_wp_m = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // distance_to_goal_m
    PyObject * field = PyObject_GetAttrString(_pymsg, "distance_to_goal_m");
    if (!field) {
      return false;
    }
    assert(PyFloat_Check(field));
    ros_message->distance_to_goal_m = (float)PyFloat_AS_DOUBLE(field);
    Py_DECREF(field);
  }
  {  // error_code
    PyObject * field = PyObject_GetAttrString(_pymsg, "error_code");
    if (!field) {
      return false;
    }
    assert(PyLong_Check(field));
    ros_message->error_code = (uint8_t)PyLong_AsUnsignedLong(field);
    Py_DECREF(field);
  }

  return true;
}

ROSIDL_GENERATOR_C_EXPORT
PyObject * combat_robot_msgs__msg__operation_state__convert_to_py(void * raw_ros_message)
{
  /* NOTE(esteve): Call constructor of OperationState */
  PyObject * _pymessage = NULL;
  {
    PyObject * pymessage_module = PyImport_ImportModule("combat_robot_msgs.msg._operation_state");
    assert(pymessage_module);
    PyObject * pymessage_class = PyObject_GetAttrString(pymessage_module, "OperationState");
    assert(pymessage_class);
    Py_DECREF(pymessage_module);
    _pymessage = PyObject_CallObject(pymessage_class, NULL);
    Py_DECREF(pymessage_class);
    if (!_pymessage) {
      return NULL;
    }
  }
  combat_robot_msgs__msg__OperationState * ros_message = (combat_robot_msgs__msg__OperationState *)raw_ros_message;
  {  // state
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->state);
    {
      int rc = PyObject_SetAttrString(_pymessage, "state", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // active_mode_id
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->active_mode_id);
    {
      int rc = PyObject_SetAttrString(_pymessage, "active_mode_id", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // mission_status
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->mission_status);
    {
      int rc = PyObject_SetAttrString(_pymessage, "mission_status", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // estop_active
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->estop_active ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "estop_active", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // permission_request_active
    PyObject * field = NULL;
    field = PyBool_FromLong(ros_message->permission_request_active ? 1 : 0);
    {
      int rc = PyObject_SetAttrString(_pymessage, "permission_request_active", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // crosshair_x
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->crosshair_x);
    {
      int rc = PyObject_SetAttrString(_pymessage, "crosshair_x", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // crosshair_y
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->crosshair_y);
    {
      int rc = PyObject_SetAttrString(_pymessage, "crosshair_y", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // current_zoom_level
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->current_zoom_level);
    {
      int rc = PyObject_SetAttrString(_pymessage, "current_zoom_level", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // gps_lat
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->gps_lat);
    {
      int rc = PyObject_SetAttrString(_pymessage, "gps_lat", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // gps_lon
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->gps_lon);
    {
      int rc = PyObject_SetAttrString(_pymessage, "gps_lon", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // gps_heading
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->gps_heading);
    {
      int rc = PyObject_SetAttrString(_pymessage, "gps_heading", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // current_speed_mps
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->current_speed_mps);
    {
      int rc = PyObject_SetAttrString(_pymessage, "current_speed_mps", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // current_waypoint_index
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->current_waypoint_index);
    {
      int rc = PyObject_SetAttrString(_pymessage, "current_waypoint_index", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // total_waypoints
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->total_waypoints);
    {
      int rc = PyObject_SetAttrString(_pymessage, "total_waypoints", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // progress_ratio
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->progress_ratio);
    {
      int rc = PyObject_SetAttrString(_pymessage, "progress_ratio", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // distance_to_next_wp_m
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->distance_to_next_wp_m);
    {
      int rc = PyObject_SetAttrString(_pymessage, "distance_to_next_wp_m", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // distance_to_goal_m
    PyObject * field = NULL;
    field = PyFloat_FromDouble(ros_message->distance_to_goal_m);
    {
      int rc = PyObject_SetAttrString(_pymessage, "distance_to_goal_m", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }
  {  // error_code
    PyObject * field = NULL;
    field = PyLong_FromUnsignedLong(ros_message->error_code);
    {
      int rc = PyObject_SetAttrString(_pymessage, "error_code", field);
      Py_DECREF(field);
      if (rc) {
        return NULL;
      }
    }
  }

  // ownership of _pymessage is transferred to the caller
  return _pymessage;
}
