#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__BoundingBox2d() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__BoundingBox2d__init(msg: *mut BoundingBox2d) -> bool;
    fn combat_robot_msgs__msg__BoundingBox2d__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<BoundingBox2d>, size: usize) -> bool;
    fn combat_robot_msgs__msg__BoundingBox2d__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<BoundingBox2d>);
    fn combat_robot_msgs__msg__BoundingBox2d__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<BoundingBox2d>, out_seq: *mut rosidl_runtime_rs::Sequence<BoundingBox2d>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__BoundingBox2d
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// BoundingBox2d.msg

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct BoundingBox2d {

    // This member is not documented.
    #[allow(missing_docs)]
    pub x: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub y: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub width: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub height: i32,

}



impl Default for BoundingBox2d {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__BoundingBox2d__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__BoundingBox2d__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for BoundingBox2d {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__BoundingBox2d__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__BoundingBox2d__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__BoundingBox2d__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for BoundingBox2d {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for BoundingBox2d where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/BoundingBox2d";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__BoundingBox2d() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__DetectedObject() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__DetectedObject__init(msg: *mut DetectedObject) -> bool;
    fn combat_robot_msgs__msg__DetectedObject__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<DetectedObject>, size: usize) -> bool;
    fn combat_robot_msgs__msg__DetectedObject__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<DetectedObject>);
    fn combat_robot_msgs__msg__DetectedObject__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<DetectedObject>, out_seq: *mut rosidl_runtime_rs::Sequence<DetectedObject>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__DetectedObject
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// DetectedObject.msg

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct DetectedObject {

    // This member is not documented.
    #[allow(missing_docs)]
    pub id: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub prob: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub box_: super::super::msg::rmw::BoundingBox2d,

}



impl Default for DetectedObject {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__DetectedObject__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__DetectedObject__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for DetectedObject {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DetectedObject__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DetectedObject__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DetectedObject__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for DetectedObject {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for DetectedObject where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/DetectedObject";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__DetectedObject() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__DetectedObjects() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__DetectedObjects__init(msg: *mut DetectedObjects) -> bool;
    fn combat_robot_msgs__msg__DetectedObjects__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<DetectedObjects>, size: usize) -> bool;
    fn combat_robot_msgs__msg__DetectedObjects__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<DetectedObjects>);
    fn combat_robot_msgs__msg__DetectedObjects__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<DetectedObjects>, out_seq: *mut rosidl_runtime_rs::Sequence<DetectedObjects>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__DetectedObjects
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// DetectedObjects.msg

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct DetectedObjects {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub image_width: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub image_height: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub objects: rosidl_runtime_rs::Sequence<super::super::msg::rmw::DetectedObject>,

}



impl Default for DetectedObjects {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__DetectedObjects__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__DetectedObjects__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for DetectedObjects {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DetectedObjects__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DetectedObjects__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DetectedObjects__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for DetectedObjects {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for DetectedObjects where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/DetectedObjects";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__DetectedObjects() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__TargetPoint() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__TargetPoint__init(msg: *mut TargetPoint) -> bool;
    fn combat_robot_msgs__msg__TargetPoint__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<TargetPoint>, size: usize) -> bool;
    fn combat_robot_msgs__msg__TargetPoint__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<TargetPoint>);
    fn combat_robot_msgs__msg__TargetPoint__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<TargetPoint>, out_seq: *mut rosidl_runtime_rs::Sequence<TargetPoint>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__TargetPoint
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct TargetPoint {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub is_locked: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub x: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub y: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub height: f32,

    /// 0: person, 1: drone
    pub class_id: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub box_: super::super::msg::rmw::BoundingBox2d,


    // This member is not documented.
    #[allow(missing_docs)]
    pub track_id: i32,

}



impl Default for TargetPoint {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__TargetPoint__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__TargetPoint__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for TargetPoint {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__TargetPoint__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__TargetPoint__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__TargetPoint__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for TargetPoint {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for TargetPoint where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/TargetPoint";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__TargetPoint() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__TouchTargetPoint() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__TouchTargetPoint__init(msg: *mut TouchTargetPoint) -> bool;
    fn combat_robot_msgs__msg__TouchTargetPoint__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<TouchTargetPoint>, size: usize) -> bool;
    fn combat_robot_msgs__msg__TouchTargetPoint__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<TouchTargetPoint>);
    fn combat_robot_msgs__msg__TouchTargetPoint__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<TouchTargetPoint>, out_seq: *mut rosidl_runtime_rs::Sequence<TouchTargetPoint>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__TouchTargetPoint
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct TouchTargetPoint {

    // This member is not documented.
    #[allow(missing_docs)]
    pub touch_x: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub touch_y: f32,

}



impl Default for TouchTargetPoint {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__TouchTargetPoint__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__TouchTargetPoint__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for TouchTargetPoint {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__TouchTargetPoint__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__TouchTargetPoint__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__TouchTargetPoint__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for TouchTargetPoint {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for TouchTargetPoint where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/TouchTargetPoint";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__TouchTargetPoint() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__UserCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__UserCommand__init(msg: *mut UserCommand) -> bool;
    fn combat_robot_msgs__msg__UserCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<UserCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__UserCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<UserCommand>);
    fn combat_robot_msgs__msg__UserCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<UserCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<UserCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__UserCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// Command command_from

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct UserCommand {
    /// UserCommand.msg
    /// This message is used to send user commands to the combat robot system.
    pub header: std_msgs::msg::rmw::Header,

    /// 0 - tablet / 1 - ble
    pub command_from: u8,

    /// Command ID
    /// 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4/5=Debug, 6=Assault, 7=Return to Home, 8=Estop
    pub command_id: u8,

    /// Manual Targeting Coordinates
    /// X coordinate in normalized pixels (0.0 - 1.0)
    pub target_x: f32,

    /// Y coordinate in normalized pixels (0.0 - 1.0)
    pub target_y: f32,

    /// Drone Search Target
    pub drone_target_lat: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub drone_target_lon: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub drone_target_valid: bool,

    /// Gun trigger control
    /// 0 - stop / 1 - start
    pub gun_trigger: bool,

    /// 0 - no permission / 1 - permission granted
    pub gun_trigger_permission: bool,

    /// Gimbal / stream control
    pub pan_speed: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub tilt_speed: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub zoom_command: i8,

    /// 0=None, 1=Start, 2=Stop
    pub stream_command: u8,

}

impl UserCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const TABLET: u8 = 0;

