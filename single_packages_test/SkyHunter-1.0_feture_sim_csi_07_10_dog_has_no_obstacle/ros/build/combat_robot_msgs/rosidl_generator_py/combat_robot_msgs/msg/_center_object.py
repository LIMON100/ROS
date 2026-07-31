# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/CenterObject.idl
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


class Metaclass_CenterObject(type):
    """Metaclass of message 'CenterObject'."""

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
                'combat_robot_msgs.msg.CenterObject')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__center_object
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__center_object
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__center_object
            cls._TYPE_SUPPORT = module.type_support_msg__msg__center_object
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__center_object

            from combat_robot_msgs.msg import BoundingBox2d
            if BoundingBox2d.__class__._TYPE_SUPPORT is None:
                BoundingBox2d.__class__.__import_type_support__()

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
        }


class CenterObject(metaclass=Metaclass_CenterObject):
    """Message class 'CenterObject'."""

    __slots__ = [
        '_header',
        '_class_id',
        '_bounding_box',
        '_target_x',
        '_target_y',
        '_laser_distance',
        '_zoom_level',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'class_id': 'int32',
        'bounding_box': 'combat_robot_msgs/BoundingBox2d',
        'target_x': 'float',
        'target_y': 'float',
        'laser_distance': 'float',
        'zoom_level': 'float',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
        rosidl_parser.definition.NamespacedType(['combat_robot_msgs', 'msg'], 'BoundingBox2d'),  # noqa: E501
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
        self.class_id = kwargs.get('class_id', int())
        from combat_robot_msgs.msg import BoundingBox2d
        self.bounding_box = kwargs.get('bounding_box', BoundingBox2d())
        self.target_x = kwargs.get('target_x', float())
        self.target_y = kwargs.get('target_y', float())
        self.laser_distance = kwargs.get('laser_distance', float())
        self.zoom_level = kwargs.get('zoom_level', float())

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
        if self.class_id != other.class_id:
            return False
        if self.bounding_box != other.bounding_box:
            return False
        if self.target_x != other.target_x:
            return False
        if self.target_y != other.target_y:
            return False
        if self.laser_distance != other.laser_distance:
            return False
        if self.zoom_level != other.zoom_level:
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
    def class_id(self):
        """Message field 'class_id'."""
        return self._class_id

    @class_id.setter
    def class_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'class_id' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'class_id' field must be an integer in [-2147483648, 2147483647]"
        self._class_id = value

    @builtins.property
    def bounding_box(self):
        """Message field 'bounding_box'."""
        return self._bounding_box

    @bounding_box.setter
    def bounding_box(self, value):
        if self._check_fields:
            from combat_robot_msgs.msg import BoundingBox2d
            assert \
                isinstance(value, BoundingBox2d), \
                "The 'bounding_box' field must be a sub message of type 'BoundingBox2d'"
        self._bounding_box = value

    @builtins.property
    def target_x(self):
        """Message field 'target_x'."""
        return self._target_x

    @target_x.setter
    def target_x(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'target_x' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'target_x' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._target_x = value

    @builtins.property
    def target_y(self):
        """Message field 'target_y'."""
        return self._target_y

    @target_y.setter
    def target_y(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'target_y' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'target_y' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._target_y = value

    @builtins.property
    def laser_distance(self):
        """Message field 'laser_distance'."""
        return self._laser_distance

    @laser_distance.setter
    def laser_distance(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'laser_distance' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'laser_distance' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._laser_distance = value

    @builtins.property
    def zoom_level(self):
        """Message field 'zoom_level'."""
        return self._zoom_level

    @zoom_level.setter
    def zoom_level(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'zoom_level' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'zoom_level' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._zoom_level = value
