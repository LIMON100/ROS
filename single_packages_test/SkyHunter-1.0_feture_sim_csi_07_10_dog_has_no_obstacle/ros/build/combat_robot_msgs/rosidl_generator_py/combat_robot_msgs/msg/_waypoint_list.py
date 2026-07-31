# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/WaypointList.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_WaypointList(type):
    """Metaclass of message 'WaypointList'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
    }

    @classmethod
    def __import_type_support__(cls):
        try:
            from rosidl_generator_py import import_type_support
            module = import_type_support('combat_robot_msgs')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'combat_robot_msgs.msg.WaypointList')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__waypoint_list
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__waypoint_list
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__waypoint_list
            cls._TYPE_SUPPORT = module.type_support_msg__msg__waypoint_list
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__waypoint_list

            from combat_robot_msgs.msg import Waypoint
            if Waypoint.__class__._TYPE_SUPPORT is None:
                Waypoint.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
        }


class WaypointList(metaclass=Metaclass_WaypointList):
    """Message class 'WaypointList'."""

    __slots__ = [
        '_mode',
        '_formation',
        '_mission_id',
        '_mission_status',
        '_waypoints',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'mode': 'int32',
        'formation': 'int32',
        'mission_id': 'int32',
        'mission_status': 'int32',
        'waypoints': 'sequence<combat_robot_msgs/Waypoint>',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
        rosidl_parser.definition.UnboundedSequence(rosidl_parser.definition.NamespacedType(['combat_robot_msgs', 'msg'], 'Waypoint')),  # noqa: E501
    )

    def __init__(self, **kwargs):
        if 'check_fields' in kwargs:
            self._check_fields = kwargs['check_fields']
        else:
            self._check_fields = ros_python_check_fields == '1'
        if self._check_fields:
            assert all('_' + key in self.__slots__ for key in kwargs.keys()), \
                'Invalid arguments passed to constructor: %s' % \
                ', '.join(sorted(k for k in kwargs.keys() if '_' + k not in self.__slots__))
        self.mode = kwargs.get('mode', int())
        self.formation = kwargs.get('formation', int())
        self.mission_id = kwargs.get('mission_id', int())
        self.mission_status = kwargs.get('mission_status', int())
        self.waypoints = kwargs.get('waypoints', [])

    def __repr__(self):
        typename = self.__class__.__module__.split('.')
        typename.pop()
        typename.append(self.__class__.__name__)
        args = []
        for s, t in zip(self.get_fields_and_field_types().keys(), self.SLOT_TYPES):
            field = getattr(self, s)
            fieldstr = repr(field)
            # We use Python array type for fields that can be directly stored
            # in them, and "normal" sequences for everything else.  If it is
            # a type that we store in an array, strip off the 'array' portion.
            if (
                isinstance(t, rosidl_parser.definition.AbstractSequence) and
                isinstance(t.value_type, rosidl_parser.definition.BasicType) and
                t.value_type.typename in ['float', 'double', 'int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64']
            ):
                if len(field) == 0:
                    fieldstr = '[]'
                else:
                    if self._check_fields:
                        assert fieldstr.startswith('array(')
                    prefix = "array('X', "
                    suffix = ')'
                    fieldstr = fieldstr[len(prefix):-len(suffix)]
            args.append(s + '=' + fieldstr)
        return '%s(%s)' % ('.'.join(typename), ', '.join(args))

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        if self.mode != other.mode:
            return False
        if self.formation != other.formation:
            return False
        if self.mission_id != other.mission_id:
            return False
        if self.mission_status != other.mission_status:
            return False
        if self.waypoints != other.waypoints:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def mode(self):
        """Message field 'mode'."""
        return self._mode

    @mode.setter
    def mode(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'mode' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'mode' field must be an integer in [-2147483648, 2147483647]"
        self._mode = value

    @builtins.property
    def formation(self):
        """Message field 'formation'."""
        return self._formation

    @formation.setter
    def formation(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'formation' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'formation' field must be an integer in [-2147483648, 2147483647]"
        self._formation = value

    @builtins.property
    def mission_id(self):
        """Message field 'mission_id'."""
        return self._mission_id

    @mission_id.setter
    def mission_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'mission_id' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'mission_id' field must be an integer in [-2147483648, 2147483647]"
        self._mission_id = value

    @builtins.property
    def mission_status(self):
        """Message field 'mission_status'."""
        return self._mission_status

    @mission_status.setter
    def mission_status(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'mission_status' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'mission_status' field must be an integer in [-2147483648, 2147483647]"
        self._mission_status = value

    @builtins.property
    def waypoints(self):
        """Message field 'waypoints'."""
        return self._waypoints

    @waypoints.setter
    def waypoints(self, value):
        if self._check_fields:
            from combat_robot_msgs.msg import Waypoint
            from collections.abc import Sequence
            from collections.abc import Set
            from collections import UserList
            from collections import UserString
            assert \
                ((isinstance(value, Sequence) or
                  isinstance(value, Set) or
                  isinstance(value, UserList)) and
                 not isinstance(value, str) and
                 not isinstance(value, UserString) and
                 all(isinstance(v, Waypoint) for v in value) and
                 True), \
                "The 'waypoints' field must be a set or sequence and each value of type 'Waypoint'"
        self._waypoints = value