    /// Command ID
    pub const IDLE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const RECON: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PROTECT_GENERAL: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PROTECT_DRONE: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const DEBUG_ATTACK: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const DEBUG_TRACKING: u8 = 5;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ASSAULT: u8 = 6;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const RETURN_TO_HOME: u8 = 7;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ESTOP: u8 = 8;

    /// Stream command
    pub const STREAM_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const STREAM_START: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const STREAM_STOP: u8 = 2;

}


impl Default for UserCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__UserCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__UserCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for UserCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__UserCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__UserCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__UserCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for UserCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for UserCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/UserCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__UserCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__MissionControlCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__MissionControlCommand__init(msg: *mut MissionControlCommand) -> bool;
    fn combat_robot_msgs__msg__MissionControlCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MissionControlCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__MissionControlCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MissionControlCommand>);
    fn combat_robot_msgs__msg__MissionControlCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MissionControlCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<MissionControlCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__MissionControlCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MissionControlCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub command_id: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub estop_requested: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub attack_permission: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub pan_speed: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub tilt_speed: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub zoom_command: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub lateral_wind_speed: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub drone_target_lat: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub drone_target_lon: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub drone_target_valid: bool,

}

impl MissionControlCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const IDLE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const RECON: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PROTECT_GENERAL: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PROTECT_DRONE: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const DEBUG_ATTACK: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const DEBUG_TRACKING: u8 = 5;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ASSAULT: u8 = 6;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const RETURN_TO_HOME: u8 = 7;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ATTACK_PERMISSION_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ATTACK_PERMISSION_APPROVE: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ATTACK_PERMISSION_DENY: u8 = 2;

}


impl Default for MissionControlCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__MissionControlCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__MissionControlCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MissionControlCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__MissionControlCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__MissionControlCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__MissionControlCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MissionControlCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MissionControlCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/MissionControlCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__MissionControlCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__StreamControlCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__StreamControlCommand__init(msg: *mut StreamControlCommand) -> bool;
    fn combat_robot_msgs__msg__StreamControlCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<StreamControlCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__StreamControlCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<StreamControlCommand>);
    fn combat_robot_msgs__msg__StreamControlCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<StreamControlCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<StreamControlCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__StreamControlCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct StreamControlCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub stream_command: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub stream_target_robot_id: u32,

}

