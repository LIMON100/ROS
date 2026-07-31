# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/Waypoint.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import math  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_Waypoint(type):
    """Metaclass of message 'Waypoint'."""

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
                'combat_robot_msgs.msg.Waypoint')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__waypoint
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__waypoint
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__waypoint
            cls._TYPE_SUPPORT = module.type_support_msg__msg__waypoint
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__waypoint

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
        }


class Waypoint(metaclass=Metaclass_Waypoint):
    """Message class 'Waypoint'."""

    __slots__ = [
        '_way_id',
        '_way_lon',
        '_way_lat',
        '_way_status',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'way_id': 'int32',
        'way_lon': 'double',
        'way_lat': 'double',
        'way_status': 'int32',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
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
        self.way_id = kwargs.get('way_id', int())
        self.way_lon = kwargs.get('way_lon', float())
        self.way_lat = kwargs.get('way_lat', float())
        self.way_status = kwargs.get('way_status', int())

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
        if self.way_id != other.way_id:
            return False
        if self.way_lon != other.way_lon:
            return False
        if self.way_lat != other.way_lat:
            return False
        if self.way_status != other.way_status:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def way_id(self):
        """Message field 'way_id'."""
        return self._way_id

    @way_id.setter
    def way_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'way_id' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'way_id' field must be an integer in [-2147483648, 2147483647]"
        self._way_id = value

    @builtins.property
    def way_lon(self):
        """Message field 'way_lon'."""
        return self._way_lon

    @way_lon.setter
    def way_lon(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'way_lon' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'way_lon' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._way_lon = value

    @builtins.property
    def way_lat(self):
        """Message field 'way_lat'."""
        return self._way_lat

    @way_lat.setter
    def way_lat(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'way_lat' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'way_lat' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._way_lat = value

    @builtins.property
    def way_status(self):
        """Message field 'way_status'."""
        return self._way_status

    @way_status.setter
    def way_status(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'way_status' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'way_status' field must be an integer in [-2147483648, 2147483647]"
        self._way_status = value
