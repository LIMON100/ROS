#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};



// Corresponds to combat_robot_msgs__msg__BoundingBox2d
/// BoundingBox2d.msg

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::BoundingBox2d::default())
  }
}

impl rosidl_runtime_rs::Message for BoundingBox2d {
  type RmwMsg = super::msg::rmw::BoundingBox2d;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        x: msg.x,
        y: msg.y,
        width: msg.width,
        height: msg.height,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      x: msg.x,
      y: msg.y,
      width: msg.width,
      height: msg.height,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      x: msg.x,
      y: msg.y,
      width: msg.width,
      height: msg.height,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__DetectedObject
/// DetectedObject.msg

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
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
    pub box_: super::msg::BoundingBox2d,

}



impl Default for DetectedObject {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::DetectedObject::default())
  }
}

impl rosidl_runtime_rs::Message for DetectedObject {
  type RmwMsg = super::msg::rmw::DetectedObject;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        id: msg.id,
        prob: msg.prob,
        box_: super::msg::BoundingBox2d::into_rmw_message(std::borrow::Cow::Owned(msg.box_)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      id: msg.id,
      prob: msg.prob,
        box_: super::msg::BoundingBox2d::into_rmw_message(std::borrow::Cow::Borrowed(&msg.box_)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      id: msg.id,
      prob: msg.prob,
      box_: super::msg::BoundingBox2d::from_rmw_message(msg.box_),
    }
  }
}


// Corresponds to combat_robot_msgs__msg__DetectedObjects
/// DetectedObjects.msg

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct DetectedObjects {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub image_width: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub image_height: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub objects: Vec<super::msg::DetectedObject>,

}



impl Default for DetectedObjects {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::DetectedObjects::default())
  }
}

impl rosidl_runtime_rs::Message for DetectedObjects {
  type RmwMsg = super::msg::rmw::DetectedObjects;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        image_width: msg.image_width,
        image_height: msg.image_height,
        objects: msg.objects
          .into_iter()
          .map(|elem| super::msg::DetectedObject::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      image_width: msg.image_width,
      image_height: msg.image_height,
        objects: msg.objects
          .iter()
          .map(|elem| super::msg::DetectedObject::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      image_width: msg.image_width,
      image_height: msg.image_height,
      objects: msg.objects
          .into_iter()
          .map(super::msg::DetectedObject::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to combat_robot_msgs__msg__TargetPoint

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct TargetPoint {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    pub box_: super::msg::BoundingBox2d,


    // This member is not documented.
    #[allow(missing_docs)]
    pub track_id: i32,

}



impl Default for TargetPoint {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::TargetPoint::default())
  }
}

impl rosidl_runtime_rs::Message for TargetPoint {
  type RmwMsg = super::msg::rmw::TargetPoint;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        is_locked: msg.is_locked,
        x: msg.x,
        y: msg.y,
        height: msg.height,
        class_id: msg.class_id,
        box_: super::msg::BoundingBox2d::into_rmw_message(std::borrow::Cow::Owned(msg.box_)).into_owned(),
        track_id: msg.track_id,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      is_locked: msg.is_locked,
      x: msg.x,
      y: msg.y,
      height: msg.height,
      class_id: msg.class_id,
        box_: super::msg::BoundingBox2d::into_rmw_message(std::borrow::Cow::Borrowed(&msg.box_)).into_owned(),
      track_id: msg.track_id,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      is_locked: msg.is_locked,
      x: msg.x,
      y: msg.y,
      height: msg.height,
      class_id: msg.class_id,
      box_: super::msg::BoundingBox2d::from_rmw_message(msg.box_),
      track_id: msg.track_id,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__TouchTargetPoint

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::TouchTargetPoint::default())
  }
}

