#include "main.h"
#include "v5_video_player.hpp"
#include <algorithm>
#include <cmath>

constexpr int8_t LEFT_FRONT_PORT  = 1;
constexpr int8_t LEFT_BACK_PORT   = 2;
constexpr int8_t RIGHT_FRONT_PORT = -9;
constexpr int8_t RIGHT_BACK_PORT  = -10;

constexpr int DEADZONE = 5;

static int exp_curve(int input) {
    if (std::abs(input) <= DEADZONE) return 0;
    double norm = input / 127.0;
    return static_cast<int>(norm * norm * norm * 127.0);
}

static pros::Task* video_task = nullptr;

static void start_video() {
    if (!video_task) {
        video_task = new pros::Task([]() {
            v5_video::play_video("/usd/video.v5y");
        }, "VideoPlayer");
    }
}

void initialize() {}

void disabled() {
    start_video();
}

void competition_initialize() {}

void autonomous() {
    start_video();
    pros::MotorGroup left_mg({LEFT_FRONT_PORT, LEFT_BACK_PORT});
    pros::MotorGroup right_mg({RIGHT_FRONT_PORT, RIGHT_BACK_PORT});

    left_mg.move(100);
    right_mg.move(100);
    pros::delay(1500);

    left_mg.move(0);
    right_mg.move(0);
}

void opcontrol() {
    start_video();

    pros::MotorGroup left_mg({LEFT_FRONT_PORT, LEFT_BACK_PORT});
    pros::MotorGroup right_mg({RIGHT_FRONT_PORT, RIGHT_BACK_PORT});
    pros::Controller master(pros::E_CONTROLLER_MASTER);

    int brake_state = 0;
    left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
    right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);

    while (true) {
        int throttle = exp_curve(master.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y));
        int turn     = exp_curve(master.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X));

        int left  = std::clamp(throttle + turn, -127, 127);
        int right = std::clamp(throttle - turn, -127, 127);

        left_mg.move(left);
        right_mg.move(right);

        if (master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_X)) {
            brake_state = (brake_state + 1) % 3;
            if (brake_state == 0) {
                left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
                right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
                master.rumble(".");
            } else if (brake_state == 1) {
                left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
                right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
                master.rumble("..");
            } else {
                left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
                right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
                master.rumble("...");
            }
        }

        pros::delay(10);
    }
}
