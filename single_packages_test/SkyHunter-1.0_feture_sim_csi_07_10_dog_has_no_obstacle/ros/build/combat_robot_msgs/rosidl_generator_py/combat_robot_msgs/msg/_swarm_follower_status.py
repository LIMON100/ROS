# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
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


class Metaclass_SwarmFollowerStatus(type):
    """Metaclass of message 'SwarmFollowerStatus'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'LINK_DISCONNECTED': 0,
        'LINK_CONNECTED': 1,
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
                'combat_robot_msgs.msg.SwarmFollowerStatus')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__swarm_follower_status
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__swarm_follower_status
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__swarm_follower_status
            cls._TYPE_SUPPORT = module.type_support_msg__msg__swarm_follower_status
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__swarm_follower_status

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'LINK_DISCONNECTED': cls.__constants['LINK_DISCONNECTED'],
            'LINK_CONNECTED': cls.__constants['LINK_CONNECTED'],
        }

    @property
    def LINK_DISCONNECTED(self):
        """Message constant 'LINK_DISCONNECTED'."""
        return Metaclass_SwarmFollowerStatus.__constants['LINK_DISCONNECTED']

    @property
    def LINK_CONNECTED(self):
        """Message constant 'LINK_CONNECTED'."""
        return Metaclass_SwarmFollowerStatus.__constants['LINK_CONNECTED']


class SwarmFollowerStatus(metaclass=Metaclass_SwarmFollowerStatus):
    """
    Message class 'SwarmFollowerStatus'.

    Constants:
      LINK_DISCONNECTED
      LINK_CONNECTED
    """

    __slots__ = [
        '_header',
        '_robot_id',
        '_leader_robot_id',
        '_link_status',
        '_last_heartbeat_sequence',
        '_heartbeat_age_sec',
        '_last_operation_mode',
        '_last_formation_type',
        '_last_formation_number',
        '_last_grouping_index',
        '_latitude',
        '_longitude',
        '_heading_deg',
        '_ground_speed_mps',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'robot_id': 'uint32',
        'leader_robot_id': 'uint32',
        'link_status': 'uint8',
        'last_heartbeat_sequence': 'uint32',
        'heartbeat_age_sec': 'float',
        'last_operation_mode': 'uint8',
        'last_formation_type': 'uint8',
        'last_formation_number': 'uint8',
        'last_grouping_index': 'uint8',
        'latitude': 'double',
        'longitude': 'double',
        'heading_deg': 'float',
        'ground_speed_mps': 'float',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
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
        self.robot_id = kwargs.get('robot_id', int())
        self.leader_robot_id = kwargs.get('leader_robot_id', int())
        self.link_status = kwargs.get('link_status', int())
        self.last_heartbeat_sequence = kwargs.get('last_heartbeat_sequence', int())
        self.heartbeat_age_sec = kwargs.get('heartbeat_age_sec', float())
        self.last_operation_mode = kwargs.get('last_operation_mode', int())
        self.last_formation_type = kwargs.get('last_formation_type', int())
        self.last_formation_number = kwargs.get('last_formation_number', int())
        self.last_grouping_index = kwargs.get('last_grouping_index', int())
        self.latitude = kwargs.get('latitude', float())
        self.longitude = kwargs.get('longitude', float())
        self.heading_deg = kwargs.get('heading_deg', float())
        self.ground_speed_mps = kwargs.get('ground_speed_mps', float())

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
        if self.robot_id != other.robot_id:
            return False
        if self.leader_robot_id != other.leader_robot_id:
            return False
        if self.link_status != other.link_status:
            return False
        if self.last_heartbeat_sequence != other.last_heartbeat_sequence:
            return False
        if self.heartbeat_age_sec != other.heartbeat_age_sec:
            return False
        if self.last_operation_mode != other.last_operation_mode:
            return False
        if self.last_formation_type != other.last_formation_type:
            return False
        if self.last_formation_number != other.last_formation_number:
            return False
        if self.last_grouping_index != other.last_grouping_index:
            return False
        if self.latitude != other.latitude:
            return False
        if self.longitude != other.longitude:
            return False
        if self.heading_deg != other.heading_deg:
            return False
        if self.ground_speed_mps != other.ground_speed_mps:
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
    def robot_id(self):
        """Message field 'robot_id'."""
        return self._robot_id

    @robot_id.setter
    def robot_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'robot_id' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'robot_id' field must be an unsigned integer in [0, 4294967295]"
        self._robot_id = value

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
    def link_status(self):
        """Message field 'link_status'."""
        return self._link_status

    @link_status.setter
    def link_status(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'link_status' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'link_status' field must be an unsigned integer in [0, 255]"
        self._link_status = value

    @builtins.property
    def last_heartbeat_sequence(self):
        """Message field 'last_heartbeat_sequence'."""
        return self._last_heartbeat_sequence

    @last_heartbeat_sequence.setter
    def last_heartbeat_sequence(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'last_heartbeat_sequence' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'last_heartbeat_sequence' field must be an unsigned integer in [0, 4294967295]"
        self._last_heartbeat_sequence = value

    @builtins.property
    def heartbeat_age_sec(self):
        """Message field 'heartbeat_age_sec'."""
        return self._heartbeat_age_sec

    @heartbeat_age_sec.setter
    def heartbeat_age_sec(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'heartbeat_age_sec' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'heartbeat_age_sec' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._heartbeat_age_sec = value

    @builtins.property
    def last_operation_mode(self):
        """Message field 'last_operation_mode'."""
        return self._last_operation_mode

    @last_operation_mode.setter
    def last_operation_mode(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'last_operation_mode' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'last_operation_mode' field must be an unsigned integer in [0, 255]"
        self._last_operation_mode = value

    @builtins.property
    def last_formation_type(self):
        """Message field 'last_formation_type'."""
        return self._last_formation_type

    @last_formation_type.setter
    def last_formation_type(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'last_formation_type' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'last_formation_type' field must be an unsigned integer in [0, 255]"
        self._last_formation_type = value

    @builtins.property
    def last_formation_number(self):
        """Message field 'last_formation_number'."""
        return self._last_formation_number

    @last_formation_number.setter
    def last_formation_number(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'last_formation_number' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'last_formation_number' field must be an unsigned integer in [0, 255]"
        self._last_formation_number = value

    @builtins.property
    def last_grouping_index(self):
        """Message field 'last_grouping_index'."""
        return self._last_grouping_index

    @last_grouping_index.setter
    def last_grouping_index(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'last_grouping_index' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'last_grouping_index' field must be an unsigned integer in [0, 255]"
        self._last_grouping_index = value

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