impl rosidl_runtime_rs::Message for TouchTargetPoint {
  type RmwMsg = super::msg::rmw::TouchTargetPoint;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        touch_x: msg.touch_x,
        touch_y: msg.touch_y,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      touch_x: msg.touch_x,
      touch_y: msg.touch_y,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      touch_x: msg.touch_x,
      touch_y: msg.touch_y,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__UserCommand
/// Command command_from

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct UserCommand {
    /// UserCommand.msg
    /// This message is used to send user commands to the combat robot system.
    pub header: std_msgs::msg::Header,

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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::UserCommand::default())
  }
}

impl rosidl_runtime_rs::Message for UserCommand {
  type RmwMsg = super::msg::rmw::UserCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        command_from: msg.command_from,
        command_id: msg.command_id,
        target_x: msg.target_x,
        target_y: msg.target_y,
        drone_target_lat: msg.drone_target_lat,
        drone_target_lon: msg.drone_target_lon,
        drone_target_valid: msg.drone_target_valid,
        gun_trigger: msg.gun_trigger,
        gun_trigger_permission: msg.gun_trigger_permission,
        pan_speed: msg.pan_speed,
        tilt_speed: msg.tilt_speed,
        zoom_command: msg.zoom_command,
        stream_command: msg.stream_command,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      command_from: msg.command_from,
      command_id: msg.command_id,
      target_x: msg.target_x,
      target_y: msg.target_y,
      drone_target_lat: msg.drone_target_lat,
      drone_target_lon: msg.drone_target_lon,
      drone_target_valid: msg.drone_target_valid,
      gun_trigger: msg.gun_trigger,
      gun_trigger_permission: msg.gun_trigger_permission,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      zoom_command: msg.zoom_command,
      stream_command: msg.stream_command,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      command_from: msg.command_from,
      command_id: msg.command_id,
      target_x: msg.target_x,
      target_y: msg.target_y,
      drone_target_lat: msg.drone_target_lat,
      drone_target_lon: msg.drone_target_lon,
      drone_target_valid: msg.drone_target_valid,
      gun_trigger: msg.gun_trigger,
      gun_trigger_permission: msg.gun_trigger_permission,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      zoom_command: msg.zoom_command,
      stream_command: msg.stream_command,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__MissionControlCommand

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MissionControlCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::MissionControlCommand::default())
  }
}

impl rosidl_runtime_rs::Message for MissionControlCommand {
  type RmwMsg = super::msg::rmw::MissionControlCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        command_id: msg.command_id,
        estop_requested: msg.estop_requested,
        attack_permission: msg.attack_permission,
        pan_speed: msg.pan_speed,
        tilt_speed: msg.tilt_speed,
        zoom_command: msg.zoom_command,
        lateral_wind_speed: msg.lateral_wind_speed,
        drone_target_lat: msg.drone_target_lat,
        drone_target_lon: msg.drone_target_lon,
        drone_target_valid: msg.drone_target_valid,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      command_id: msg.command_id,
      estop_requested: msg.estop_requested,
      attack_permission: msg.attack_permission,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      zoom_command: msg.zoom_command,
      lateral_wind_speed: msg.lateral_wind_speed,
      drone_target_lat: msg.drone_target_lat,
      drone_target_lon: msg.drone_target_lon,
      drone_target_valid: msg.drone_target_valid,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      command_id: msg.command_id,
      estop_requested: msg.estop_requested,
      attack_permission: msg.attack_permission,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      zoom_command: msg.zoom_command,
      lateral_wind_speed: msg.lateral_wind_speed,
      drone_target_lat: msg.drone_target_lat,
      drone_target_lon: msg.drone_target_lon,
      drone_target_valid: msg.drone_target_valid,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__StreamControlCommand

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct StreamControlCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::StreamControlCommand::default())
  }
}

