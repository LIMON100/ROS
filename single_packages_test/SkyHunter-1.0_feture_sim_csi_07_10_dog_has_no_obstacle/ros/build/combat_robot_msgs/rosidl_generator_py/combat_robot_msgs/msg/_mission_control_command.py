# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/MissionControlCommand.idl
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


class Metaclass_MissionControlCommand(type):
    """Metaclass of message 'MissionControlCommand'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'IDLE': 0,
        'RECON': 1,
        'PROTECT_GENERAL': 2,
        'PROTECT_DRONE': 3,
        'DEBUG_ATTACK': 4,
        'DEBUG_TRACKING': 5,
        'ASSAULT': 6,
        'RETURN_TO_HOME': 7,
        'ATTACK_PERMISSION_NONE': 0,
        'ATTACK_PERMISSION_APPROVE': 1,
        'ATTACK_PERMISSION_DENY': 2,
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
                'combat_robot_msgs.msg.MissionControlCommand')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__mission_control_command
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__mission_control_command
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__mission_control_command
            cls._TYPE_SUPPORT = module.type_support_msg__msg__mission_control_command
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__mission_control_command

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'IDLE': cls.__constants['IDLE'],
            'RECON': cls.__constants['RECON'],
            'PROTECT_GENERAL': cls.__constants['PROTECT_GENERAL'],
            'PROTECT_DRONE': cls.__constants['PROTECT_DRONE'],
            'DEBUG_ATTACK': cls.__constants['DEBUG_ATTACK'],
            'DEBUG_TRACKING': cls.__constants['DEBUG_TRACKING'],
            'ASSAULT': cls.__constants['ASSAULT'],
            'RETURN_TO_HOME': cls.__constants['RETURN_TO_HOME'],
            'ATTACK_PERMISSION_NONE': cls.__constants['ATTACK_PERMISSION_NONE'],
            'ATTACK_PERMISSION_APPROVE': cls.__constants['ATTACK_PERMISSION_APPROVE'],
            'ATTACK_PERMISSION_DENY': cls.__constants['ATTACK_PERMISSION_DENY'],
        }

    @property
    def IDLE(self):
        """Message constant 'IDLE'."""
        return Metaclass_MissionControlCommand.__constants['IDLE']

    @property
    def RECON(self):
        """Message constant 'RECON'."""
        return Metaclass_MissionControlCommand.__constants['RECON']

    @property
    def PROTECT_GENERAL(self):
        """Message constant 'PROTECT_GENERAL'."""
        return Metaclass_MissionControlCommand.__constants['PROTECT_GENERAL']

    @property
    def PROTECT_DRONE(self):
        """Message constant 'PROTECT_DRONE'."""
        return Metaclass_MissionControlCommand.__constants['PROTECT_DRONE']

    @property
    def DEBUG_ATTACK(self):
        """Message constant 'DEBUG_ATTACK'."""
        return Metaclass_MissionControlCommand.__constants['DEBUG_ATTACK']

    @property
    def DEBUG_TRACKING(self):
        """Message constant 'DEBUG_TRACKING'."""
        return Metaclass_MissionControlCommand.__constants['DEBUG_TRACKING']

    @property
    def ASSAULT(self):
        """Message constant 'ASSAULT'."""
        return Metaclass_MissionControlCommand.__constants['ASSAULT']

    @property
    def RETURN_TO_HOME(self):
        """Message constant 'RETURN_TO_HOME'."""
        return Metaclass_MissionControlCommand.__constants['RETURN_TO_HOME']

    @property
    def ATTACK_PERMISSION_NONE(self):
        """Message constant 'ATTACK_PERMISSION_NONE'."""
        return Metaclass_MissionControlCommand.__constants['ATTACK_PERMISSION_NONE']

    @property
    def ATTACK_PERMISSION_APPROVE(self):
        """Message constant 'ATTACK_PERMISSION_APPROVE'."""
        return Metaclass_MissionControlCommand.__constants['ATTACK_PERMISSION_APPROVE']

    @property
    def ATTACK_PERMISSION_DENY(self):
        """Message constant 'ATTACK_PERMISSION_DENY'."""
        return Metaclass_MissionControlCommand.__constants['ATTACK_PERMISSION_DENY']


class MissionControlCommand(metaclass=Metaclass_MissionControlCommand):
    """
    Message class 'MissionControlCommand'.

    Constants:
      IDLE
      RECON
      PROTECT_GENERAL
      PROTECT_DRONE
      DEBUG_ATTACK
      DEBUG_TRACKING
      ASSAULT
      RETURN_TO_HOME
      ATTACK_PERMISSION_NONE
      ATTACK_PERMISSION_APPROVE
      ATTACK_PERMISSION_DENY
    """

    __slots__ = [
        '_header',
        '_command_id',
        '_estop_requested',
        '_attack_permission',
        '_pan_speed',
        '_tilt_speed',
        '_zoom_command',
        '_lateral_wind_speed',
        '_drone_target_lat',
        '_drone_target_lon',
        '_drone_target_valid',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'command_id': 'uint8',
        'estop_requested': 'boolean',
        'attack_permission': 'uint8',
        'pan_speed': 'int8',
        'tilt_speed': 'int8',
        'zoom_command': 'int8',
        'lateral_wind_speed': 'float',
        'drone_target_lat': 'double',
        'drone_target_lon': 'double',
        'drone_target_valid': 'boolean',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('int8'),  # noqa: E501
        rosidl_parser.definition.BasicType('int8'),  # noqa: E501
        rosidl_parser.definition.BasicType('int8'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
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
        self.command_id = kwargs.get('command_id', int())
        self.estop_requested = kwargs.get('estop_requested', bool())
        self.attack_permission = kwargs.get('attack_permission', int())
        self.pan_speed = kwargs.get('pan_speed', int())
        self.tilt_speed = kwargs.get('tilt_speed', int())
        self.zoom_command = kwargs.get('zoom_command', int())
        self.lateral_wind_speed = kwargs.get('lateral_wind_speed', float())
        self.drone_target_lat = kwargs.get('drone_target_lat', float())
        self.drone_target_lon = kwargs.get('drone_target_lon', float())
        self.drone_target_valid = kwargs.get('drone_target_valid', bool())

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
        if self.command_id != other.command_id:
            return False
        if self.estop_requested != other.estop_requested:
            return False
        if self.attack_permission != other.attack_permission:
            return False
        if self.pan_speed != other.pan_speed:
            return False
        if self.tilt_speed != other.tilt_speed:
            return False
        if self.zoom_command != other.zoom_command:
            return False
        if self.lateral_wind_speed != other.lateral_wind_speed:
            return False
        if self.drone_target_lat != other.drone_target_lat:
            return False
        if self.drone_target_lon != other.drone_target_lon:
            return False
        if self.drone_target_valid != other.drone_target_valid:
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
    def command_id(self):
        """Message field 'command_id'."""
        return self._command_id

    @command_id.setter
    def command_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'command_id' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'command_id' field must be an unsigned integer in [0, 255]"
        self._command_id = value

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
    def attack_permission(self):
        """Message field 'attack_permission'."""
        return self._attack_permission

    @attack_permission.setter
    def attack_permission(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'attack_permission' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'attack_permission' field must be an unsigned integer in [0, 255]"
        self._attack_permission = value

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
            assert value >= -128 and value < 128, \
                "The 'pan_speed' field must be an integer in [-128, 127]"
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
            assert value >= -128 and value < 128, \
                "The 'tilt_speed' field must be an integer in [-128, 127]"
        self._tilt_speed = value

    @builtins.property
    def zoom_command(self):
        """Message field 'zoom_command'."""
        return self._zoom_command

    @zoom_command.setter
    def zoom_command(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'zoom_command' field must be of type 'int'"
            assert value >= -128 and value < 128, \
                "The 'zoom_command' field must be an integer in [-128, 127]"
        self._zoom_command = value

    @builtins.property
    def lateral_wind_speed(self):
        """Message field 'lateral_wind_speed'."""
        return self._lateral_wind_speed

    @lateral_wind_speed.setter
    def lateral_wind_speed(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'lateral_wind_speed' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'lateral_wind_speed' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._lateral_wind_speed = value

    @builtins.property
    def drone_target_lat(self):
        """Message field 'drone_target_lat'."""
        return self._drone_target_lat

    @drone_target_lat.setter
    def drone_target_lat(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'drone_target_lat' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'drone_target_lat' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._drone_target_lat = value

    @builtins.property
    def drone_target_lon(self):
        """Message field 'drone_target_lon'."""
        return self._drone_target_lon

    @drone_target_lon.setter
    def drone_target_lon(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'drone_target_lon' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'drone_target_lon' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._drone_target_lon = value

    @builtins.property
    def drone_target_valid(self):
        """Message field 'drone_target_valid'."""
        return self._drone_target_valid

    @drone_target_valid.setter
    def drone_target_valid(self, value):
        if self._check_fields:
            assert \
                isinstance(value, bool), \
                "The 'drone_target_valid' field must be of type 'bool'"
        self._drone_target_valid = value