impl StreamControlCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const STREAM_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const STREAM_START: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const STREAM_STOP: u8 = 2;

}


impl Default for StreamControlCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__StreamControlCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__StreamControlCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for StreamControlCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__StreamControlCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__StreamControlCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__StreamControlCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for StreamControlCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for StreamControlCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/StreamControlCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__StreamControlCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmControlCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__SwarmControlCommand__init(msg: *mut SwarmControlCommand) -> bool;
    fn combat_robot_msgs__msg__SwarmControlCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<SwarmControlCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__SwarmControlCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<SwarmControlCommand>);
    fn combat_robot_msgs__msg__SwarmControlCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<SwarmControlCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<SwarmControlCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__SwarmControlCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmControlCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation_type: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation_number: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub grouping_index: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub selected_robot_count: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub selected_robot_ids: [u32; 8],

}

impl SwarmControlCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FORMATION_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FORMATION_RECON: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FORMATION_PROTECT: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FORMATION_ASSAULT: u8 = 3;

}


impl Default for SwarmControlCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__SwarmControlCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__SwarmControlCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for SwarmControlCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmControlCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmControlCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmControlCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for SwarmControlCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for SwarmControlCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/SwarmControlCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmControlCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmFollowerStatus() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__SwarmFollowerStatus__init(msg: *mut SwarmFollowerStatus) -> bool;
    fn combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<SwarmFollowerStatus>, size: usize) -> bool;
    fn combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<SwarmFollowerStatus>);
    fn combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<SwarmFollowerStatus>, out_seq: *mut rosidl_runtime_rs::Sequence<SwarmFollowerStatus>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__SwarmFollowerStatus
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmFollowerStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub robot_id: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub leader_robot_id: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub link_status: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub last_heartbeat_sequence: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub heartbeat_age_sec: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub last_operation_mode: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub last_formation_type: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub last_formation_number: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub last_grouping_index: u8,

    /// Follower position so the operator-host (leader) command_server can place this
    /// robot on the tablet swarm map. Filled by the follower's own command_server from
    /// its NavSatFix (/sN/fix); 0 until a fix is received.
    pub latitude: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub longitude: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub heading_deg: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub ground_speed_mps: f32,

}

impl SwarmFollowerStatus {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const LINK_DISCONNECTED: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const LINK_CONNECTED: u8 = 1;

}


impl Default for SwarmFollowerStatus {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__SwarmFollowerStatus__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__SwarmFollowerStatus__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for SwarmFollowerStatus {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmFollowerStatus__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for SwarmFollowerStatus {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for SwarmFollowerStatus where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/SwarmFollowerStatus";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmFollowerStatus() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmLeaderHeartbeat() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__SwarmLeaderHeartbeat__init(msg: *mut SwarmLeaderHeartbeat) -> bool;
    fn combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<SwarmLeaderHeartbeat>, size: usize) -> bool;
    fn combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<SwarmLeaderHeartbeat>);
    fn combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<SwarmLeaderHeartbeat>, out_seq: *mut rosidl_runtime_rs::Sequence<SwarmLeaderHeartbeat>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__SwarmLeaderHeartbeat
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmLeaderHeartbeat {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub sequence: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub leader_robot_id: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub operation_mode: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub estop_active: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation_type: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation_number: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub grouping_index: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub selected_robot_count: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub selected_robot_ids: [u32; 8],

}



impl Default for SwarmLeaderHeartbeat {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__SwarmLeaderHeartbeat__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__SwarmLeaderHeartbeat__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for SwarmLeaderHeartbeat {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmLeaderHeartbeat__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for SwarmLeaderHeartbeat {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for SwarmLeaderHeartbeat where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/SwarmLeaderHeartbeat";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmLeaderHeartbeat() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmPathCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__SwarmPathCommand__init(msg: *mut SwarmPathCommand) -> bool;
    fn combat_robot_msgs__msg__SwarmPathCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<SwarmPathCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__SwarmPathCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<SwarmPathCommand>);
    fn combat_robot_msgs__msg__SwarmPathCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<SwarmPathCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<SwarmPathCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__SwarmPathCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmPathCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub command: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub num_waypoints: u16,