impl rosidl_runtime_rs::Message for StreamControlCommand {
  type RmwMsg = super::msg::rmw::StreamControlCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        stream_command: msg.stream_command,
        stream_target_robot_id: msg.stream_target_robot_id,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      stream_command: msg.stream_command,
      stream_target_robot_id: msg.stream_target_robot_id,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      stream_command: msg.stream_command,
      stream_target_robot_id: msg.stream_target_robot_id,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__SwarmControlCommand

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmControlCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::SwarmControlCommand::default())
  }
}

impl rosidl_runtime_rs::Message for SwarmControlCommand {
  type RmwMsg = super::msg::rmw::SwarmControlCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        formation_type: msg.formation_type,
        formation_number: msg.formation_number,
        grouping_index: msg.grouping_index,
        selected_robot_count: msg.selected_robot_count,
        selected_robot_ids: msg.selected_robot_ids,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      formation_type: msg.formation_type,
      formation_number: msg.formation_number,
      grouping_index: msg.grouping_index,
      selected_robot_count: msg.selected_robot_count,
        selected_robot_ids: msg.selected_robot_ids,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      formation_type: msg.formation_type,
      formation_number: msg.formation_number,
      grouping_index: msg.grouping_index,
      selected_robot_count: msg.selected_robot_count,
      selected_robot_ids: msg.selected_robot_ids,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__SwarmFollowerStatus

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmFollowerStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::SwarmFollowerStatus::default())
  }
}

impl rosidl_runtime_rs::Message for SwarmFollowerStatus {
  type RmwMsg = super::msg::rmw::SwarmFollowerStatus;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        robot_id: msg.robot_id,
        leader_robot_id: msg.leader_robot_id,
        link_status: msg.link_status,
        last_heartbeat_sequence: msg.last_heartbeat_sequence,
        heartbeat_age_sec: msg.heartbeat_age_sec,
        last_operation_mode: msg.last_operation_mode,
        last_formation_type: msg.last_formation_type,
        last_formation_number: msg.last_formation_number,
        last_grouping_index: msg.last_grouping_index,
        latitude: msg.latitude,
        longitude: msg.longitude,
        heading_deg: msg.heading_deg,
        ground_speed_mps: msg.ground_speed_mps,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      robot_id: msg.robot_id,
      leader_robot_id: msg.leader_robot_id,
      link_status: msg.link_status,
      last_heartbeat_sequence: msg.last_heartbeat_sequence,
      heartbeat_age_sec: msg.heartbeat_age_sec,
      last_operation_mode: msg.last_operation_mode,
      last_formation_type: msg.last_formation_type,
      last_formation_number: msg.last_formation_number,
      last_grouping_index: msg.last_grouping_index,
      latitude: msg.latitude,
      longitude: msg.longitude,
      heading_deg: msg.heading_deg,
      ground_speed_mps: msg.ground_speed_mps,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      robot_id: msg.robot_id,
      leader_robot_id: msg.leader_robot_id,
      link_status: msg.link_status,
      last_heartbeat_sequence: msg.last_heartbeat_sequence,
      heartbeat_age_sec: msg.heartbeat_age_sec,
      last_operation_mode: msg.last_operation_mode,
      last_formation_type: msg.last_formation_type,
      last_formation_number: msg.last_formation_number,
      last_grouping_index: msg.last_grouping_index,
      latitude: msg.latitude,
      longitude: msg.longitude,
      heading_deg: msg.heading_deg,
      ground_speed_mps: msg.ground_speed_mps,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__SwarmLeaderHeartbeat

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmLeaderHeartbeat {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::SwarmLeaderHeartbeat::default())
  }
}

