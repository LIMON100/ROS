# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/StreamControlCommand.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_StreamControlCommand(type):
    """Metaclass of message 'StreamControlCommand'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'STREAM_NONE': 0,
        'STREAM_START': 1,
        'STREAM_STOP': 2,
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
                'combat_robot_msgs.msg.StreamControlCommand')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__stream_control_command
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__stream_control_command
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__stream_control_command
            cls._TYPE_SUPPORT = module.type_support_msg__msg__stream_control_command
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__stream_control_command

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'STREAM_NONE': cls.__constants['STREAM_NONE'],
            'STREAM_START': cls.__constants['STREAM_START'],
            'STREAM_STOP': cls.__constants['STREAM_STOP'],
        }

    @property
    def STREAM_NONE(self):
        """Message constant 'STREAM_NONE'."""
        return Metaclass_StreamControlCommand.__constants['STREAM_NONE']

    @property
    def STREAM_START(self):
        """Message constant 'STREAM_START'."""
        return Metaclass_StreamControlCommand.__constants['STREAM_START']

    @property
    def STREAM_STOP(self):
        """Message constant 'STREAM_STOP'."""
        return Metaclass_StreamControlCommand.__constants['STREAM_STOP']


class StreamControlCommand(metaclass=Metaclass_StreamControlCommand):
    """
    Message class 'StreamControlCommand'.

    Constants:
      STREAM_NONE
      STREAM_START
      STREAM_STOP
    """

    __slots__ = [
        '_header',
        '_stream_command',
        '_stream_target_robot_id',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'stream_command': 'uint8',
        'stream_target_robot_id': 'uint32',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
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
        self.stream_command = kwargs.get('stream_command', int())
        self.stream_target_robot_id = kwargs.get('stream_target_robot_id', int())

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
        if self.stream_command != other.stream_command:
            return False
        if self.stream_target_robot_id != other.stream_target_robot_id:
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
    def stream_command(self):
        """Message field 'stream_command'."""
        return self._stream_command

    @stream_command.setter
    def stream_command(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'stream_command' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'stream_command' field must be an unsigned integer in [0, 255]"
        self._stream_command = value

    @builtins.property
    def stream_target_robot_id(self):
        """Message field 'stream_target_robot_id'."""
        return self._stream_target_robot_id

    @stream_target_robot_id.setter
    def stream_target_robot_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'stream_target_robot_id' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'stream_target_robot_id' field must be an unsigned integer in [0, 4294967295]"
        self._stream_target_robot_id = value