    // This member is not documented.
    #[allow(missing_docs)]
    pub path_json: rosidl_runtime_rs::String,

}

impl SwarmPathCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CMD_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CMD_START: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CMD_STOP: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CMD_PAUSE: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CMD_RESUME: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CMD_LOAD_PATH: u8 = 5;

    /// path follower / operator signals 'waypoints done'; FSM switches PROTECT modes from drive to engage
    pub const CMD_COMPLETE: u8 = 6;

}


impl Default for SwarmPathCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__SwarmPathCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__SwarmPathCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for SwarmPathCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmPathCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmPathCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmPathCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for SwarmPathCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for SwarmPathCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/SwarmPathCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmPathCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmRobotCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__SwarmRobotCommand__init(msg: *mut SwarmRobotCommand) -> bool;
    fn combat_robot_msgs__msg__SwarmRobotCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<SwarmRobotCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__SwarmRobotCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<SwarmRobotCommand>);
    fn combat_robot_msgs__msg__SwarmRobotCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<SwarmRobotCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<SwarmRobotCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__SwarmRobotCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmRobotCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub sequence: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub command_type: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub leader_robot_id: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub target_robot_id: u32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub operation_mode: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub estop_requested: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub path_command: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub num_waypoints: u16,


    // This member is not documented.
    #[allow(missing_docs)]
    pub path_id: rosidl_runtime_rs::String,


    // This member is not documented.
    #[allow(missing_docs)]
    pub path_json: rosidl_runtime_rs::String,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation_type: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation_number: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub grouping_index: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub slot_index: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub selected_robot_count: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub selected_robot_ids: [u32; 8],

}

impl SwarmRobotCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const COMMAND_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const COMMAND_MODE: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const COMMAND_PATH: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const COMMAND_FORMATION: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const COMMAND_SYNC: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PATH_CMD_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PATH_CMD_START: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PATH_CMD_STOP: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PATH_CMD_PAUSE: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PATH_CMD_RESUME: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const PATH_CMD_LOAD_PATH: u8 = 5;

}


impl Default for SwarmRobotCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__SwarmRobotCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__SwarmRobotCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for SwarmRobotCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmRobotCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmRobotCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__SwarmRobotCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for SwarmRobotCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for SwarmRobotCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/SwarmRobotCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__SwarmRobotCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__IMUState() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__IMUState__init(msg: *mut IMUState) -> bool;
    fn combat_robot_msgs__msg__IMUState__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<IMUState>, size: usize) -> bool;
    fn combat_robot_msgs__msg__IMUState__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<IMUState>);
    fn combat_robot_msgs__msg__IMUState__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<IMUState>, out_seq: *mut rosidl_runtime_rs::Sequence<IMUState>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__IMUState
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct IMUState {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,

    /// Roll Pitch Yaw
    pub angle: geometry_msgs::msg::rmw::Vector3,


    // This member is not documented.
    #[allow(missing_docs)]
    pub is_connected: bool,

    /// "gun" or "car"
    pub device_id: rosidl_runtime_rs::String,

}



