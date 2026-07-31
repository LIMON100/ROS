# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/PanTiltControlCommand.idl
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


class Metaclass_PanTiltControlCommand(type):
    """Metaclass of message 'PanTiltControlCommand'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'CONTROL_BRAKE': 0,
        'CONTROL_HOR_POS': 1,
        'CONTROL_VER_POS': 2,
        'CONTROL_DIR': 3,
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
                'combat_robot_msgs.msg.PanTiltControlCommand')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__pan_tilt_control_command
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__pan_tilt_control_command
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__pan_tilt_control_command
            cls._TYPE_SUPPORT = module.type_support_msg__msg__pan_tilt_control_command
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__pan_tilt_control_command

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'CONTROL_BRAKE': cls.__constants['CONTROL_BRAKE'],
            'CONTROL_HOR_POS': cls.__constants['CONTROL_HOR_POS'],
            'CONTROL_VER_POS': cls.__constants['CONTROL_VER_POS'],
            'CONTROL_DIR': cls.__constants['CONTROL_DIR'],
        }

    @property
    def CONTROL_BRAKE(self):
        """Message constant 'CONTROL_BRAKE'."""
        return Metaclass_PanTiltControlCommand.__constants['CONTROL_BRAKE']

    @property
    def CONTROL_HOR_POS(self):
        """Message constant 'CONTROL_HOR_POS'."""
        return Metaclass_PanTiltControlCommand.__constants['CONTROL_HOR_POS']

    @property
    def CONTROL_VER_POS(self):
        """Message constant 'CONTROL_VER_POS'."""
        return Metaclass_PanTiltControlCommand.__constants['CONTROL_VER_POS']

    @property
    def CONTROL_DIR(self):
        """Message constant 'CONTROL_DIR'."""
        return Metaclass_PanTiltControlCommand.__constants['CONTROL_DIR']


class PanTiltControlCommand(metaclass=Metaclass_PanTiltControlCommand):
    """
    Message class 'PanTiltControlCommand'.

    Constants:
      CONTROL_BRAKE
      CONTROL_HOR_POS
      CONTROL_VER_POS
      CONTROL_DIR
    """

    __slots__ = [
        '_header',
        '_control_mode',
        '_horizontal_angle',
        '_vertical_angle',
        '_pan_speed',
        '_tilt_speed',
        '_pan_dir',
        '_tilt_dir',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'control_mode': 'uint8',
        'horizontal_angle': 'float',
        'vertical_angle': 'float',
        'pan_speed': 'uint8',
        'tilt_speed': 'uint8',
        'pan_dir': 'uint8',
        'tilt_dir': 'uint8',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
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
        self.control_mode = kwargs.get('control_mode', int())
        self.horizontal_angle = kwargs.get('horizontal_angle', float())
        self.vertical_angle = kwargs.get('vertical_angle', float())
        self.pan_speed = kwargs.get('pan_speed', int())
        self.tilt_speed = kwargs.get('tilt_speed', int())
        self.pan_dir = kwargs.get('pan_dir', int())
        self.tilt_dir = kwargs.get('tilt_dir', int())

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
        if self.control_mode != other.control_mode:
            return False
        if self.horizontal_angle != other.horizontal_angle:
            return False
        if self.vertical_angle != other.vertical_angle:
            return False
        if self.pan_speed != other.pan_speed:
            return False
        if self.tilt_speed != other.tilt_speed:
            return False
        if self.pan_dir != other.pan_dir:
            return False
        if self.tilt_dir != other.tilt_dir:
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
    def control_mode(self):
        """Message field 'control_mode'."""
        return self._control_mode

    @control_mode.setter
    def control_mode(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'control_mode' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'control_mode' field must be an unsigned integer in [0, 255]"
        self._control_mode = value

    @builtins.property
    def horizontal_angle(self):
        """Message field 'horizontal_angle'."""
        return self._horizontal_angle

    @horizontal_angle.setter
    def horizontal_angle(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'horizontal_angle' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'horizontal_angle' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._horizontal_angle = value

    @builtins.property
    def vertical_angle(self):
        """Message field 'vertical_angle'."""
        return self._vertical_angle

    @vertical_angle.setter
    def vertical_angle(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'vertical_angle' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'vertical_angle' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._vertical_angle = value

    @builtins.property
    def pan_speed(self):
        """Message field 'pan_speed'."""
        return self._pan_speed

    @pan_speed.setter
    def pan_speed(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'pan_speed' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'pan_speed' field must be an unsigned integer in [0, 255]"
        self._pan_speed = value

    @builtins.property
    def tilt_speed(self):
        """Message field 'tilt_speed'."""
        return self._tilt_speed

    @tilt_speed.setter
    def tilt_speed(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'tilt_speed' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'tilt_speed' field must be an unsigned integer in [0, 255]"
        self._tilt_speed = value

    @builtins.property
    def pan_dir(self):
        """Message field 'pan_dir'."""
        return self._pan_dir

    @pan_dir.setter
    def pan_dir(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'pan_dir' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'pan_dir' field must be an unsigned integer in [0, 255]"
        self._pan_dir = value

    @builtins.property
    def tilt_dir(self):
        """Message field 'tilt_dir'."""
        return self._tilt_dir

    @tilt_dir.setter
    def tilt_dir(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'tilt_dir' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'tilt_dir' field must be an unsigned integer in [0, 255]"
        self._tilt_dir = value