impl rosidl_runtime_rs::Message for SwarmLeaderHeartbeat {
  type RmwMsg = super::msg::rmw::SwarmLeaderHeartbeat;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        sequence: msg.sequence,
        leader_robot_id: msg.leader_robot_id,
        operation_mode: msg.operation_mode,
        estop_active: msg.estop_active,
        formation_type: msg.formation_type,
        formation_number: msg.formation_number,
        grouping_index: msg.grouping_index,
        selected_robot_count: msg.selected_robot_count,
        selected_robot_ids: msg.selected_robot_ids,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      sequence: msg.sequence,
      leader_robot_id: msg.leader_robot_id,
      operation_mode: msg.operation_mode,
      estop_active: msg.estop_active,
      formation_type: msg.formation_type,
      formation_number: msg.formation_number,
      grouping_index: msg.grouping_index,
      selected_robot_count: msg.selected_robot_count,
        selected_robot_ids: msg.selected_robot_ids,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      sequence: msg.sequence,
      leader_robot_id: msg.leader_robot_id,
      operation_mode: msg.operation_mode,
      estop_active: msg.estop_active,
      formation_type: msg.formation_type,
      formation_number: msg.formation_number,
      grouping_index: msg.grouping_index,
      selected_robot_count: msg.selected_robot_count,
      selected_robot_ids: msg.selected_robot_ids,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__SwarmPathCommand

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmPathCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub command: u8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub num_waypoints: u16,


    // This member is not documented.
    #[allow(missing_docs)]
    pub path_json: std::string::String,

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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::SwarmPathCommand::default())
  }
}

impl rosidl_runtime_rs::Message for SwarmPathCommand {
  type RmwMsg = super::msg::rmw::SwarmPathCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        command: msg.command,
        num_waypoints: msg.num_waypoints,
        path_json: msg.path_json.as_str().into(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      command: msg.command,
      num_waypoints: msg.num_waypoints,
        path_json: msg.path_json.as_str().into(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      command: msg.command,
      num_waypoints: msg.num_waypoints,
      path_json: msg.path_json.to_string(),
    }
  }
}


// Corresponds to combat_robot_msgs__msg__SwarmRobotCommand

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SwarmRobotCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    pub path_id: std::string::String,


    // This member is not documented.
    #[allow(missing_docs)]
    pub path_json: std::string::String,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::SwarmRobotCommand::default())
  }
}

impl rosidl_runtime_rs::Message for SwarmRobotCommand {
  type RmwMsg = super::msg::rmw::SwarmRobotCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        sequence: msg.sequence,
        command_type: msg.command_type,
        leader_robot_id: msg.leader_robot_id,
        target_robot_id: msg.target_robot_id,
        operation_mode: msg.operation_mode,
        estop_requested: msg.estop_requested,
        path_command: msg.path_command,
        num_waypoints: msg.num_waypoints,
        path_id: msg.path_id.as_str().into(),
        path_json: msg.path_json.as_str().into(),
        formation_type: msg.formation_type,
        formation_number: msg.formation_number,
        grouping_index: msg.grouping_index,
        slot_index: msg.slot_index,
        selected_robot_count: msg.selected_robot_count,
        selected_robot_ids: msg.selected_robot_ids,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      sequence: msg.sequence,
      command_type: msg.command_type,
      leader_robot_id: msg.leader_robot_id,
      target_robot_id: msg.target_robot_id,
      operation_mode: msg.operation_mode,
      estop_requested: msg.estop_requested,
      path_command: msg.path_command,
      num_waypoints: msg.num_waypoints,
        path_id: msg.path_id.as_str().into(),
        path_json: msg.path_json.as_str().into(),
      formation_type: msg.formation_type,
      formation_number: msg.formation_number,
      grouping_index: msg.grouping_index,
      slot_index: msg.slot_index,
      selected_robot_count: msg.selected_robot_count,
        selected_robot_ids: msg.selected_robot_ids,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      sequence: msg.sequence,
      command_type: msg.command_type,
      leader_robot_id: msg.leader_robot_id,
      target_robot_id: msg.target_robot_id,
      operation_mode: msg.operation_mode,
      estop_requested: msg.estop_requested,
      path_command: msg.path_command,
      num_waypoints: msg.num_waypoints,
      path_id: msg.path_id.to_string(),
      path_json: msg.path_json.to_string(),
      formation_type: msg.formation_type,
      formation_number: msg.formation_number,
      grouping_index: msg.grouping_index,
      slot_index: msg.slot_index,
      selected_robot_count: msg.selected_robot_count,
      selected_robot_ids: msg.selected_robot_ids,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__IMUState

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct IMUState {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// Roll Pitch Yaw
    pub angle: geometry_msgs::msg::Vector3,


    // This member is not documented.
    #[allow(missing_docs)]
    pub is_connected: bool,

    /// "gun" or "car"
    pub device_id: std::string::String,

}



impl Default for IMUState {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::IMUState::default())
  }
}