impl Default for IMUState {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__IMUState__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__IMUState__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for IMUState {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__IMUState__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__IMUState__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__IMUState__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for IMUState {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for IMUState where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/IMUState";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__IMUState() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__OperationState() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__OperationState__init(msg: *mut OperationState) -> bool;
    fn combat_robot_msgs__msg__OperationState__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<OperationState>, size: usize) -> bool;
    fn combat_robot_msgs__msg__OperationState__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<OperationState>);
    fn combat_robot_msgs__msg__OperationState__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<OperationState>, out_seq: *mut rosidl_runtime_rs::Sequence<OperationState>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__OperationState
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// Internal ROS operation state values

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct OperationState {

    // This member is not documented.
    #[allow(missing_docs)]
    pub state: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub active_mode_id: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub mission_status: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub estop_active: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub permission_request_active: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub crosshair_x: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub crosshair_y: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub current_zoom_level: f32,

    /// Robot navigation status
    pub gps_lat: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub gps_lon: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub gps_heading: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub current_speed_mps: f32,

    /// Common mission placeholder fields for app integration
    pub current_waypoint_index: u16,


    // This member is not documented.
    #[allow(missing_docs)]
    pub total_waypoints: u16,


    // This member is not documented.
    #[allow(missing_docs)]
    pub progress_ratio: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub distance_to_next_wp_m: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub distance_to_goal_m: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub error_code: u8,

}

impl OperationState {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const INIT: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const IDLE: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MOVE: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const SURVEILLANCE: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const DRONE_SURVEILLANCE: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MANUAL_ATTACK: u8 = 5;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ASSAULT: u8 = 6;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const TRACKING: u8 = 7;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const EMERGENCY_STOP: u8 = 8;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ERROR: u8 = 9;

    /// App-facing active mode IDs
    pub const ACTIVE_MODE_IDLE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ACTIVE_MODE_RECON: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ACTIVE_MODE_PROTECT_GENERAL: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ACTIVE_MODE_PROTECT_DRONE: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ACTIVE_MODE_ASSAULT: u8 = 6;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ACTIVE_MODE_RETURN_TO_HOME: u8 = 7;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const ACTIVE_MODE_ESTOP: u8 = 8;

    /// Common mission status IDs
    pub const MISSION_NONE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MISSION_READY: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MISSION_MOVING: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MISSION_PAUSED: u8 = 3;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MISSION_REACHED: u8 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MISSION_SURVEILLING: u8 = 5;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const MISSION_ERROR: u8 = 6;

}


impl Default for OperationState {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__OperationState__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__OperationState__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for OperationState {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__OperationState__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__OperationState__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__OperationState__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for OperationState {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for OperationState where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/OperationState";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__OperationState() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__DriveCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__DriveCommand__init(msg: *mut DriveCommand) -> bool;
    fn combat_robot_msgs__msg__DriveCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<DriveCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__DriveCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<DriveCommand>);
    fn combat_robot_msgs__msg__DriveCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<DriveCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<DriveCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__DriveCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// combat_robot_msgs/msg/DriveCommand.msg

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct DriveCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,

    /// m/s, forward(+)/backward(-)
    pub linear_velocity: f32,

    /// rad/s, left(+)/right(-)  ← 기존 부호 규칙 그대로 사용
    pub angular_velocity: f32,

}



impl Default for DriveCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__DriveCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__DriveCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for DriveCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DriveCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DriveCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__DriveCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for DriveCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for DriveCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/DriveCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__DriveCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__PanTiltControlCommand() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__PanTiltControlCommand__init(msg: *mut PanTiltControlCommand) -> bool;
    fn combat_robot_msgs__msg__PanTiltControlCommand__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<PanTiltControlCommand>, size: usize) -> bool;
    fn combat_robot_msgs__msg__PanTiltControlCommand__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<PanTiltControlCommand>);
    fn combat_robot_msgs__msg__PanTiltControlCommand__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<PanTiltControlCommand>, out_seq: *mut rosidl_runtime_rs::Sequence<PanTiltControlCommand>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__PanTiltControlCommand
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct PanTiltControlCommand {
    /// Pan-Tilt Control Command message
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub control_mode: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub horizontal_angle: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub vertical_angle: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub pan_speed: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub tilt_speed: u8,

    /// 0 - stop / 1 - right / 2 - left
    pub pan_dir: u8,

    /// 0 - stop / 1 - up / 2 - down
    pub tilt_dir: u8,

}

