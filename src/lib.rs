#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![feature(async_await)]


mod error;
mod dbus;
mod rpc;
mod mio;
mod pcan;
pub mod bindings;
// mod analog;
// pub use analog::AIN;


// pub use mio::*;
// pub use pcan::*;
// pub use simulation as io;



mod simulation;
// mod pump;
// pub use sensor::*;
// pub use control::*;

pub use self::error::MioError;
pub use self::mio::*;
pub use self::rpc::*;
pub use self::dbus::*;

extern { fn analog_get_in01(); }




#[cfg(test)]
mod tests {
  use super::bindings::*;
    #[test]
    fn capi_test() {
      unsafe{
        let in01 = analog_get_in01(2);
        println!("ANALOG IN01:{}",in01);
        assert_eq!(in01,887);
      }
    }
}
//

//end