impl rosidl_runtime_rs::Message for IMUState {
  type RmwMsg = super::msg::rmw::IMUState;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        angle: geometry_msgs::msg::Vector3::into_rmw_message(std::borrow::Cow::Owned(msg.angle)).into_owned(),
        is_connected: msg.is_connected,
        device_id: msg.device_id.as_str().into(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        angle: geometry_msgs::msg::Vector3::into_rmw_message(std::borrow::Cow::Borrowed(&msg.angle)).into_owned(),
      is_connected: msg.is_connected,
        device_id: msg.device_id.as_str().into(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      angle: geometry_msgs::msg::Vector3::from_rmw_message(msg.angle),
      is_connected: msg.is_connected,
      device_id: msg.device_id.to_string(),
    }
  }
}


// Corresponds to combat_robot_msgs__msg__OperationState
/// Internal ROS operation state values

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::OperationState::default())
  }
}

impl rosidl_runtime_rs::Message for OperationState {
  type RmwMsg = super::msg::rmw::OperationState;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        state: msg.state,
        active_mode_id: msg.active_mode_id,
        mission_status: msg.mission_status,
        estop_active: msg.estop_active,
        permission_request_active: msg.permission_request_active,
        crosshair_x: msg.crosshair_x,
        crosshair_y: msg.crosshair_y,
        current_zoom_level: msg.current_zoom_level,
        gps_lat: msg.gps_lat,
        gps_lon: msg.gps_lon,
        gps_heading: msg.gps_heading,
        current_speed_mps: msg.current_speed_mps,
        current_waypoint_index: msg.current_waypoint_index,
        total_waypoints: msg.total_waypoints,
        progress_ratio: msg.progress_ratio,
        distance_to_next_wp_m: msg.distance_to_next_wp_m,
        distance_to_goal_m: msg.distance_to_goal_m,
        error_code: msg.error_code,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      state: msg.state,
      active_mode_id: msg.active_mode_id,
      mission_status: msg.mission_status,
      estop_active: msg.estop_active,
      permission_request_active: msg.permission_request_active,
      crosshair_x: msg.crosshair_x,
      crosshair_y: msg.crosshair_y,
      current_zoom_level: msg.current_zoom_level,
      gps_lat: msg.gps_lat,
      gps_lon: msg.gps_lon,
      gps_heading: msg.gps_heading,
      current_speed_mps: msg.current_speed_mps,
      current_waypoint_index: msg.current_waypoint_index,
      total_waypoints: msg.total_waypoints,
      progress_ratio: msg.progress_ratio,
      distance_to_next_wp_m: msg.distance_to_next_wp_m,
      distance_to_goal_m: msg.distance_to_goal_m,
      error_code: msg.error_code,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      state: msg.state,
      active_mode_id: msg.active_mode_id,
      mission_status: msg.mission_status,
      estop_active: msg.estop_active,
      permission_request_active: msg.permission_request_active,
      crosshair_x: msg.crosshair_x,
      crosshair_y: msg.crosshair_y,
      current_zoom_level: msg.current_zoom_level,
      gps_lat: msg.gps_lat,
      gps_lon: msg.gps_lon,
      gps_heading: msg.gps_heading,
      current_speed_mps: msg.current_speed_mps,
      current_waypoint_index: msg.current_waypoint_index,
      total_waypoints: msg.total_waypoints,
      progress_ratio: msg.progress_ratio,
      distance_to_next_wp_m: msg.distance_to_next_wp_m,
      distance_to_goal_m: msg.distance_to_goal_m,
      error_code: msg.error_code,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__DriveCommand
/// combat_robot_msgs/msg/DriveCommand.msg

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct DriveCommand {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// m/s, forward(+)/backward(-)
    pub linear_velocity: f32,

    /// rad/s, left(+)/right(-)  ← 기존 부호 규칙 그대로 사용
    pub angular_velocity: f32,

}



impl Default for DriveCommand {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::DriveCommand::default())
  }
}