impl PanTiltControlCommand {

    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CONTROL_BRAKE: u8 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CONTROL_HOR_POS: u8 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CONTROL_VER_POS: u8 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const CONTROL_DIR: u8 = 3;

}


impl Default for PanTiltControlCommand {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__PanTiltControlCommand__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__PanTiltControlCommand__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for PanTiltControlCommand {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__PanTiltControlCommand__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__PanTiltControlCommand__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__PanTiltControlCommand__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for PanTiltControlCommand {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for PanTiltControlCommand where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/PanTiltControlCommand";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__PanTiltControlCommand() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__PanTiltState() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__PanTiltState__init(msg: *mut PanTiltState) -> bool;
    fn combat_robot_msgs__msg__PanTiltState__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<PanTiltState>, size: usize) -> bool;
    fn combat_robot_msgs__msg__PanTiltState__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<PanTiltState>);
    fn combat_robot_msgs__msg__PanTiltState__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<PanTiltState>, out_seq: *mut rosidl_runtime_rs::Sequence<PanTiltState>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__PanTiltState
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct PanTiltState {

    // This member is not documented.
    #[allow(missing_docs)]
    pub stamp: builtin_interfaces::msg::rmw::Time,


    // This member is not documented.
    #[allow(missing_docs)]
    pub control_mode: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub horizontal_angle: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub vertical_angle: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub pan_speed: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub tilt_speed: i32,

}



impl Default for PanTiltState {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__PanTiltState__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__PanTiltState__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for PanTiltState {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__PanTiltState__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__PanTiltState__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__PanTiltState__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for PanTiltState {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for PanTiltState where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/PanTiltState";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__PanTiltState() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__CenterObject() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__CenterObject__init(msg: *mut CenterObject) -> bool;
    fn combat_robot_msgs__msg__CenterObject__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<CenterObject>, size: usize) -> bool;
    fn combat_robot_msgs__msg__CenterObject__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<CenterObject>);
    fn combat_robot_msgs__msg__CenterObject__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<CenterObject>, out_seq: *mut rosidl_runtime_rs::Sequence<CenterObject>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__CenterObject
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// CenterObject.msg

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct CenterObject {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub class_id: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub bounding_box: super::super::msg::rmw::BoundingBox2d,


    // This member is not documented.
    #[allow(missing_docs)]
    pub target_x: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub target_y: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub laser_distance: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub zoom_level: f32,

}



impl Default for CenterObject {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__CenterObject__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__CenterObject__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for CenterObject {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__CenterObject__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__CenterObject__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__CenterObject__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for CenterObject {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for CenterObject where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/CenterObject";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__CenterObject() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__Waypoint() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__Waypoint__init(msg: *mut Waypoint) -> bool;
    fn combat_robot_msgs__msg__Waypoint__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<Waypoint>, size: usize) -> bool;
    fn combat_robot_msgs__msg__Waypoint__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<Waypoint>);
    fn combat_robot_msgs__msg__Waypoint__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<Waypoint>, out_seq: *mut rosidl_runtime_rs::Sequence<Waypoint>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__Waypoint
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Waypoint {

    // This member is not documented.
    #[allow(missing_docs)]
    pub way_id: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub way_lon: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub way_lat: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub way_status: i32,

}



impl Default for Waypoint {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__Waypoint__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__Waypoint__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for Waypoint {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__Waypoint__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__Waypoint__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__Waypoint__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for Waypoint {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for Waypoint where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/Waypoint";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__Waypoint() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__WaypointList() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__WaypointList__init(msg: *mut WaypointList) -> bool;
    fn combat_robot_msgs__msg__WaypointList__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<WaypointList>, size: usize) -> bool;
    fn combat_robot_msgs__msg__WaypointList__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<WaypointList>);
    fn combat_robot_msgs__msg__WaypointList__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<WaypointList>, out_seq: *mut rosidl_runtime_rs::Sequence<WaypointList>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__WaypointList
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct WaypointList {

    // This member is not documented.
    #[allow(missing_docs)]
    pub mode: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub formation: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub mission_id: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub mission_status: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub waypoints: rosidl_runtime_rs::Sequence<super::super::msg::rmw::Waypoint>,

}



impl Default for WaypointList {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__WaypointList__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__WaypointList__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for WaypointList {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__WaypointList__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__WaypointList__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__WaypointList__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for WaypointList {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for WaypointList where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/WaypointList";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__WaypointList() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__GnssStatus() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__GnssStatus__init(msg: *mut GnssStatus) -> bool;
    fn combat_robot_msgs__msg__GnssStatus__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<GnssStatus>, size: usize) -> bool;
    fn combat_robot_msgs__msg__GnssStatus__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<GnssStatus>);
    fn combat_robot_msgs__msg__GnssStatus__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<GnssStatus>, out_seq: *mut rosidl_runtime_rs::Sequence<GnssStatus>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__GnssStatus
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// GNSS / RTK 드라이버가 publish 하는 status.
/// robot_server 가 /gnss/status 토픽으로 subscribe 하여 leader 위치/heading/speed 와
/// 앱 상태 패킷의 fix 품질 표시에 사용함.
///
/// 모든 필드를 매 주기 채워 보내주세요. 값을 알 수 없으면 아래 정의된 INVALID 값을 사용.

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct GnssStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub fix_status: u8,

