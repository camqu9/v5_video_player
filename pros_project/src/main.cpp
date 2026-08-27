#include "main.h"
#include "v5_video_player.hpp"

#define LEFT_FRONT_PORT   1
#define LEFT_BACK_PORT    2
#define RIGHT_FRONT_PORT  -9
#define RIGHT_BACK_PORT   -10

const int DEADZONE = 5;

int apply_drive_curve(int input) {
    if (abs(input) < DEADZONE) return 0;
    double normalized = input / 127.0;
    return (int)(normalized * normalized * normalized * 127.0);
}

pros::Task* video_task = nullptr;

void start_video_player() {
    if (video_task == nullptr) {
        video_task = new pros::Task([]() {
            v5_video::play_video("/usd/video.v5y");
        });
    }
}

void initialize() {}

void disabled() {
    start_video_player();
}

void competition_initialize() {}

void autonomous() {
    start_video_player();

    pros::MotorGroup left_motors({LEFT_FRONT_PORT, LEFT_BACK_PORT});
    pros::MotorGroup right_motors({RIGHT_FRONT_PORT, RIGHT_BACK_PORT});

    left_motors.move(100);
    right_motors.move(100);
    pros::delay(1500);

    left_motors.move(0);
    right_motors.move(0);
}

void opcontrol() {
    start_video_player();

    pros::MotorGroup left_motors({LEFT_FRONT_PORT, LEFT_BACK_PORT});
    pros::MotorGroup right_motors({RIGHT_FRONT_PORT, RIGHT_BACK_PORT});
    pros::Controller master(pros::E_CONTROLLER_MASTER);

    int brake_mode = 0;

    while (true) {
        int power = master.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);
        int turn  = master.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);

        power = apply_drive_curve(power);
        turn  = apply_drive_curve(turn);

        int left_power  = power + turn;
        int right_power = power - turn;

        left_motors.move(left_power);
        right_motors.move(right_power);

        if (master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_X)) {
            brake_mode = (brake_mode + 1) % 3;

            if (brake_mode == 0) {
                left_motors.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
                right_motors.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
                master.rumble(".");
            } else if (brake_mode == 1) {
                left_motors.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
                right_motors.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
                master.rumble("..");
            } else if (brake_mode == 2) {
                left_motors.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
                right_motors.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
                master.rumble("...");
            }
        }

        pros::delay(10);
    }
}
