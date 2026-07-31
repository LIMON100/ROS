# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/OperationState.idl
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


class Metaclass_OperationState(type):
    """Metaclass of message 'OperationState'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'INIT': 0,
        'IDLE': 1,
        'MOVE': 2,
        'SURVEILLANCE': 3,
        'DRONE_SURVEILLANCE': 4,
        'MANUAL_ATTACK': 5,
        'ASSAULT': 6,
        'TRACKING': 7,
        'EMERGENCY_STOP': 8,
        'ERROR': 9,
        'ACTIVE_MODE_IDLE': 0,
        'ACTIVE_MODE_RECON': 1,
        'ACTIVE_MODE_PROTECT_GENERAL': 2,
        'ACTIVE_MODE_PROTECT_DRONE': 3,
        'ACTIVE_MODE_ASSAULT': 6,
        'ACTIVE_MODE_RETURN_TO_HOME': 7,
        'ACTIVE_MODE_ESTOP': 8,
        'MISSION_NONE': 0,
        'MISSION_READY': 1,
        'MISSION_MOVING': 2,
        'MISSION_PAUSED': 3,
        'MISSION_REACHED': 4,
        'MISSION_SURVEILLING': 5,
        'MISSION_ERROR': 6,
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
                'combat_robot_msgs.msg.OperationState')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__operation_state
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__operation_state
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__operation_state
            cls._TYPE_SUPPORT = module.type_support_msg__msg__operation_state
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__operation_state

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'INIT': cls.__constants['INIT'],
            'IDLE': cls.__constants['IDLE'],
            'MOVE': cls.__constants['MOVE'],
            'SURVEILLANCE': cls.__constants['SURVEILLANCE'],
            'DRONE_SURVEILLANCE': cls.__constants['DRONE_SURVEILLANCE'],
            'MANUAL_ATTACK': cls.__constants['MANUAL_ATTACK'],
            'ASSAULT': cls.__constants['ASSAULT'],
            'TRACKING': cls.__constants['TRACKING'],
            'EMERGENCY_STOP': cls.__constants['EMERGENCY_STOP'],
            'ERROR': cls.__constants['ERROR'],
            'ACTIVE_MODE_IDLE': cls.__constants['ACTIVE_MODE_IDLE'],
            'ACTIVE_MODE_RECON': cls.__constants['ACTIVE_MODE_RECON'],
            'ACTIVE_MODE_PROTECT_GENERAL': cls.__constants['ACTIVE_MODE_PROTECT_GENERAL'],
            'ACTIVE_MODE_PROTECT_DRONE': cls.__constants['ACTIVE_MODE_PROTECT_DRONE'],
            'ACTIVE_MODE_ASSAULT': cls.__constants['ACTIVE_MODE_ASSAULT'],
            'ACTIVE_MODE_RETURN_TO_HOME': cls.__constants['ACTIVE_MODE_RETURN_TO_HOME'],
            'ACTIVE_MODE_ESTOP': cls.__constants['ACTIVE_MODE_ESTOP'],
            'MISSION_NONE': cls.__constants['MISSION_NONE'],
            'MISSION_READY': cls.__constants['MISSION_READY'],
            'MISSION_MOVING': cls.__constants['MISSION_MOVING'],
            'MISSION_PAUSED': cls.__constants['MISSION_PAUSED'],
            'MISSION_REACHED': cls.__constants['MISSION_REACHED'],
            'MISSION_SURVEILLING': cls.__constants['MISSION_SURVEILLING'],
            'MISSION_ERROR': cls.__constants['MISSION_ERROR'],
        }

    @property
    def INIT(self):
        """Message constant 'INIT'."""
        return Metaclass_OperationState.__constants['INIT']

    @property
    def IDLE(self):
        """Message constant 'IDLE'."""
        return Metaclass_OperationState.__constants['IDLE']

    @property
    def MOVE(self):
        """Message constant 'MOVE'."""
        return Metaclass_OperationState.__constants['MOVE']

    @property
    def SURVEILLANCE(self):
        """Message constant 'SURVEILLANCE'."""
        return Metaclass_OperationState.__constants['SURVEILLANCE']

    @property
    def DRONE_SURVEILLANCE(self):
        """Message constant 'DRONE_SURVEILLANCE'."""
        return Metaclass_OperationState.__constants['DRONE_SURVEILLANCE']

    @property
    def MANUAL_ATTACK(self):
        """Message constant 'MANUAL_ATTACK'."""
        return Metaclass_OperationState.__constants['MANUAL_ATTACK']

    @property
    def ASSAULT(self):
        """Message constant 'ASSAULT'."""
        return Metaclass_OperationState.__constants['ASSAULT']

    @property
    def TRACKING(self):
        """Message constant 'TRACKING'."""
        return Metaclass_OperationState.__constants['TRACKING']

    @property
    def EMERGENCY_STOP(self):
        """Message constant 'EMERGENCY_STOP'."""
        return Metaclass_OperationState.__constants['EMERGENCY_STOP']

    @property
    def ERROR(self):
        """Message constant 'ERROR'."""
        return Metaclass_OperationState.__constants['ERROR']

    @property
    def ACTIVE_MODE_IDLE(self):
        """Message constant 'ACTIVE_MODE_IDLE'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_IDLE']

    @property
    def ACTIVE_MODE_RECON(self):
        """Message constant 'ACTIVE_MODE_RECON'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_RECON']

    @property
    def ACTIVE_MODE_PROTECT_GENERAL(self):
        """Message constant 'ACTIVE_MODE_PROTECT_GENERAL'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_PROTECT_GENERAL']

    @property
    def ACTIVE_MODE_PROTECT_DRONE(self):
        """Message constant 'ACTIVE_MODE_PROTECT_DRONE'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_PROTECT_DRONE']

    @property
    def ACTIVE_MODE_ASSAULT(self):
        """Message constant 'ACTIVE_MODE_ASSAULT'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_ASSAULT']

    @property
    def ACTIVE_MODE_RETURN_TO_HOME(self):
        """Message constant 'ACTIVE_MODE_RETURN_TO_HOME'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_RETURN_TO_HOME']

    @property
    def ACTIVE_MODE_ESTOP(self):
        """Message constant 'ACTIVE_MODE_ESTOP'."""
        return Metaclass_OperationState.__constants['ACTIVE_MODE_ESTOP']

    @property
    def MISSION_NONE(self):
        """Message constant 'MISSION_NONE'."""
        return Metaclass_OperationState.__constants['MISSION_NONE']

    @property
    def MISSION_READY(self):
        """Message constant 'MISSION_READY'."""
        return Metaclass_OperationState.__constants['MISSION_READY']

    @property
    def MISSION_MOVING(self):
        """Message constant 'MISSION_MOVING'."""
        return Metaclass_OperationState.__constants['MISSION_MOVING']

    @property
    def MISSION_PAUSED(self):
        """Message constant 'MISSION_PAUSED'."""
        return Metaclass_OperationState.__constants['MISSION_PAUSED']

    @property
    def MISSION_REACHED(self):
        """Message constant 'MISSION_REACHED'."""
        return Metaclass_OperationState.__constants['MISSION_REACHED']

    @property
    def MISSION_SURVEILLING(self):
        """Message constant 'MISSION_SURVEILLING'."""
        return Metaclass_OperationState.__constants['MISSION_SURVEILLING']

    @property
    def MISSION_ERROR(self):
        """Message constant 'MISSION_ERROR'."""
        return Metaclass_OperationState.__constants['MISSION_ERROR']


class OperationState(metaclass=Metaclass_OperationState):
    """
    Message class 'OperationState'.

    Constants:
      INIT
      IDLE
      MOVE
      SURVEILLANCE
      DRONE_SURVEILLANCE
      MANUAL_ATTACK
      ASSAULT
      TRACKING
      EMERGENCY_STOP
      ERROR
      ACTIVE_MODE_IDLE
      ACTIVE_MODE_RECON
      ACTIVE_MODE_PROTECT_GENERAL
      ACTIVE_MODE_PROTECT_DRONE
      ACTIVE_MODE_ASSAULT
      ACTIVE_MODE_RETURN_TO_HOME
      ACTIVE_MODE_ESTOP
      MISSION_NONE
      MISSION_READY
      MISSION_MOVING
      MISSION_PAUSED
      MISSION_REACHED
      MISSION_SURVEILLING
      MISSION_ERROR
    """

    __slots__ = [
        '_state',
        '_active_mode_id',
        '_mission_status',
        '_estop_active',
        '_permission_request_active',
        '_crosshair_x',
        '_crosshair_y',
        '_current_zoom_level',
        '_gps_lat',
        '_gps_lon',
        '_gps_heading',
        '_current_speed_mps',
        '_current_waypoint_index',
        '_total_waypoints',
        '_progress_ratio',
        '_distance_to_next_wp_m',
        '_distance_to_goal_m',
        '_error_code',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'state': 'uint8',
        'active_mode_id': 'uint8',
        'mission_status': 'uint8',
        'estop_active': 'boolean',
        'permission_request_active': 'boolean',
        'crosshair_x': 'float',
        'crosshair_y': 'float',
        'current_zoom_level': 'float',
        'gps_lat': 'double',
        'gps_lon': 'double',
        'gps_heading': 'float',
        'current_speed_mps': 'float',
        'current_waypoint_index': 'uint16',
        'total_waypoints': 'uint16',
        'progress_ratio': 'float',
        'distance_to_next_wp_m': 'float',
        'distance_to_goal_m': 'float',
        'error_code': 'uint8',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint16'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint16'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
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
        self.state = kwargs.get('state', int())
        self.active_mode_id = kwargs.get('active_mode_id', int())
        self.mission_status = kwargs.get('mission_status', int())
        self.estop_active = kwargs.get('estop_active', bool())
        self.permission_request_active = kwargs.get('permission_request_active', bool())
        self.crosshair_x = kwargs.get('crosshair_x', float())
        self.crosshair_y = kwargs.get('crosshair_y', float())
        self.current_zoom_level = kwargs.get('current_zoom_level', float())
        self.gps_lat = kwargs.get('gps_lat', float())
        self.gps_lon = kwargs.get('gps_lon', float())
        self.gps_heading = kwargs.get('gps_heading', float())
        self.current_speed_mps = kwargs.get('current_speed_mps', float())
        self.current_waypoint_index = kwargs.get('current_waypoint_index', int())
        self.total_waypoints = kwargs.get('total_waypoints', int())
        self.progress_ratio = kwargs.get('progress_ratio', float())
        self.distance_to_next_wp_m = kwargs.get('distance_to_next_wp_m', float())
        self.distance_to_goal_m = kwargs.get('distance_to_goal_m', float())
        self.error_code = kwargs.get('error_code', int())

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
        if self.state != other.state:
            return False
        if self.active_mode_id != other.active_mode_id:
            return False
        if self.mission_status != other.mission_status:
            return False
        if self.estop_active != other.estop_active:
            return False
        if self.permission_request_active != other.permission_request_active:
            return False
        if self.crosshair_x != other.crosshair_x:
            return False
        if self.crosshair_y != other.crosshair_y:
            return False
        if self.current_zoom_level != other.current_zoom_level:
            return False
        if self.gps_lat != other.gps_lat:
            return False
        if self.gps_lon != other.gps_lon:
            return False
        if self.gps_heading != other.gps_heading:
            return False
        if self.current_speed_mps != other.current_speed_mps:
            return False
        if self.current_waypoint_index != other.current_waypoint_index:
            return False
        if self.total_waypoints != other.total_waypoints:
            return False
        if self.progress_ratio != other.progress_ratio:
            return False
        if self.distance_to_next_wp_m != other.distance_to_next_wp_m:
            return False
        if self.distance_to_goal_m != other.distance_to_goal_m:
            return False
        if self.error_code != other.error_code:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def state(self):
        """Message field 'state'."""
        return self._state

    @state.setter
    def state(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'state' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'state' field must be an unsigned integer in [0, 255]"
        self._state = value

    @builtins.property
    def active_mode_id(self):
        """Message field 'active_mode_id'."""
        return self._active_mode_id

    @active_mode_id.setter
    def active_mode_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'active_mode_id' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'active_mode_id' field must be an unsigned integer in [0, 255]"
        self._active_mode_id = value

    @builtins.property
    def mission_status(self):
        """Message field 'mission_status'."""
        return self._mission_status

    @mission_status.setter
    def mission_status(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'mission_status' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'mission_status' field must be an unsigned integer in [0, 255]"
        self._mission_status = value

    @builtins.property
    def estop_active(self):
        """Message field 'estop_active'."""
        return self._estop_active

    @estop_active.setter
    def estop_active(self, value):
        if self._check_fields:
            assert \
                isinstance(value, bool), \
                "The 'estop_active' field must be of type 'bool'"
        self._estop_active = value

    @builtins.property
    def permission_request_active(self):
        """Message field 'permission_request_active'."""
        return self._permission_request_active

    @permission_request_active.setter
    def permission_request_active(self, value):
        if self._check_fields:
            assert \
                isinstance(value, bool), \
                "The 'permission_request_active' field must be of type 'bool'"
        self._permission_request_active = value

    @builtins.property
    def crosshair_x(self):
        """Message field 'crosshair_x'."""
        return self._crosshair_x

    @crosshair_x.setter
    def crosshair_x(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'crosshair_x' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'crosshair_x' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._crosshair_x = value

    @builtins.property
    def crosshair_y(self):
        """Message field 'crosshair_y'."""
        return self._crosshair_y

    @crosshair_y.setter
    def crosshair_y(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'crosshair_y' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'crosshair_y' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._crosshair_y = value

    @builtins.property
    def current_zoom_level(self):
        """Message field 'current_zoom_level'."""
        return self._current_zoom_level

    @current_zoom_level.setter
    def current_zoom_level(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'current_zoom_level' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'current_zoom_level' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._current_zoom_level = value

    @builtins.property
    def gps_lat(self):
        """Message field 'gps_lat'."""
        return self._gps_lat

    @gps_lat.setter
    def gps_lat(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'gps_lat' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'gps_lat' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._gps_lat = value

    @builtins.property
    def gps_lon(self):
        """Message field 'gps_lon'."""
        return self._gps_lon

    @gps_lon.setter
    def gps_lon(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'gps_lon' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'gps_lon' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._gps_lon = value

    @builtins.property
    def gps_heading(self):
        """Message field 'gps_heading'."""
        return self._gps_heading

    @gps_heading.setter
    def gps_heading(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'gps_heading' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'gps_heading' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._gps_heading = value

    @builtins.property
    def current_speed_mps(self):
        """Message field 'current_speed_mps'."""
        return self._current_speed_mps

    @current_speed_mps.setter
    def current_speed_mps(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'current_speed_mps' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'current_speed_mps' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._current_speed_mps = value

    @builtins.property
    def current_waypoint_index(self):
        """Message field 'current_waypoint_index'."""
        return self._current_waypoint_index

    @current_waypoint_index.setter
    def current_waypoint_index(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'current_waypoint_index' field must be of type 'int'"
            assert value >= 0 and value < 65536, \
                "The 'current_waypoint_index' field must be an unsigned integer in [0, 65535]"
        self._current_waypoint_index = value

    @builtins.property
    def total_waypoints(self):
        """Message field 'total_waypoints'."""
        return self._total_waypoints

    @total_waypoints.setter
    def total_waypoints(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'total_waypoints' field must be of type 'int'"
            assert value >= 0 and value < 65536, \
                "The 'total_waypoints' field must be an unsigned integer in [0, 65535]"
        self._total_waypoints = value

    @builtins.property
    def progress_ratio(self):
        """Message field 'progress_ratio'."""
        return self._progress_ratio

    @progress_ratio.setter
    def progress_ratio(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'progress_ratio' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'progress_ratio' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._progress_ratio = value

    @builtins.property
    def distance_to_next_wp_m(self):
        """Message field 'distance_to_next_wp_m'."""
        return self._distance_to_next_wp_m

    @distance_to_next_wp_m.setter
    def distance_to_next_wp_m(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'distance_to_next_wp_m' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'distance_to_next_wp_m' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._distance_to_next_wp_m = value

    @builtins.property
    def distance_to_goal_m(self):
        """Message field 'distance_to_goal_m'."""
        return self._distance_to_goal_m

    @distance_to_goal_m.setter
    def distance_to_goal_m(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'distance_to_goal_m' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'distance_to_goal_m' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._distance_to_goal_m = value

    @builtins.property
    def error_code(self):
        """Message field 'error_code'."""
        return self._error_code

    @error_code.setter
    def error_code(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'error_code' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'error_code' field must be an unsigned integer in [0, 255]"
        self._error_code = value