impl rosidl_runtime_rs::Message for DriveCommand {
  type RmwMsg = super::msg::rmw::DriveCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        linear_velocity: msg.linear_velocity,
        angular_velocity: msg.angular_velocity,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      linear_velocity: msg.linear_velocity,
      angular_velocity: msg.angular_velocity,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      linear_velocity: msg.linear_velocity,
      angular_velocity: msg.angular_velocity,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__PanTiltControlCommand

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct PanTiltControlCommand {
    /// Pan-Tilt Control Command message
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::PanTiltControlCommand::default())
  }
}

impl rosidl_runtime_rs::Message for PanTiltControlCommand {
  type RmwMsg = super::msg::rmw::PanTiltControlCommand;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        control_mode: msg.control_mode,
        horizontal_angle: msg.horizontal_angle,
        vertical_angle: msg.vertical_angle,
        pan_speed: msg.pan_speed,
        tilt_speed: msg.tilt_speed,
        pan_dir: msg.pan_dir,
        tilt_dir: msg.tilt_dir,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      control_mode: msg.control_mode,
      horizontal_angle: msg.horizontal_angle,
      vertical_angle: msg.vertical_angle,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      pan_dir: msg.pan_dir,
      tilt_dir: msg.tilt_dir,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      control_mode: msg.control_mode,
      horizontal_angle: msg.horizontal_angle,
      vertical_angle: msg.vertical_angle,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      pan_dir: msg.pan_dir,
      tilt_dir: msg.tilt_dir,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__PanTiltState

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct PanTiltState {

    // This member is not documented.
    #[allow(missing_docs)]
    pub stamp: builtin_interfaces::msg::Time,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::PanTiltState::default())
  }
}

impl rosidl_runtime_rs::Message for PanTiltState {
  type RmwMsg = super::msg::rmw::PanTiltState;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        stamp: builtin_interfaces::msg::Time::into_rmw_message(std::borrow::Cow::Owned(msg.stamp)).into_owned(),
        control_mode: msg.control_mode,
        horizontal_angle: msg.horizontal_angle,
        vertical_angle: msg.vertical_angle,
        pan_speed: msg.pan_speed,
        tilt_speed: msg.tilt_speed,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        stamp: builtin_interfaces::msg::Time::into_rmw_message(std::borrow::Cow::Borrowed(&msg.stamp)).into_owned(),
      control_mode: msg.control_mode,
      horizontal_angle: msg.horizontal_angle,
      vertical_angle: msg.vertical_angle,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      stamp: builtin_interfaces::msg::Time::from_rmw_message(msg.stamp),
      control_mode: msg.control_mode,
      horizontal_angle: msg.horizontal_angle,
      vertical_angle: msg.vertical_angle,
      pan_speed: msg.pan_speed,
      tilt_speed: msg.tilt_speed,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__CenterObject
/// CenterObject.msg

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct CenterObject {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub class_id: i32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub bounding_box: super::msg::BoundingBox2d,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::CenterObject::default())
  }
}