    /// 추적 위성 수 (0~32 typical)
    pub num_satellites: u8,

    /// 위치 — WGS-84
    /// 유효하지 않으면 latitude=longitude=NaN
    /// 도
    pub latitude: f64,

    /// 도
    pub longitude: f64,

    /// MSL m
    pub altitude_m: f64,

    /// 방위각 / 속도
    /// 유효하지 않으면 -1.0
    /// 0~360, true north 기준
    pub heading_deg: f32,

    /// >= 0
    pub ground_speed_mps: f32,

    /// 정확도 (1-sigma CEP)
    /// 알 수 없으면 -1.0
    pub horizontal_accuracy_m: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub vertical_accuracy_m: f32,

}

impl GnssStatus {
    /// Fix 품질 (NMEA / RTK 기준)
    /// No fix / 수신기 끊김
    pub const FIX_NONE: u8 = 0;

    /// 2D 위성 fix
    pub const FIX_2D: u8 = 1;

    /// 3D 위성 fix
    pub const FIX_3D: u8 = 2;

    /// Differential GPS
    pub const FIX_DGPS: u8 = 3;

    /// RTK Float (cm~m 수준)
    pub const FIX_RTK_FLOAT: u8 = 4;

    /// RTK Fixed (cm 수준)
    pub const FIX_RTK_FIXED: u8 = 5;

}


impl Default for GnssStatus {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__GnssStatus__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__GnssStatus__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for GnssStatus {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__GnssStatus__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__GnssStatus__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__GnssStatus__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for GnssStatus {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for GnssStatus where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/GnssStatus";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__GnssStatus() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__ChassisStatus() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__ChassisStatus__init(msg: *mut ChassisStatus) -> bool;
    fn combat_robot_msgs__msg__ChassisStatus__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<ChassisStatus>, size: usize) -> bool;
    fn combat_robot_msgs__msg__ChassisStatus__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<ChassisStatus>);
    fn combat_robot_msgs__msg__ChassisStatus__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<ChassisStatus>, out_seq: *mut rosidl_runtime_rs::Sequence<ChassisStatus>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__ChassisStatus
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// 차량(chassis) 컨트롤러가 publish 하는 status.
/// robot_server 가 /chassis/status 토픽으로 subscribe 하여 BMS / 구동 상태를
/// 앱 상태 패킷의 battery_pct, velocity 등에 반영함.
///
/// 가능한 한 모든 필드를 매 주기 채워 보내주세요. 값을 모르면 0 또는 NaN 사용 가능.

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct ChassisStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub drive_state: u8,

    /// 배터리 — BMS 값
    /// 0~100 (255 = 알 수 없음)
    pub battery_pct: u8,

    /// V
    pub battery_voltage_v: f32,

    /// A  ( + = 방전 / - = 충전 )
    pub battery_current_a: f32,

    /// Velocity feedback — 차량이 실제로 움직이는 속도 (encoder/odometry 기반)
    /// 알 수 없으면 NaN.
    /// 전진 양 = +, 후진 = -
    pub linear_velocity_mps: f32,

