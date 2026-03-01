# generated from rosidl_generator_py/resource/_idl.py.em
# with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
# generated code does not contain a copyright notice


# Import statements for member types

import builtins  # noqa: E402, I100

import math  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_SwarmHeartbeat(type):
    """Metaclass of message 'SwarmHeartbeat'."""

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
            module = import_type_support('skyhunter_msgs')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'skyhunter_msgs.msg.SwarmHeartbeat')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__swarm_heartbeat
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__swarm_heartbeat
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__swarm_heartbeat
            cls._TYPE_SUPPORT = module.type_support_msg__msg__swarm_heartbeat
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__swarm_heartbeat

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


class SwarmHeartbeat(metaclass=Metaclass_SwarmHeartbeat):
    """Message class 'SwarmHeartbeat'."""

    __slots__ = [
        '_header',
        '_robot_id',
        '_term',
        '_is_leader',
        '_battery_level',
        '_leader_id_num',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'robot_id': 'string',
        'term': 'uint32',
        'is_leader': 'boolean',
        'battery_level': 'float',
        'leader_id_num': 'int32',
    }

    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
    )

    def __init__(self, **kwargs):
        assert all('_' + key in self.__slots__ for key in kwargs.keys()), \
            'Invalid arguments passed to constructor: %s' % \
            ', '.join(sorted(k for k in kwargs.keys() if '_' + k not in self.__slots__))
        from std_msgs.msg import Header
        self.header = kwargs.get('header', Header())
        self.robot_id = kwargs.get('robot_id', str())
        self.term = kwargs.get('term', int())
        self.is_leader = kwargs.get('is_leader', bool())
        self.battery_level = kwargs.get('battery_level', float())
        self.leader_id_num = kwargs.get('leader_id_num', int())

    def __repr__(self):
        typename = self.__class__.__module__.split('.')
        typename.pop()
        typename.append(self.__class__.__name__)
        args = []
        for s, t in zip(self.__slots__, self.SLOT_TYPES):
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
                    assert fieldstr.startswith('array(')
                    prefix = "array('X', "
                    suffix = ')'
                    fieldstr = fieldstr[len(prefix):-len(suffix)]
            args.append(s[1:] + '=' + fieldstr)
        return '%s(%s)' % ('.'.join(typename), ', '.join(args))

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        if self.header != other.header:
            return False
        if self.robot_id != other.robot_id:
            return False
        if self.term != other.term:
            return False
        if self.is_leader != other.is_leader:
            return False
        if self.battery_level != other.battery_level:
            return False
        if self.leader_id_num != other.leader_id_num:
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
        if __debug__:
            from std_msgs.msg import Header
            assert \
                isinstance(value, Header), \
                "The 'header' field must be a sub message of type 'Header'"
        self._header = value

    @builtins.property
    def robot_id(self):
        """Message field 'robot_id'."""
        return self._robot_id

    @robot_id.setter
    def robot_id(self, value):
        if __debug__:
            assert \
                isinstance(value, str), \
                "The 'robot_id' field must be of type 'str'"
        self._robot_id = value

    @builtins.property
    def term(self):
        """Message field 'term'."""
        return self._term

    @term.setter
    def term(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'term' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'term' field must be an unsigned integer in [0, 4294967295]"
        self._term = value

    @builtins.property
    def is_leader(self):
        """Message field 'is_leader'."""
        return self._is_leader

    @is_leader.setter
    def is_leader(self, value):
        if __debug__:
            assert \
                isinstance(value, bool), \
                "The 'is_leader' field must be of type 'bool'"
        self._is_leader = value

    @builtins.property
    def battery_level(self):
        """Message field 'battery_level'."""
        return self._battery_level

    @battery_level.setter
    def battery_level(self, value):
        if __debug__:
            assert \
                isinstance(value, float), \
                "The 'battery_level' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'battery_level' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._battery_level = value

    @builtins.property
    def leader_id_num(self):
        """Message field 'leader_id_num'."""
        return self._leader_id_num

    @leader_id_num.setter
    def leader_id_num(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'leader_id_num' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'leader_id_num' field must be an integer in [-2147483648, 2147483647]"
        self._leader_id_num = value
