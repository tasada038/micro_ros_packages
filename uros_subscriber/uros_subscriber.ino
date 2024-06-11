//-----------------------------------------------------------------------------------
// MIT License

// Copyright (c) 2024 Takumi Asada

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//-----------------------------------------------------------------------------------

/* ---------- References ---------- */
// https://github.com/StarlingUAS/ProjectStarling/blob/e2cd2905a2ae465121392f2c4960727e205d1d2f/system/safety_button/safety_button.ino#L257
// https://github.com/husarion/rosbot_ros2_firmware/blob/0ed8ae81f0a59d244a42c6db76bc03e046d63208/src/main.cpp#L353
// ----- IMU -----
// https://github.com/ashok-kummar/microros_imu_publisher/blob/main/imu_publisher.ino
// ----- Geometry -----
// https://github.com/microsoft/Glide/blob/1a1439858af53874fd678a568174d5c36931697a/ws/src/teensy-microros/microros/microros.ino#L3
// ----- Joint state -----
// https://github.com/rt-net/pico_micro_ros_arduino_examples/blob/0e8f599d1b6065b57bb8a6ed10ecba28ab106e46/uROS_STEP10_tfMsg/uROS_STEP10_tfMsg.ino#L16
// micro ros raspi pico sdk
// https://github.com/micro-ROS/micro_ros_raspberrypi_pico_sdk/tree/humble
//-----------------------------------------------------------------------------------

#include <micro_ros_arduino.h>

#include <M5Stack.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/int32.h>

#include <rmw_microros/rmw_microros.h>

rcl_publisher_t publisher;
rcl_subscription_t subscriber;
std_msgs__msg__Int32 msg;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;
rcl_node_options_t node_ops; // Foxy
rcl_init_options_t init_options; // Humble
size_t domain_id = 89;

static unsigned long long prev_connect_test_time;
bool micro_ros_init_successful;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn;}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}


void subscription_callback(const void * msgin)
{  
  // TODO M5stack
  // M5.Lcd.printf("micro ROS2 Data:%d \n",msg.data);

  if (msg.data == 0){
    M5.Lcd.drawJpgFile(SD, "/img/green_status.jpg");  
  }
  else if (msg.data == 1){
    M5.Lcd.drawJpgFile(SD, "/img/yellow_status.jpg");
  }
  else if (msg.data == 2){
    M5.Lcd.drawJpgFile(SD, "/img/blue_status.jpg");
  }

  else if (msg.data == 3){
    M5.Lcd.drawJpgFile(SD, "/img/red_status.jpg");
  }

}

bool create_entities(){
  allocator = rcl_get_default_allocator();

  /* Humble */
  // create init_options
  init_options = rcl_get_zero_initialized_init_options();
  RCCHECK(rcl_init_options_init(&init_options, allocator)); // <--- This was missing on ur side
  // Set ROS domain id
  RCCHECK(rcl_init_options_set_domain_id(&init_options, domain_id));
  // Setup support structure.
  RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));
  // create node
  RCCHECK(rclc_node_init_default(&node, "micro_ros_arduino_node", "", &support));

  // create subscriber
  RCCHECK(rclc_subscription_init_default(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "/micro_ros_arduino_data"));

  // create executor
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &msg, &subscription_callback, ON_NEW_DATA));
  micro_ros_init_successful = true;
  return true;
}

void destroy_entities(){
    // rcl_publisher_fini(&mission_start_publisher, &node);
    rcl_subscription_fini(&subscriber, &node);
    rcl_node_fini(&node);
    rcl_timer_fini(&timer);
    rclc_executor_fini(&executor);
    rclc_support_fini(&support);
    micro_ros_init_successful = false;
}

void setup() {
  // TODO M5stack
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("micro ROS2 M5Stack START\n");

  set_microros_transports();
  micro_ros_init_successful = false;
}

void loop() {
    // check if the agent got disconnected at 20Hz
    // if(micros() - prev_connect_test_time > 200000)
    if(millis() - prev_connect_test_time >= 100)
    {
        // prev_connect_test_time = micros();
        prev_connect_test_time = millis();
        // check if the agent is connected
        // param[in] timeout_ms Timeout in ms (per attempt).
        // param[in] attempts Number of tries before considering the ping as failed.
        if(RMW_RET_OK == rmw_uros_ping_agent(50, 2))
        {
            // reconnect if agent got disconnected or haven't at all
            if (!micro_ros_init_successful) 
            {
              create_entities();
            } 
        } 
        else if(micro_ros_init_successful)
        {
            // clean up micro-ROS components
            destroy_entities();
        } 
    }
    
    if(micro_ros_init_successful)
    {
      RCCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));
    } else {
        // flash_leds();
    }
    delay(100);
}