    /// rad/s, 반시계 양 = +
    pub angular_velocity_rps: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub fault_flags: u32,

    /// 모터 최고 온도 (좌/우 휠 중 큰 값). 알 수 없으면 NaN.
    pub motor_temp_c: f32,

}

impl ChassisStatus {
    /// Drive state
    /// 정상 운행 가능
    pub const DRIVE_OK: u8 = 0;

    /// 모터/통신 장애 — 운행 불가
    pub const DRIVE_FAULT: u8 = 1;

    /// E-Stop 인가됨
    pub const DRIVE_ESTOP: u8 = 2;

    /// Fault flags — bitmask. 여러 결함 동시 표현 가능.
    pub const FAULT_NONE: u32 = 0;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FAULT_LEFT_WHEEL: u32 = 1;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FAULT_RIGHT_WHEEL: u32 = 2;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FAULT_LOW_BATTERY: u32 = 4;


    // This constant is not documented.
    #[allow(missing_docs)]
    pub const FAULT_OVERTEMP: u32 = 8;

    /// chassis ↔ robot_server 통신 (Modbus 등) 장애
    pub const FAULT_COMM: u32 = 16;

}


impl Default for ChassisStatus {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__ChassisStatus__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__ChassisStatus__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for ChassisStatus {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__ChassisStatus__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__ChassisStatus__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__ChassisStatus__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for ChassisStatus {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for ChassisStatus where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/ChassisStatus";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__ChassisStatus() }
  }
}


#[link(name = "combat_robot_msgs__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__LidarStatus() -> *const std::ffi::c_void;
}

#[link(name = "combat_robot_msgs__rosidl_generator_c")]
extern "C" {
    fn combat_robot_msgs__msg__LidarStatus__init(msg: *mut LidarStatus) -> bool;
    fn combat_robot_msgs__msg__LidarStatus__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<LidarStatus>, size: usize) -> bool;
    fn combat_robot_msgs__msg__LidarStatus__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<LidarStatus>);
    fn combat_robot_msgs__msg__LidarStatus__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<LidarStatus>, out_seq: *mut rosidl_runtime_rs::Sequence<LidarStatus>) -> bool;
}

// Corresponds to combat_robot_msgs__msg__LidarStatus
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// LiDAR 드라이버가 publish 하는 status.
/// robot_server 가 /lidar/status 토픽으로 subscribe 하여 LiDAR 헬스 / 장애물 정보를
/// 추후 status 패킷에 반영하거나 안전 분기 입력으로 활용함.
///
/// 가능한 한 모든 필드를 매 주기 채워 보내주세요.

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct LidarStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub status: u8,

    /// 마지막 스캔 통계
    pub last_scan_point_count: u32,

    /// 실측 Hz (목표값과 차이 나면 LIDAR_DEGRADED 권장)
    pub scan_rate_hz: f32,

    /// 장애물 감지 — 안전 정지 입력으로 활용 가능.
    /// 가까이 있는 장애물 없으면 obstacle_detected=false, min_obstacle_distance_m=NaN.
    pub obstacle_detected: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub min_obstacle_distance_m: f32,

}

impl LidarStatus {
    /// Driver / hardware 상태
    /// 정상 스캔 중
    pub const LIDAR_OK: u8 = 0;

    /// 스캔되지만 품질 저하 (rate↓, noise↑)
    pub const LIDAR_DEGRADED: u8 = 1;

    /// 통신 / hardware 장애
    pub const LIDAR_FAULT: u8 = 2;

}


impl Default for LidarStatus {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !combat_robot_msgs__msg__LidarStatus__init(&mut msg as *mut _) {
        panic!("Call to combat_robot_msgs__msg__LidarStatus__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for LidarStatus {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__LidarStatus__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__LidarStatus__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { combat_robot_msgs__msg__LidarStatus__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for LidarStatus {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for LidarStatus where Self: Sized {
  const TYPE_NAME: &'static str = "combat_robot_msgs/msg/LidarStatus";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__combat_robot_msgs__msg__LidarStatus() }
  }
}


