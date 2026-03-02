# generated from rosidl_generator_py/resource/_idl.py.em
# with input from skyhunter_msgs:msg/LeaderState.idl
# generated code does not contain a copyright notice


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_LeaderState(type):
    """Metaclass of message 'LeaderState'."""

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
                'skyhunter_msgs.msg.LeaderState')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__leader_state
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__leader_state
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__leader_state
            cls._TYPE_SUPPORT = module.type_support_msg__msg__leader_state
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__leader_state

            from geometry_msgs.msg import Pose
            if Pose.__class__._TYPE_SUPPORT is None:
                Pose.__class__.__import_type_support__()

            from geometry_msgs.msg import Twist
            if Twist.__class__._TYPE_SUPPORT is None:
                Twist.__class__.__import_type_support__()

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


class LeaderState(metaclass=Metaclass_LeaderState):
    """Message class 'LeaderState'."""

    __slots__ = [
        '_header',
        '_pose',
        '_velocity',
        '_next_waypoints',
        '_formation_mode',
        '_formation_state',
        '_swarm_state',
        '_formation_type',
        '_current_waypoint_index',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'pose': 'geometry_msgs/Pose',
        'velocity': 'geometry_msgs/Twist',
        'next_waypoints': 'sequence<geometry_msgs/Pose>',
        'formation_mode': 'uint8',
        'formation_state': 'uint8',
        'swarm_state': 'int8',
        'formation_type': 'int8',
        'current_waypoint_index': 'int32',
    }

    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.NamespacedType(['geometry_msgs', 'msg'], 'Pose'),  # noqa: E501
        rosidl_parser.definition.NamespacedType(['geometry_msgs', 'msg'], 'Twist'),  # noqa: E501
        rosidl_parser.definition.UnboundedSequence(rosidl_parser.definition.NamespacedType(['geometry_msgs', 'msg'], 'Pose')),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('int8'),  # noqa: E501
        rosidl_parser.definition.BasicType('int8'),  # noqa: E501
        rosidl_parser.definition.BasicType('int32'),  # noqa: E501
    )

    def __init__(self, **kwargs):
        assert all('_' + key in self.__slots__ for key in kwargs.keys()), \
            'Invalid arguments passed to constructor: %s' % \
            ', '.join(sorted(k for k in kwargs.keys() if '_' + k not in self.__slots__))
        from std_msgs.msg import Header
        self.header = kwargs.get('header', Header())
        from geometry_msgs.msg import Pose
        self.pose = kwargs.get('pose', Pose())
        from geometry_msgs.msg import Twist
        self.velocity = kwargs.get('velocity', Twist())
        self.next_waypoints = kwargs.get('next_waypoints', [])
        self.formation_mode = kwargs.get('formation_mode', int())
        self.formation_state = kwargs.get('formation_state', int())
        self.swarm_state = kwargs.get('swarm_state', int())
        self.formation_type = kwargs.get('formation_type', int())
        self.current_waypoint_index = kwargs.get('current_waypoint_index', int())

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
        if self.pose != other.pose:
            return False
        if self.velocity != other.velocity:
            return False
        if self.next_waypoints != other.next_waypoints:
            return False
        if self.formation_mode != other.formation_mode:
            return False
        if self.formation_state != other.formation_state:
            return False
        if self.swarm_state != other.swarm_state:
            return False
        if self.formation_type != other.formation_type:
            return False
        if self.current_waypoint_index != other.current_waypoint_index:
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
    def pose(self):
        """Message field 'pose'."""
        return self._pose

    @pose.setter
    def pose(self, value):
        if __debug__:
            from geometry_msgs.msg import Pose
            assert \
                isinstance(value, Pose), \
                "The 'pose' field must be a sub message of type 'Pose'"
        self._pose = value

    @builtins.property
    def velocity(self):
        """Message field 'velocity'."""
        return self._velocity

    @velocity.setter
    def velocity(self, value):
        if __debug__:
            from geometry_msgs.msg import Twist
            assert \
                isinstance(value, Twist), \
                "The 'velocity' field must be a sub message of type 'Twist'"
        self._velocity = value

    @builtins.property
    def next_waypoints(self):
        """Message field 'next_waypoints'."""
        return self._next_waypoints

    @next_waypoints.setter
    def next_waypoints(self, value):
        if __debug__:
            from geometry_msgs.msg import Pose
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
                 all(isinstance(v, Pose) for v in value) and
                 True), \
                "The 'next_waypoints' field must be a set or sequence and each value of type 'Pose'"
        self._next_waypoints = value

    @builtins.property
    def formation_mode(self):
        """Message field 'formation_mode'."""
        return self._formation_mode

    @formation_mode.setter
    def formation_mode(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'formation_mode' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'formation_mode' field must be an unsigned integer in [0, 255]"
        self._formation_mode = value

    @builtins.property
    def formation_state(self):
        """Message field 'formation_state'."""
        return self._formation_state

    @formation_state.setter
    def formation_state(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'formation_state' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'formation_state' field must be an unsigned integer in [0, 255]"
        self._formation_state = value

    @builtins.property
    def swarm_state(self):
        """Message field 'swarm_state'."""
        return self._swarm_state

    @swarm_state.setter
    def swarm_state(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'swarm_state' field must be of type 'int'"
            assert value >= -128 and value < 128, \
                "The 'swarm_state' field must be an integer in [-128, 127]"
        self._swarm_state = value

    @builtins.property
    def formation_type(self):
        """Message field 'formation_type'."""
        return self._formation_type

    @formation_type.setter
    def formation_type(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'formation_type' field must be of type 'int'"
            assert value >= -128 and value < 128, \
                "The 'formation_type' field must be an integer in [-128, 127]"
        self._formation_type = value

    @builtins.property
    def current_waypoint_index(self):
        """Message field 'current_waypoint_index'."""
        return self._current_waypoint_index

    @current_waypoint_index.setter
    def current_waypoint_index(self, value):
        if __debug__:
            assert \
                isinstance(value, int), \
                "The 'current_waypoint_index' field must be of type 'int'"
            assert value >= -2147483648 and value < 2147483648, \
                "The 'current_waypoint_index' field must be an integer in [-2147483648, 2147483647]"
        self._current_waypoint_index = value
