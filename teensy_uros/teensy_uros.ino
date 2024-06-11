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

/* ----- Terminal Command ----- */
// ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -v6
// ros2 topic echo /uros/fsr_data
// ros2 topic echo /uros/sos_data
// ros2 topic echo /uros/tds_data

/* ----- Include ----- */
#include <micro_ros_arduino.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>

#include <std_msgs/msg/float32.h>

/* ----- Define ----- */
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){return false;}}

/* ----- Pin number ----- */
#define SOSPIN 23
#define TDSPIN 22
#define FSRPIN 21

/* ----- Template Empty Message ----- */
std_msgs__msg__Float32 tds_msg;
std_msgs__msg__Float32 sos_msg;
std_msgs__msg__Float32 fsr_msg;

/* ----- Publisher ----- */
rcl_publisher_t tds_publisher;
rcl_publisher_t sos_publisher;
rcl_publisher_t fsr_publisher;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

/* ----- Node Options ----- */
rcl_node_options_t node_ops; // Foxy
rcl_init_options_t init_options; // Humble
size_t domain_id = 89;
static unsigned long long prev_connect_test_time;
bool micro_ros_init_successful;

/* ----- timer_callback ----- */
void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    float tds_val = analogRead(TDSPIN);
    float sos_val = digitalRead(SOSPIN);
    float fsr_val = analogRead(FSRPIN);

    RCSOFTCHECK(rcl_publish(&tds_publisher, &tds_msg, NULL));
    RCSOFTCHECK(rcl_publish(&sos_publisher, &sos_msg, NULL));
    RCSOFTCHECK(rcl_publish(&fsr_publisher, &fsr_msg, NULL));
    tds_msg.data = tds_val;
    sos_msg.data = sos_val;
    fsr_msg.data = fsr_val;
  }
}

bool create_entities(){
  allocator = rcl_get_default_allocator();

  /* ----- ROS 2 Humble ----- */
  // create init_options
  init_options = rcl_get_zero_initialized_init_options();
  RCCHECK(rcl_init_options_init(&init_options, allocator)); // <--- This was missing on ur side
  // Set ROS domain id
  RCCHECK(rcl_init_options_set_domain_id(&init_options, domain_id));
  // Setup support structure.
  RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));
  // create node
  RCCHECK(rclc_node_init_default(&node, "micro_ros_arduino_node", "", &support));

  /* ----- create fsr_publisher ----- */
  RCCHECK(rclc_publisher_init_default(
            &tds_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
            "/uros/tds_data"));

  /* ----- create sos_publisher ----- */
  RCCHECK(rclc_publisher_init_default(
            &sos_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
            "/uros/sos_data"));

  /* ----- create fsr_publisher ----- */
  RCCHECK(rclc_publisher_init_default(
            &fsr_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
            "/uros/fsr_data"));

  /* ----- create timer ----- */
  const unsigned int timer_timeout = 100;
  RCCHECK(rclc_timer_init_default(
            &timer,
            &support,
            RCL_MS_TO_NS(timer_timeout),
            timer_callback));

  /* ----- create executor ----- */
  /* The number of communication objects num_handles = #publihser + #timers */
  unsigned int num_handles = 3 + 1;
  RCCHECK(rclc_executor_init(&executor, &support.context, num_handles, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  micro_ros_init_successful = true;
  return true;
}

/* ----- destroy entities ----- */
void destroy_entities(){
    rcl_publisher_fini(&tds_publisher, &node);
    rcl_publisher_fini(&sos_publisher, &node);
    rcl_publisher_fini(&fsr_publisher, &node);
    // rcl_subscription_fini(&subscriber, &node);
    rcl_node_fini(&node);
    rcl_timer_fini(&timer);
    rclc_executor_fini(&executor);
    rclc_support_fini(&support);

    micro_ros_init_successful = false;
}

/* ----- setup ----- */
void setup() {
  pinMode(TDSPIN,INPUT);
  pinMode(SOSPIN,INPUT);
  pinMode(FSRPIN, INPUT);

  set_microros_transports();
  micro_ros_init_successful = false;
}

/* ----- loop ----- */
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
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
    } else {
        // flash_leds();
    }
    delay(100);
}
