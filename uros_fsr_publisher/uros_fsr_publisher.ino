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

#include <micro_ros_arduino.h>

#include <M5Stack.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/float32.h>

#include <rmw_microros/rmw_microros.h>

rcl_publisher_t publisher;
rcl_subscription_t subscriber;
std_msgs__msg__Float32 msg;
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

#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){return false;}}


void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    float val1 = analogRead(21);
    float val2 = analogReadMilliVolts(36);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.drawNumber(val1, 160,  60, 8);
    M5.Lcd.drawNumber(val2, 160, 180, 8);
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
    msg.data = val1;
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

  // create publisher
  RCCHECK(rclc_publisher_init_default(
            &publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
            "/uros/fsr_data"));

  // create timer,
  const unsigned int timer_timeout = 100;
  RCCHECK(rclc_timer_init_default(
            &timer,
            &support,
            RCL_MS_TO_NS(timer_timeout),
            timer_callback));


  // create executor
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  micro_ros_init_successful = true;
  return true;
}

void destroy_entities(){
    rcl_publisher_fini(&publisher, &node);
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
  pinMode(36, INPUT);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(4);
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
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
    } else {
        // flash_leds();
    }
    delay(100);
}