impl rosidl_runtime_rs::Message for CenterObject {
  type RmwMsg = super::msg::rmw::CenterObject;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        class_id: msg.class_id,
        bounding_box: super::msg::BoundingBox2d::into_rmw_message(std::borrow::Cow::Owned(msg.bounding_box)).into_owned(),
        target_x: msg.target_x,
        target_y: msg.target_y,
        laser_distance: msg.laser_distance,
        zoom_level: msg.zoom_level,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      class_id: msg.class_id,
        bounding_box: super::msg::BoundingBox2d::into_rmw_message(std::borrow::Cow::Borrowed(&msg.bounding_box)).into_owned(),
      target_x: msg.target_x,
      target_y: msg.target_y,
      laser_distance: msg.laser_distance,
      zoom_level: msg.zoom_level,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      class_id: msg.class_id,
      bounding_box: super::msg::BoundingBox2d::from_rmw_message(msg.bounding_box),
      target_x: msg.target_x,
      target_y: msg.target_y,
      laser_distance: msg.laser_distance,
      zoom_level: msg.zoom_level,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__Waypoint

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Waypoint::default())
  }
}

impl rosidl_runtime_rs::Message for Waypoint {
  type RmwMsg = super::msg::rmw::Waypoint;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        way_id: msg.way_id,
        way_lon: msg.way_lon,
        way_lat: msg.way_lat,
        way_status: msg.way_status,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      way_id: msg.way_id,
      way_lon: msg.way_lon,
      way_lat: msg.way_lat,
      way_status: msg.way_status,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      way_id: msg.way_id,
      way_lon: msg.way_lon,
      way_lat: msg.way_lat,
      way_status: msg.way_status,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__WaypointList

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
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
    pub waypoints: Vec<super::msg::Waypoint>,

}



impl Default for WaypointList {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::WaypointList::default())
  }
}

