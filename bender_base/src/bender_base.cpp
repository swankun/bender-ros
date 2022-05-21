/**
 *
 *  \file
 *  \brief      Main entry point for bender base, based on jackal_base
 *  \author     Mike Purvis <mpurvis@clearpathrobotics.com>
 *  \author     Wankun Sirichotiyakul <wankunsirichotiyakul@u.boisestate.edu>
 *  \copyright  Copyright (c) 2013, Clearpath Robotics, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Clearpath Robotics, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CLEARPATH ROBOTICS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Please send comments, questions, or patches to code@clearpathrobotics.com
 *
 */


#include <string>

#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>

#include <controller_manager/controller_manager.h>
#include <ros/ros.h>
#include <rosserial_server/serial_session.h>

#include "bender_base/hardware_interface.h"

typedef boost::chrono::steady_clock time_source;

void controlThread(ros::Rate rate, 
  bender_base::BenderHardware* robot, 
  controller_manager::ControllerManager* cm, 
  diagnostic_updater::Updater* diag
)
{
  time_source::time_point last_time = time_source::now();

  while (1)
  {
    // Calculate monotonic time elapsed
    time_source::time_point this_time = time_source::now();
    boost::chrono::duration<double> elapsed_duration = this_time - last_time;
    ros::Duration elapsed(elapsed_duration.count());
    last_time = this_time;

    robot->read();
    cm->update(ros::Time::now(), elapsed);
    robot->write();
    diag->update();
    rate.sleep();
  }
}

int main(int argc, char* argv[])
{
  // Initialize ROS node.
  ros::init(argc, argv, "bender_base_node");
  bender_base::BenderHardware bender;

  // Create the serial rosserial server in a background ASIO event loop.
  std::string port;
  ros::param::param<std::string>("~port", port, "/dev/ttyACM0");
  boost::asio::io_service io_service;
  new rosserial_server::SerialSession(io_service, port, 115200);
  boost::thread(boost::bind(&boost::asio::io_service::run, &io_service));

  // Create diagnostic updater, to update itself on the ROS thread.
  diagnostic_updater::Updater diag;
  diag.add("Bender diagnostic", &bender, &bender_base::BenderHardware::produce_diagnostics);

  // Background thread for the controls callback.
  ros::NodeHandle controller_nh("/bender");
  controller_manager::ControllerManager cm(&bender, controller_nh);
  boost::thread(boost::bind(controlThread, ros::Rate(50), &bender, &cm, &diag));


  // Foreground ROS spinner for ROS callbacks, including rosserial, diagnostics
  ros::spin();

  return 0;
}
