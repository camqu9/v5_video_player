#include "main.h"
#include "v5_video_player.hpp"
#include <cmath>

// ============================================================================
// DRIVETRAIN CONFIGURATION
// ============================================================================
// Adjust motor ports to match your cabling. Negative '-' reverses motor direction.
constexpr int8_t LEFT_FRONT_PORT  = 1;
constexpr int8_t LEFT_BACK_PORT   = 2;
constexpr int8_t RIGHT_FRONT_PORT = -9;   // Negative = reversed direction
constexpr int8_t RIGHT_BACK_PORT  = -10;  // Negative = reversed direction

// Deadband to ignore joystick drift around center
constexpr int DEADZONE = 5;

// Helper: Applies exponential power curve for smooth low-speed control & precision
static int exp_curve(int input) {
    if (std::abs(input) <= DEADZONE) return 0;
    double normalized = static_cast<double>(input) / 127.0;
    double curved = normalized * normalized * normalized;
    return static_cast<int>(curved * 127.0);
}

// Background task handle for video player
static pros::Task* video_task_ptr = nullptr;

static void start_background_video() {
    if (!video_task_ptr) {
        video_task_ptr = new pros::Task([]() {
            v5_video::play_video("/usd/video.v5y");
        }, "V5 Video Player");
    }
}

void initialize() {
    printf("[RobotInit] Initializing 4-Motor Drivetrain...\n");
}

void disabled() {
    start_background_video();
}

void competition_initialize() {}

void autonomous() {
    start_background_video();
    // Simple autonomous routine: Drive forward for 1.5s then stop
    pros::MotorGroup left_mg({LEFT_FRONT_PORT, LEFT_BACK_PORT});
    pros::MotorGroup right_mg({RIGHT_FRONT_PORT, RIGHT_BACK_PORT});

    left_mg.move(100);
    right_mg.move(100);
    pros::delay(1500);

    left_mg.move(0);
    right_mg.move(0);
}

void opcontrol() {
    // Start background video playback on LCD
    start_background_video();

    // Instantiate drivetrain motor groups
    pros::MotorGroup left_mg({LEFT_FRONT_PORT, LEFT_BACK_PORT});
    pros::MotorGroup right_mg({RIGHT_FRONT_PORT, RIGHT_BACK_PORT});

    // Master controller
    pros::Controller master(pros::E_CONTROLLER_MASTER);

    // Brake mode state tracking (0 = COAST, 1 = BRAKE, 2 = HOLD)
    int brake_mode_state = 0;
    left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
    right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);

    printf("[OpControl] Driver control active (Split Arcade Drive).\n");

    while (true) {
        // --- 1. Read Joysticks (Split Arcade Drive) -------------------------
        int throttle = master.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y);   // Forward/Reverse
        int turn     = master.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);  // Left/Right Turn

        // --- 2. Apply Exponential Curve & Deadband --------------------------
        int throttle_curved = exp_curve(throttle);
        int turn_curved     = exp_curve(turn);

        // --- 3. Arcade Drive Kinematics -------------------------------------
        int left_power  = throttle_curved + turn_curved;
        int right_power = throttle_curved - turn_curved;

        // Clamp power values between -127 and +127
        left_power  = std::clamp(left_power,  -127, 127);
        right_power = std::clamp(right_power, -127, 127);

        // Command motors
        left_mg.move(left_power);
        right_mg.move(right_power);

        // --- 4. Brake Mode Toggle (Button X) --------------------------------
        if (master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_X)) {
            brake_mode_state = (brake_mode_state + 1) % 3;
            if (brake_mode_state == 0) {
                left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
                right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_COAST);
                master.rumble(".");
                printf("[Drive] Brake Mode: COAST\n");
            } else if (brake_mode_state == 1) {
                left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
                right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_BRAKE);
                master.rumble("..");
                printf("[Drive] Brake Mode: BRAKE\n");
            } else {
                left_mg.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
                right_mg.set_brake_mode(pros::E_MOTOR_BRAKE_HOLD);
                master.rumble("...");
                printf("[Drive] Brake Mode: HOLD\n");
            }
        }

        // Loop delay for RTOS task scheduling
        pros::delay(10);
    }
}
