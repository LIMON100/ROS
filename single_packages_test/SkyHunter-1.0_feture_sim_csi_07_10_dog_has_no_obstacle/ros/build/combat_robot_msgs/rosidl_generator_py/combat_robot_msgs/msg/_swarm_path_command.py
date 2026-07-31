# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/SwarmPathCommand.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_SwarmPathCommand(type):
    """Metaclass of message 'SwarmPathCommand'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'CMD_NONE': 0,
        'CMD_START': 1,
        'CMD_STOP': 2,
        'CMD_PAUSE': 3,
        'CMD_RESUME': 4,
        'CMD_LOAD_PATH': 5,
        'CMD_COMPLETE': 6,
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
                'combat_robot_msgs.msg.SwarmPathCommand')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__swarm_path_command
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__swarm_path_command
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__swarm_path_command
            cls._TYPE_SUPPORT = module.type_support_msg__msg__swarm_path_command
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__swarm_path_command

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'CMD_NONE': cls.__constants['CMD_NONE'],
            'CMD_START': cls.__constants['CMD_START'],
            'CMD_STOP': cls.__constants['CMD_STOP'],
            'CMD_PAUSE': cls.__constants['CMD_PAUSE'],
            'CMD_RESUME': cls.__constants['CMD_RESUME'],
            'CMD_LOAD_PATH': cls.__constants['CMD_LOAD_PATH'],
            'CMD_COMPLETE': cls.__constants['CMD_COMPLETE'],
        }

    @property
    def CMD_NONE(self):
        """Message constant 'CMD_NONE'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_NONE']

    @property
    def CMD_START(self):
        """Message constant 'CMD_START'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_START']

    @property
    def CMD_STOP(self):
        """Message constant 'CMD_STOP'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_STOP']

    @property
    def CMD_PAUSE(self):
        """Message constant 'CMD_PAUSE'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_PAUSE']

    @property
    def CMD_RESUME(self):
        """Message constant 'CMD_RESUME'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_RESUME']

    @property
    def CMD_LOAD_PATH(self):
        """Message constant 'CMD_LOAD_PATH'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_LOAD_PATH']

    @property
    def CMD_COMPLETE(self):
        """Message constant 'CMD_COMPLETE'."""
        return Metaclass_SwarmPathCommand.__constants['CMD_COMPLETE']


class SwarmPathCommand(metaclass=Metaclass_SwarmPathCommand):
    """
    Message class 'SwarmPathCommand'.

    Constants:
      CMD_NONE
      CMD_START
      CMD_STOP
      CMD_PAUSE
      CMD_RESUME
      CMD_LOAD_PATH
      CMD_COMPLETE
    """

    __slots__ = [
        '_header',
        '_command',
        '_num_waypoints',
        '_path_json',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'command': 'uint8',
        'num_waypoints': 'uint16',
        'path_json': 'string',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint16'),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
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
        self.command = kwargs.get('command', int())
        self.num_waypoints = kwargs.get('num_waypoints', int())
        self.path_json = kwargs.get('path_json', str())

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
        if self.command != other.command:
            return False
        if self.num_waypoints != other.num_waypoints:
            return False
        if self.path_json != other.path_json:
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
    def command(self):
        """Message field 'command'."""
        return self._command

    @command.setter
    def command(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'command' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'command' field must be an unsigned integer in [0, 255]"
        self._command = value

    @builtins.property
    def num_waypoints(self):
        """Message field 'num_waypoints'."""
        return self._num_waypoints

    @num_waypoints.setter
    def num_waypoints(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'num_waypoints' field must be of type 'int'"
            assert value >= 0 and value < 65536, \
                "The 'num_waypoints' field must be an unsigned integer in [0, 65535]"
        self._num_waypoints = value

    @builtins.property
    def path_json(self):
        """Message field 'path_json'."""
        return self._path_json

    @path_json.setter
    def path_json(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'path_json' field must be of type 'str'"
        self._path_json = value
