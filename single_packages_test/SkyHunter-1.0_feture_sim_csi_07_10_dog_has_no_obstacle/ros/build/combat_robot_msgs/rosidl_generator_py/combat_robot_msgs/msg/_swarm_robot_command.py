# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/SwarmRobotCommand.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

# Member 'selected_robot_ids'
import numpy  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_SwarmRobotCommand(type):
    """Metaclass of message 'SwarmRobotCommand'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'COMMAND_NONE': 0,
        'COMMAND_MODE': 1,
        'COMMAND_PATH': 2,
        'COMMAND_FORMATION': 3,
        'COMMAND_SYNC': 4,
        'PATH_CMD_NONE': 0,
        'PATH_CMD_START': 1,
        'PATH_CMD_STOP': 2,
        'PATH_CMD_PAUSE': 3,
        'PATH_CMD_RESUME': 4,
        'PATH_CMD_LOAD_PATH': 5,
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
                'combat_robot_msgs.msg.SwarmRobotCommand')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__swarm_robot_command
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__swarm_robot_command
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__swarm_robot_command
            cls._TYPE_SUPPORT = module.type_support_msg__msg__swarm_robot_command
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__swarm_robot_command

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'COMMAND_NONE': cls.__constants['COMMAND_NONE'],
            'COMMAND_MODE': cls.__constants['COMMAND_MODE'],
            'COMMAND_PATH': cls.__constants['COMMAND_PATH'],
            'COMMAND_FORMATION': cls.__constants['COMMAND_FORMATION'],
            'COMMAND_SYNC': cls.__constants['COMMAND_SYNC'],
            'PATH_CMD_NONE': cls.__constants['PATH_CMD_NONE'],
            'PATH_CMD_START': cls.__constants['PATH_CMD_START'],
            'PATH_CMD_STOP': cls.__constants['PATH_CMD_STOP'],
            'PATH_CMD_PAUSE': cls.__constants['PATH_CMD_PAUSE'],
            'PATH_CMD_RESUME': cls.__constants['PATH_CMD_RESUME'],
            'PATH_CMD_LOAD_PATH': cls.__constants['PATH_CMD_LOAD_PATH'],
        }

    @property
    def COMMAND_NONE(self):
        """Message constant 'COMMAND_NONE'."""
        return Metaclass_SwarmRobotCommand.__constants['COMMAND_NONE']

    @property
    def COMMAND_MODE(self):
        """Message constant 'COMMAND_MODE'."""
        return Metaclass_SwarmRobotCommand.__constants['COMMAND_MODE']

    @property
    def COMMAND_PATH(self):
        """Message constant 'COMMAND_PATH'."""
        return Metaclass_SwarmRobotCommand.__constants['COMMAND_PATH']

    @property
    def COMMAND_FORMATION(self):
        """Message constant 'COMMAND_FORMATION'."""
        return Metaclass_SwarmRobotCommand.__constants['COMMAND_FORMATION']

    @property
    def COMMAND_SYNC(self):
        """Message constant 'COMMAND_SYNC'."""
        return Metaclass_SwarmRobotCommand.__constants['COMMAND_SYNC']

    @property
    def PATH_CMD_NONE(self):
        """Message constant 'PATH_CMD_NONE'."""
        return Metaclass_SwarmRobotCommand.__constants['PATH_CMD_NONE']

    @property
    def PATH_CMD_START(self):
        """Message constant 'PATH_CMD_START'."""
        return Metaclass_SwarmRobotCommand.__constants['PATH_CMD_START']

    @property
    def PATH_CMD_STOP(self):
        """Message constant 'PATH_CMD_STOP'."""
        return Metaclass_SwarmRobotCommand.__constants['PATH_CMD_STOP']

    @property
    def PATH_CMD_PAUSE(self):
        """Message constant 'PATH_CMD_PAUSE'."""
        return Metaclass_SwarmRobotCommand.__constants['PATH_CMD_PAUSE']

    @property
    def PATH_CMD_RESUME(self):
        """Message constant 'PATH_CMD_RESUME'."""
        return Metaclass_SwarmRobotCommand.__constants['PATH_CMD_RESUME']

    @property
    def PATH_CMD_LOAD_PATH(self):
        """Message constant 'PATH_CMD_LOAD_PATH'."""
        return Metaclass_SwarmRobotCommand.__constants['PATH_CMD_LOAD_PATH']


class SwarmRobotCommand(metaclass=Metaclass_SwarmRobotCommand):
    """
    Message class 'SwarmRobotCommand'.

    Constants:
      COMMAND_NONE
      COMMAND_MODE
      COMMAND_PATH
      COMMAND_FORMATION
      COMMAND_SYNC
      PATH_CMD_NONE
      PATH_CMD_START
      PATH_CMD_STOP
      PATH_CMD_PAUSE
      PATH_CMD_RESUME
      PATH_CMD_LOAD_PATH
    """

    __slots__ = [
        '_header',
        '_sequence',
        '_command_type',
        '_leader_robot_id',
        '_target_robot_id',
        '_operation_mode',
        '_estop_requested',
        '_path_command',
        '_num_waypoints',
        '_path_id',
        '_path_json',
        '_formation_type',
        '_formation_number',
        '_grouping_index',
        '_slot_index',
        '_selected_robot_count',
        '_selected_robot_ids',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'sequence': 'uint32',
        'command_type': 'uint8',
        'leader_robot_id': 'uint32',
        'target_robot_id': 'uint32',
        'operation_mode': 'uint8',
        'estop_requested': 'boolean',
        'path_command': 'uint8',
        'num_waypoints': 'uint16',
        'path_id': 'string',
        'path_json': 'string',
        'formation_type': 'uint8',
        'formation_number': 'uint8',
        'grouping_index': 'uint8',
        'slot_index': 'uint8',
        'selected_robot_count': 'uint8',
        'selected_robot_ids': 'uint32[8]',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint16'),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.Array(rosidl_parser.definition.BasicType('uint32'), 8),  # noqa: E501
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
        self.sequence = kwargs.get('sequence', int())
        self.command_type = kwargs.get('command_type', int())
        self.leader_robot_id = kwargs.get('leader_robot_id', int())
        self.target_robot_id = kwargs.get('target_robot_id', int())
        self.operation_mode = kwargs.get('operation_mode', int())
        self.estop_requested = kwargs.get('estop_requested', bool())
        self.path_command = kwargs.get('path_command', int())
        self.num_waypoints = kwargs.get('num_waypoints', int())
        self.path_id = kwargs.get('path_id', str())
        self.path_json = kwargs.get('path_json', str())
        self.formation_type = kwargs.get('formation_type', int())
        self.formation_number = kwargs.get('formation_number', int())
        self.grouping_index = kwargs.get('grouping_index', int())
        self.slot_index = kwargs.get('slot_index', int())
        self.selected_robot_count = kwargs.get('selected_robot_count', int())
        if 'selected_robot_ids' not in kwargs:
            self.selected_robot_ids = numpy.zeros(8, dtype=numpy.uint32)
        else:
            self.selected_robot_ids = kwargs.get('selected_robot_ids')

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
        if self.sequence != other.sequence:
            return False
        if self.command_type != other.command_type:
            return False
        if self.leader_robot_id != other.leader_robot_id:
            return False
        if self.target_robot_id != other.target_robot_id:
            return False
        if self.operation_mode != other.operation_mode:
            return False
        if self.estop_requested != other.estop_requested:
            return False
        if self.path_command != other.path_command:
            return False
        if self.num_waypoints != other.num_waypoints:
            return False
        if self.path_id != other.path_id:
            return False
        if self.path_json != other.path_json:
            return False
        if self.formation_type != other.formation_type:
            return False
        if self.formation_number != other.formation_number:
            return False
        if self.grouping_index != other.grouping_index:
            return False
        if self.slot_index != other.slot_index:
            return False
        if self.selected_robot_count != other.selected_robot_count:
            return False
        if any(self.selected_robot_ids != other.selected_robot_ids):
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
    def sequence(self):
        """Message field 'sequence'."""
        return self._sequence

    @sequence.setter
    def sequence(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'sequence' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'sequence' field must be an unsigned integer in [0, 4294967295]"
        self._sequence = value

    @builtins.property
    def command_type(self):
        """Message field 'command_type'."""
        return self._command_type

    @command_type.setter
    def command_type(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'command_type' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'command_type' field must be an unsigned integer in [0, 255]"
        self._command_type = value

    @builtins.property
    def leader_robot_id(self):
        """Message field 'leader_robot_id'."""
        return self._leader_robot_id

    @leader_robot_id.setter
    def leader_robot_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'leader_robot_id' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'leader_robot_id' field must be an unsigned integer in [0, 4294967295]"
        self._leader_robot_id = value

    @builtins.property
    def target_robot_id(self):
        """Message field 'target_robot_id'."""
        return self._target_robot_id

    @target_robot_id.setter
    def target_robot_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'target_robot_id' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'target_robot_id' field must be an unsigned integer in [0, 4294967295]"
        self._target_robot_id = value

    @builtins.property
    def operation_mode(self):
        """Message field 'operation_mode'."""
        return self._operation_mode

    @operation_mode.setter
    def operation_mode(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'operation_mode' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'operation_mode' field must be an unsigned integer in [0, 255]"
        self._operation_mode = value

    @builtins.property
    def estop_requested(self):
        """Message field 'estop_requested'."""
        return self._estop_requested

    @estop_requested.setter
    def estop_requested(self, value):
        if self._check_fields:
            assert \
                isinstance(value, bool), \
                "The 'estop_requested' field must be of type 'bool'"
        self._estop_requested = value

    @builtins.property
    def path_command(self):
        """Message field 'path_command'."""
        return self._path_command

    @path_command.setter
    def path_command(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'path_command' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'path_command' field must be an unsigned integer in [0, 255]"
        self._path_command = value

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
    def path_id(self):
        """Message field 'path_id'."""
        return self._path_id

    @path_id.setter
    def path_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'path_id' field must be of type 'str'"
        self._path_id = value

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

    @builtins.property
    def formation_type(self):
        """Message field 'formation_type'."""
        return self._formation_type

    @formation_type.setter
    def formation_type(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'formation_type' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'formation_type' field must be an unsigned integer in [0, 255]"
        self._formation_type = value

    @builtins.property
    def formation_number(self):
        """Message field 'formation_number'."""
        return self._formation_number

    @formation_number.setter
    def formation_number(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'formation_number' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'formation_number' field must be an unsigned integer in [0, 255]"
        self._formation_number = value

    @builtins.property
    def grouping_index(self):
        """Message field 'grouping_index'."""
        return self._grouping_index

    @grouping_index.setter
    def grouping_index(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'grouping_index' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'grouping_index' field must be an unsigned integer in [0, 255]"
        self._grouping_index = value

    @builtins.property
    def slot_index(self):
        """Message field 'slot_index'."""
        return self._slot_index

    @slot_index.setter
    def slot_index(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'slot_index' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'slot_index' field must be an unsigned integer in [0, 255]"
        self._slot_index = value

    @builtins.property
    def selected_robot_count(self):
        """Message field 'selected_robot_count'."""
        return self._selected_robot_count

    @selected_robot_count.setter
    def selected_robot_count(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'selected_robot_count' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'selected_robot_count' field must be an unsigned integer in [0, 255]"
        self._selected_robot_count = value

    @builtins.property
    def selected_robot_ids(self):
        """Message field 'selected_robot_ids'."""
        return self._selected_robot_ids

    @selected_robot_ids.setter
    def selected_robot_ids(self, value):
        if self._check_fields:
            if isinstance(value, numpy.ndarray):
                assert value.dtype == numpy.uint32, \
                    "The 'selected_robot_ids' numpy.ndarray() must have the dtype of 'numpy.uint32'"
                assert value.size == 8, \
                    "The 'selected_robot_ids' numpy.ndarray() must have a size of 8"
                self._selected_robot_ids = value
                return
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
                 len(value) == 8 and
                 all(isinstance(v, int) for v in value) and
                 all(val >= 0 and val < 4294967296 for val in value)), \
                "The 'selected_robot_ids' field must be a set or sequence with length 8 and each value of type 'int' and each unsigned integer in [0, 4294967295]"
        self._selected_robot_ids = numpy.array(value, dtype=numpy.uint32)
