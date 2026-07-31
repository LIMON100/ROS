# generated from rosidl_generator_py/resource/_idl.py.em
# with input from combat_robot_msgs:msg/ChassisStatus.idl
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


class Metaclass_ChassisStatus(type):
    """Metaclass of message 'ChassisStatus'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'DRIVE_OK': 0,
        'DRIVE_FAULT': 1,
        'DRIVE_ESTOP': 2,
        'FAULT_NONE': 0,
        'FAULT_LEFT_WHEEL': 1,
        'FAULT_RIGHT_WHEEL': 2,
        'FAULT_LOW_BATTERY': 4,
        'FAULT_OVERTEMP': 8,
        'FAULT_COMM': 16,
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
                'combat_robot_msgs.msg.ChassisStatus')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__chassis_status
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__chassis_status
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__chassis_status
            cls._TYPE_SUPPORT = module.type_support_msg__msg__chassis_status
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__chassis_status

            from std_msgs.msg import Header
            if Header.__class__._TYPE_SUPPORT is None:
                Header.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'DRIVE_OK': cls.__constants['DRIVE_OK'],
            'DRIVE_FAULT': cls.__constants['DRIVE_FAULT'],
            'DRIVE_ESTOP': cls.__constants['DRIVE_ESTOP'],
            'FAULT_NONE': cls.__constants['FAULT_NONE'],
            'FAULT_LEFT_WHEEL': cls.__constants['FAULT_LEFT_WHEEL'],
            'FAULT_RIGHT_WHEEL': cls.__constants['FAULT_RIGHT_WHEEL'],
            'FAULT_LOW_BATTERY': cls.__constants['FAULT_LOW_BATTERY'],
            'FAULT_OVERTEMP': cls.__constants['FAULT_OVERTEMP'],
            'FAULT_COMM': cls.__constants['FAULT_COMM'],
        }

    @property
    def DRIVE_OK(self):
        """Message constant 'DRIVE_OK'."""
        return Metaclass_ChassisStatus.__constants['DRIVE_OK']

    @property
    def DRIVE_FAULT(self):
        """Message constant 'DRIVE_FAULT'."""
        return Metaclass_ChassisStatus.__constants['DRIVE_FAULT']

    @property
    def DRIVE_ESTOP(self):
        """Message constant 'DRIVE_ESTOP'."""
        return Metaclass_ChassisStatus.__constants['DRIVE_ESTOP']

    @property
    def FAULT_NONE(self):
        """Message constant 'FAULT_NONE'."""
        return Metaclass_ChassisStatus.__constants['FAULT_NONE']

    @property
    def FAULT_LEFT_WHEEL(self):
        """Message constant 'FAULT_LEFT_WHEEL'."""
        return Metaclass_ChassisStatus.__constants['FAULT_LEFT_WHEEL']

    @property
    def FAULT_RIGHT_WHEEL(self):
        """Message constant 'FAULT_RIGHT_WHEEL'."""
        return Metaclass_ChassisStatus.__constants['FAULT_RIGHT_WHEEL']

    @property
    def FAULT_LOW_BATTERY(self):
        """Message constant 'FAULT_LOW_BATTERY'."""
        return Metaclass_ChassisStatus.__constants['FAULT_LOW_BATTERY']

    @property
    def FAULT_OVERTEMP(self):
        """Message constant 'FAULT_OVERTEMP'."""
        return Metaclass_ChassisStatus.__constants['FAULT_OVERTEMP']

    @property
    def FAULT_COMM(self):
        """Message constant 'FAULT_COMM'."""
        return Metaclass_ChassisStatus.__constants['FAULT_COMM']


class ChassisStatus(metaclass=Metaclass_ChassisStatus):
    """
    Message class 'ChassisStatus'.

    Constants:
      DRIVE_OK
      DRIVE_FAULT
      DRIVE_ESTOP
      FAULT_NONE
      FAULT_LEFT_WHEEL
      FAULT_RIGHT_WHEEL
      FAULT_LOW_BATTERY
      FAULT_OVERTEMP
      FAULT_COMM
    """

    __slots__ = [
        '_header',
        '_drive_state',
        '_battery_pct',
        '_battery_voltage_v',
        '_battery_current_a',
        '_linear_velocity_mps',
        '_angular_velocity_rps',
        '_fault_flags',
        '_motor_temp_c',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'header': 'std_msgs/Header',
        'drive_state': 'uint8',
        'battery_pct': 'uint8',
        'battery_voltage_v': 'float',
        'battery_current_a': 'float',
        'linear_velocity_mps': 'float',
        'angular_velocity_rps': 'float',
        'fault_flags': 'uint32',
        'motor_temp_c': 'float',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['std_msgs', 'msg'], 'Header'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('float'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
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
        self.drive_state = kwargs.get('drive_state', int())
        self.battery_pct = kwargs.get('battery_pct', int())
        self.battery_voltage_v = kwargs.get('battery_voltage_v', float())
        self.battery_current_a = kwargs.get('battery_current_a', float())
        self.linear_velocity_mps = kwargs.get('linear_velocity_mps', float())
        self.angular_velocity_rps = kwargs.get('angular_velocity_rps', float())
        self.fault_flags = kwargs.get('fault_flags', int())
        self.motor_temp_c = kwargs.get('motor_temp_c', float())

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
        if self.drive_state != other.drive_state:
            return False
        if self.battery_pct != other.battery_pct:
            return False
        if self.battery_voltage_v != other.battery_voltage_v:
            return False
        if self.battery_current_a != other.battery_current_a:
            return False
        if self.linear_velocity_mps != other.linear_velocity_mps:
            return False
        if self.angular_velocity_rps != other.angular_velocity_rps:
            return False
        if self.fault_flags != other.fault_flags:
            return False
        if self.motor_temp_c != other.motor_temp_c:
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
    def drive_state(self):
        """Message field 'drive_state'."""
        return self._drive_state

    @drive_state.setter
    def drive_state(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'drive_state' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'drive_state' field must be an unsigned integer in [0, 255]"
        self._drive_state = value

    @builtins.property
    def battery_pct(self):
        """Message field 'battery_pct'."""
        return self._battery_pct

    @battery_pct.setter
    def battery_pct(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'battery_pct' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'battery_pct' field must be an unsigned integer in [0, 255]"
        self._battery_pct = value

    @builtins.property
    def battery_voltage_v(self):
        """Message field 'battery_voltage_v'."""
        return self._battery_voltage_v

    @battery_voltage_v.setter
    def battery_voltage_v(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'battery_voltage_v' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'battery_voltage_v' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._battery_voltage_v = value

    @builtins.property
    def battery_current_a(self):
        """Message field 'battery_current_a'."""
        return self._battery_current_a

    @battery_current_a.setter
    def battery_current_a(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'battery_current_a' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'battery_current_a' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._battery_current_a = value

    @builtins.property
    def linear_velocity_mps(self):
        """Message field 'linear_velocity_mps'."""
        return self._linear_velocity_mps

    @linear_velocity_mps.setter
    def linear_velocity_mps(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'linear_velocity_mps' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'linear_velocity_mps' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._linear_velocity_mps = value

    @builtins.property
    def angular_velocity_rps(self):
        """Message field 'angular_velocity_rps'."""
        return self._angular_velocity_rps

    @angular_velocity_rps.setter
    def angular_velocity_rps(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'angular_velocity_rps' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'angular_velocity_rps' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._angular_velocity_rps = value

    @builtins.property
    def fault_flags(self):
        """Message field 'fault_flags'."""
        return self._fault_flags

    @fault_flags.setter
    def fault_flags(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'fault_flags' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'fault_flags' field must be an unsigned integer in [0, 4294967295]"
        self._fault_flags = value

    @builtins.property
    def motor_temp_c(self):
        """Message field 'motor_temp_c'."""
        return self._motor_temp_c

    @motor_temp_c.setter
    def motor_temp_c(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'motor_temp_c' field must be of type 'float'"
            assert not (value < -3.402823466e+38 or value > 3.402823466e+38) or math.isinf(value), \
                "The 'motor_temp_c' field must be a float in [-3.402823466e+38, 3.402823466e+38]"
        self._motor_temp_c = value
