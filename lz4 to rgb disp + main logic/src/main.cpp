#include "main.h"
#include "pros/misc.h"
#include "pros/misc.hpp"
#include "pros/motor_group.hpp"
#include "pros/motors.hpp"
#include "pros/rtos.hpp"
#include "v5p.hpp"
#include "lemlib/api.hpp"
#include "pros/ai_vision.hpp"
#include <algorithm>
V5P furry;
pros::Controller master (pros::E_CONTROLLER_MASTER);
pros::MotorGroup furry_1 ({11,12});
pros::MotorGroup furry_2 ({9,10});
pros::AIVision catgirl_datecter(5);
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
  furry.video("/usd/girls_like_us.v5p");
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
void competition_initialize() {}

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
while (true){
	bool meow = false;
 for (auto &girlcat : catgirl_datecter.get_all_objects()){
	if (!pros::AIVision::is_type(girlcat, pros::AivisionDetectType::tag)) continue;
	
	auto &t = girlcat.object.tag;
int girlcat_x = (t.x0 + t.x1 + t.x2 + t.x3) / 4;
int go_towards_gay_furrys = (girlcat_x - 160) / 2;
furry_1.move(std::clamp(60 + go_towards_gay_furrys, -127, 127));
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
	catgirl_datecter.reset();
	 while (true) {
    int girlcat = master.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_X);
    int spiner_cat = master.get_analog(pros::E_CONTROLLER_ANALOG_RIGHT_Y);
	 furry_1.move(girlcat + spiner_cat);
	 furry_2.move(girlcat - spiner_cat); // mrreoawwww :pleading: mrroewawww mrroewawwddd girlcat REIDING THIS IM A CATGIRL mrreowww 
      pros::delay(20);
	 }
}
	