impl rosidl_runtime_rs::Message for WaypointList {
  type RmwMsg = super::msg::rmw::WaypointList;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        mode: msg.mode,
        formation: msg.formation,
        mission_id: msg.mission_id,
        mission_status: msg.mission_status,
        waypoints: msg.waypoints
          .into_iter()
          .map(|elem| super::msg::Waypoint::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      mode: msg.mode,
      formation: msg.formation,
      mission_id: msg.mission_id,
      mission_status: msg.mission_status,
        waypoints: msg.waypoints
          .iter()
          .map(|elem| super::msg::Waypoint::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      mode: msg.mode,
      formation: msg.formation,
      mission_id: msg.mission_id,
      mission_status: msg.mission_status,
      waypoints: msg.waypoints
          .into_iter()
          .map(super::msg::Waypoint::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to combat_robot_msgs__msg__GnssStatus
/// GNSS / RTK 드라이버가 publish 하는 status.
/// robot_server 가 /gnss/status 토픽으로 subscribe 하여 leader 위치/heading/speed 와
/// 앱 상태 패킷의 fix 품질 표시에 사용함.
///
/// 모든 필드를 매 주기 채워 보내주세요. 값을 알 수 없으면 아래 정의된 INVALID 값을 사용.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct GnssStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::GnssStatus::default())
  }
}

impl rosidl_runtime_rs::Message for GnssStatus {
  type RmwMsg = super::msg::rmw::GnssStatus;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        fix_status: msg.fix_status,
        num_satellites: msg.num_satellites,
        latitude: msg.latitude,
        longitude: msg.longitude,
        altitude_m: msg.altitude_m,
        heading_deg: msg.heading_deg,
        ground_speed_mps: msg.ground_speed_mps,
        horizontal_accuracy_m: msg.horizontal_accuracy_m,
        vertical_accuracy_m: msg.vertical_accuracy_m,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      fix_status: msg.fix_status,
      num_satellites: msg.num_satellites,
      latitude: msg.latitude,
      longitude: msg.longitude,
      altitude_m: msg.altitude_m,
      heading_deg: msg.heading_deg,
      ground_speed_mps: msg.ground_speed_mps,
      horizontal_accuracy_m: msg.horizontal_accuracy_m,
      vertical_accuracy_m: msg.vertical_accuracy_m,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      fix_status: msg.fix_status,
      num_satellites: msg.num_satellites,
      latitude: msg.latitude,
      longitude: msg.longitude,
      altitude_m: msg.altitude_m,
      heading_deg: msg.heading_deg,
      ground_speed_mps: msg.ground_speed_mps,
      horizontal_accuracy_m: msg.horizontal_accuracy_m,
      vertical_accuracy_m: msg.vertical_accuracy_m,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__ChassisStatus
/// 차량(chassis) 컨트롤러가 publish 하는 status.
/// robot_server 가 /chassis/status 토픽으로 subscribe 하여 BMS / 구동 상태를
/// 앱 상태 패킷의 battery_pct, velocity 등에 반영함.
///
/// 가능한 한 모든 필드를 매 주기 채워 보내주세요. 값을 모르면 0 또는 NaN 사용 가능.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct ChassisStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::ChassisStatus::default())
  }
}

impl rosidl_runtime_rs::Message for ChassisStatus {
  type RmwMsg = super::msg::rmw::ChassisStatus;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        drive_state: msg.drive_state,
        battery_pct: msg.battery_pct,
        battery_voltage_v: msg.battery_voltage_v,
        battery_current_a: msg.battery_current_a,
        linear_velocity_mps: msg.linear_velocity_mps,
        angular_velocity_rps: msg.angular_velocity_rps,
        fault_flags: msg.fault_flags,
        motor_temp_c: msg.motor_temp_c,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      drive_state: msg.drive_state,
      battery_pct: msg.battery_pct,
      battery_voltage_v: msg.battery_voltage_v,
      battery_current_a: msg.battery_current_a,
      linear_velocity_mps: msg.linear_velocity_mps,
      angular_velocity_rps: msg.angular_velocity_rps,
      fault_flags: msg.fault_flags,
      motor_temp_c: msg.motor_temp_c,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      drive_state: msg.drive_state,
      battery_pct: msg.battery_pct,
      battery_voltage_v: msg.battery_voltage_v,
      battery_current_a: msg.battery_current_a,
      linear_velocity_mps: msg.linear_velocity_mps,
      angular_velocity_rps: msg.angular_velocity_rps,
      fault_flags: msg.fault_flags,
      motor_temp_c: msg.motor_temp_c,
    }
  }
}


// Corresponds to combat_robot_msgs__msg__LidarStatus
/// LiDAR 드라이버가 publish 하는 status.
/// robot_server 가 /lidar/status 토픽으로 subscribe 하여 LiDAR 헬스 / 장애물 정보를
/// 추후 status 패킷에 반영하거나 안전 분기 입력으로 활용함.
///
/// 가능한 한 모든 필드를 매 주기 채워 보내주세요.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct LidarStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


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
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::LidarStatus::default())
  }
}

impl rosidl_runtime_rs::Message for LidarStatus {
  type RmwMsg = super::msg::rmw::LidarStatus;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        status: msg.status,
        last_scan_point_count: msg.last_scan_point_count,
        scan_rate_hz: msg.scan_rate_hz,
        obstacle_detected: msg.obstacle_detected,
        min_obstacle_distance_m: msg.min_obstacle_distance_m,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
      status: msg.status,
      last_scan_point_count: msg.last_scan_point_count,
      scan_rate_hz: msg.scan_rate_hz,
      obstacle_detected: msg.obstacle_detected,
      min_obstacle_distance_m: msg.min_obstacle_distance_m,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      status: msg.status,
      last_scan_point_count: msg.last_scan_point_count,
      scan_rate_hz: msg.scan_rate_hz,
      obstacle_detected: msg.obstacle_detected,
      min_obstacle_distance_m: msg.min_obstacle_distance_m,
    }
  }
}


