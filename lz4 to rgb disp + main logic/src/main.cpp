#include "main.h"
#include "pros/abstract_motor.hpp"
#include "pros/misc.h"
#include "pros/misc.hpp"
#include "pros/motor_group.hpp"
#include "pros/motors.hpp"
#include "pros/rtos.hpp"
#include "v5p.hpp"
#include "lemlib/api.hpp"
#include "pros/ai_vision.h"
#include <algorithm>
#include <chrono>
#include "pros/adi.hpp"
#include "pros/optical.hpp"
V5P furry;
pros::adi::DigitalIn STOP_FUCKING_STRIPING_NOW('b');
pros::Controller master (pros::E_CONTROLLER_MASTER);
pros::MotorGroup furry_1 ({11,12});
pros::MotorGroup furry_2 ({-9,-10});
pros::AIVision catgirl_datecter(2);
pros::MotorGroup dr4b ({5,6});
pros::adi::Pneumatics air ('a',false);
pros::Motor spinerclaw (1);
pros::Optical catgirl_opti(7);
pros::Imu imu(3);
lemlib::ControllerSettings linearController(10, 0, 3, 3, 1, 100, 3, 500, 20);
lemlib::ControllerSettings angularController(2, 0, 10, 3, 1, 100, 3, 500, 0);
lemlib::Drivetrain furtrain(&furry_1,&furry_2,12.5, lemlib::Omniwheel::NEW_325,  200, 2);
lemlib::TrackingWheel left_tracking_wheel(&furry_1, lemlib::Omniwheel::NEW_325, -6.25, 200);
lemlib::TrackingWheel right_tracking_wheel(&furry_2, lemlib::Omniwheel::NEW_325, 6.25, 200);
lemlib::OdomSensors purrspeed(&left_tracking_wheel,&right_tracking_wheel,nullptr,nullptr,&imu );
lemlib::Chassis chassis(furtrain, linearController, angularController, purrspeed);
/**
 * A callback function for LLEMU's center button.
 *
 * When this callback is fired, it will toggle line 2 of the LCD text between
 * "I was pressed!" and nothing.
 */
void on_center_button() {
}

/**
 * Runs initialization code. This occurs as soon as the program is started.
 * 
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */
void initialize() {
pros::Task funnythingvideo([]{
  furry.video("/usd/drhouse.v5p");
});
}
void disabled() {}

/**
 * Runs after initialize(), and before autonomous when connected to the Field
 * Management System or the VEX Competition Switch. This is intended for
 * competition-specific initialization routines, such as an autonomous selector
 * on the LCD.
 *
 * This task will exit when the robot is enabled and autonomous or opcontrol
 * starts.
 */
void competition_initialize() {
pros::Task funnythingvideo([]{
  furry.video("/usd/drhouse.v5p");
});
}
/**
 * Runs the user autonomous code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the autonomous
 * mode. Alternatively, this function may be called in initialize or opcontrol
 * for non-competition testing purposes.
 *
 * If the robot is disabled or communications is lost, the autonomous task
 * will be stopped. Re-enabling the robot will restart the task, not re-start it
 * from where it left off.
 */
void autonomous() {
double hue = catgirl_opti.get_hue(); 
bool reh = (hue < 30 || hue > 330);
bool bluh = (hue > 180 && hue < 260); 
	catgirl_datecter.enable_detection_types(pros::AivisionModeType::tags);
	while (true) {
	
	bool meow = false;
 for (auto &girlcat : catgirl_datecter.get_all_objects()){
	if (!pros::AIVision::is_type(girlcat, pros::AivisionDetectType::tag)) continue;
	
	auto &t = girlcat.object.tag;
int girlcat_x = (t.x0 + t.x1 + t.x2 + t.x3) / 4;
int go_towards_gay_furrys = (girlcat_x - 160) / 2;
furry_1.move(std::clamp(60 - go_towards_gay_furrys, -127, 127));
furry_2.move(std::clamp(60 - go_towards_gay_furrys, -127, 127));
meow = true;
break; 

 }
 if (!meow)
{
	furry_1.move(0);
	furry_2.move(0);

} pros::delay(20);}
}
;
/**
 * Runs the operator control code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the operator
 * control mode.
 *
 * If no competition control is connected, this function will run immediately
 * following initialize().
 *
 * If the robot is disabled or communications is lost, the
 * operator control task will be stopped. Re-enabling the robot will restart the
 * task, not resume it from where it left off.
 */
void opcontrol() {
	dr4b.set_brake_mode(pros::MotorBrake::hold);
	catgirl_datecter.reset();
	while (true) {
		int dr4b_up = master.get_digital(pros::E_CONTROLLER_DIGITAL_L1);
		int dr4b_down = master.get_digital(pros::E_CONTROLLER_DIGITAL_L2);
		if (STOP_FUCKING_STRIPING_NOW.get_value() == 1) { dr4b_down = 0; }
		int girlcat = master.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
		int spiner_cat = master.get_analog(pros::E_CONTROLLER_ANALOG_LEFT_Y) * 2;
		int air_go = master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_R1);
		int claw_go = master.get_digital_new_press(pros::E_CONTROLLER_DIGITAL_R2);
		static bool air_state = false;
		static bool claw_state = false;
        chassis.arcade(spiner_cat,girlcat);
		dr4b.move_voltage((dr4b_up - dr4b_down) * 12000);

		if (air_go) {
			air_state = !air_state;
			if (air_state) {
				air.extend();
			} else {
				air.retract();
			}
		}

		if (claw_go) {
			claw_state = !claw_state;
			if (claw_state) {
				spinerclaw.move_absolute(1098, 255);
			} else {
				spinerclaw.move_absolute(0, 255
			);
			}
		}
		pros::delay(20);
	}
}