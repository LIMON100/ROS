# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/GnssStatus.idl
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


class Metaclass_GnssStatus(type):
    """Metaclass of message 'GnssStatus'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'FIX_NONE': 0,
        'FIX_2D': 1,
        'FIX_3D': 2,
        'FIX_DGPS': 3,
        'FIX_RTK_FLOAT': 4,
        'FIX_RTK_FIXED': 5,
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
                'combat_robot_msgs.msg.GnssStatus')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__gnss_status
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__gnss_status
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__gnss_status
            cls._TYPE_SUPPORT = module.type_support_msg__msg__gnss_status
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__gnss_status

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'FIX_NONE': cls.__constants['FIX_NONE'],
            'FIX_2D': cls.__constants['FIX_2D'],
            'FIX_3D': cls.__constants['FIX_3D'],
            'FIX_DGPS': cls.__constants['FIX_DGPS'],
            'FIX_RTK_FLOAT': cls.__constants['FIX_RTK_FLOAT'],
            'FIX_RTK_FIXED': cls.__constants['FIX_RTK_FIXED'],
        }

    @property
    def FIX_NONE(self):
        """Message constant 'FIX_NONE'."""
        return Metaclass_GnssStatus.__constants['FIX_NONE']

    @property
    def FIX_2D(self):
        """Message constant 'FIX_2D'."""
        return Metaclass_GnssStatus.__constants['FIX_2D']

    @property
    def FIX_3D(self):
        """Message constant 'FIX_3D'."""
        return Metaclass_GnssStatus.__constants['FIX_3D']

    @property
    def FIX_DGPS(self):
        """Message constant 'FIX_DGPS'."""
        return Metaclass_GnssStatus.__constants['FIX_DGPS']

    @property
    def FIX_RTK_FLOAT(self):
        """Message constant 'FIX_RTK_FLOAT'."""
        return Metaclass_GnssStatus.__constants['FIX_RTK_FLOAT']

    @property
    def FIX_RTK_FIXED(self):
        """Message constant 'FIX_RTK_FIXED'."""
        return Metaclass_GnssStatus.__constants['FIX_RTK_FIXED']


class GnssStatus(metaclass=Metaclass_GnssStatus):
    """
    Message class 'GnssStatus'.

    Constants:
      FIX_NONE
      FIX_2D
      FIX_3D
      FIX_DGPS
      FIX_RTK_FLOAT
      FIX_RTK_FIXED
    """

    __slots__ = [
        '_header',
        '_fix_status',
        '_num_satellites',
        '_latitude',
        '_longitude',
        '_altitude_m',
        '_heading_deg',
        '_ground_speed_mps',
        '_horizontal_accuracy_m',
        '_vertical_accuracy_m',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'fix_status': 'uint8',
        'num_satellites': 'uint8',
        'latitude': 'double',
        'longitude': 'double',
        'altitude_m': 'double',
        'heading_deg': 'float',
        'ground_speed_mps': 'float',
        'horizontal_accuracy_m': 'float',
        'vertical_accuracy_m': 'float',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
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
        from std_msgs.msg import Header
        self.header = kwargs.get('header', Header())
        self.fix_status = kwargs.get('fix_status', int())
        self.num_satellites = kwargs.get('num_satellites', int())
        self.latitude = kwargs.get('latitude', float())
        self.longitude = kwargs.get('longitude', float())
        self.altitude_m = kwargs.get('altitude_m', float())
        self.heading_deg = kwargs.get('heading_deg', float())
        self.ground_speed_mps = kwargs.get('ground_speed_mps', float())
        self.horizontal_accuracy_m = kwargs.get('horizontal_accuracy_m', float())
        self.vertical_accuracy_m = kwargs.get('vertical_accuracy_m', float())

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
        if self.header != other.header:
            return False
        if self.fix_status != other.fix_status:
            return False
        if self.num_satellites != other.num_satellites:
            return False
        if self.latitude != other.latitude:
            return False
        if self.longitude != other.longitude:
            return False
        if self.altitude_m != other.altitude_m:
            return False
        if self.heading_deg != other.heading_deg:
            return False
        if self.ground_speed_mps != other.ground_speed_mps:
            return False
        if self.horizontal_accuracy_m != other.horizontal_accuracy_m:
            return False
        if self.vertical_accuracy_m != other.vertical_accuracy_m:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def header(self):
        """Message field 'header'."""
        return self._header

    @header.setter
    def header(self, value):
        if self._check_fields:
            from std_msgs.msg import Header
            assert \
                isinstance(value, Header), \
                "The 'header' field must be a sub message of type 'Header'"
        self._header = value

    @builtins.property
    def fix_status(self):
        """Message field 'fix_status'."""
        return self._fix_status

    @fix_status.setter
    def fix_status(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'fix_status' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'fix_status' field must be an unsigned integer in [0, 255]"
        self._fix_status = value

    @builtins.property
    def num_satellites(self):
        """Message field 'num_satellites'."""
        return self._num_satellites

    @num_satellites.setter
    def num_satellites(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'num_satellites' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'num_satellites' field must be an unsigned integer in [0, 255]"
        self._num_satellites = value

    @builtins.property
    def latitude(self):
        """Message field 'latitude'."""
        return self._latitude

    @latitude.setter
    def latitude(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'latitude' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'latitude' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._latitude = value

    @builtins.property
    def longitude(self):
        """Message field 'longitude'."""
        return self._longitude

    @longitude.setter
    def longitude(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'longitude' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'longitude' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._longitude = value

    @builtins.property
    def altitude_m(self):
        """Message field 'altitude_m'."""
        return self._altitude_m

    @altitude_m.setter
    def altitude_m(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'altitude_m' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'altitude_m' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._altitude_m = value

    @builtins.property
    def heading_deg(self):
        """Message field 'heading_deg'."""
        return self._heading_deg

    @heading_deg.setter
    def heading_deg(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'heading_deg' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'heading_deg' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._heading_deg = value

    @builtins.property
    def ground_speed_mps(self):
        """Message field 'ground_speed_mps'."""
        return self._ground_speed_mps

    @ground_speed_mps.setter
    def ground_speed_mps(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'ground_speed_mps' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'ground_speed_mps' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._ground_speed_mps = value

    @builtins.property
    def horizontal_accuracy_m(self):
        """Message field 'horizontal_accuracy_m'."""
        return self._horizontal_accuracy_m

    @horizontal_accuracy_m.setter
    def horizontal_accuracy_m(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'horizontal_accuracy_m' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'horizontal_accuracy_m' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._horizontal_accuracy_m = value

    @builtins.property
    def vertical_accuracy_m(self):
        """Message field 'vertical_accuracy_m'."""
        return self._vertical_accuracy_m

    @vertical_accuracy_m.setter
    def vertical_accuracy_m(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'vertical_accuracy_m' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'vertical_accuracy_m' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._vertical_accuracy_m = value
