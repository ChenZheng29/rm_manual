//
// Created by peter on 2021/7/22.
//

#include "rm_manual/chassis_gimbal_shooter_manual.h"

namespace rm_manual
{
ChassisGimbalShooterManual::ChassisGimbalShooterManual(ros::NodeHandle& nh) : ChassisGimbalManual(nh)
{
  ros::NodeHandle shooter_nh(nh, "shooter");
  shooter_cmd_sender_ = new rm_common::ShooterCommandSender(shooter_nh, data_.referee_.referee_data_);
  ros::NodeHandle detection_switch_nh(nh, "detection_switch");
  switch_detection_srv_ = new rm_common::SwitchDetectionCaller(detection_switch_nh);
  XmlRpc::XmlRpcValue rpc_value;
  nh.getParam("shooter_calibration", rpc_value);
  shooter_calibration_ = new rm_common::CalibrationQueue(rpc_value, nh, controller_manager_);
  shooter_power_on_event_.setRising(boost::bind(&ChassisGimbalShooterManual::shooterOutputOn, this));
  self_inspection_event_.setRising(boost::bind(&ChassisGimbalShooterManual::selfInspectionStart, this));
  game_start_event_.setRising(boost::bind(&ChassisGimbalShooterManual::gameStart, this));
  left_switch_up_event_.setActiveHigh(boost::bind(&ChassisGimbalShooterManual::leftSwitchUpOn, this, _1));
  left_switch_mid_event_.setActiveHigh(boost::bind(&ChassisGimbalShooterManual::leftSwitchMidOn, this, _1));
  e_event_.setRising(boost::bind(&ChassisGimbalShooterManual::ePress, this));
  g_event_.setRising(boost::bind(&ChassisGimbalShooterManual::gPress, this));
  q_event_.setRising(boost::bind(&ChassisGimbalShooterManual::qPress, this));
  f_event_.setRising(boost::bind(&ChassisGimbalShooterManual::fPress, this));
  b_event_.setRising(boost::bind(&ChassisGimbalShooterManual::bPress, this));
  ctrl_c_event_.setRising(boost::bind(&ChassisGimbalShooterManual::ctrlCPress, this));
  ctrl_v_event_.setRising(boost::bind(&ChassisGimbalShooterManual::ctrlVPress, this));
  ctrl_r_event_.setRising(boost::bind(&ChassisGimbalShooterManual::ctrlRPress, this));
  ctrl_b_event_.setRising(boost::bind(&ChassisGimbalShooterManual::ctrlBPress, this));
  shift_event_.setEdge(boost::bind(&ChassisGimbalShooterManual::shiftPress, this),
                       boost::bind(&ChassisGimbalShooterManual::shiftRelease, this));
  mouse_left_event_.setActiveHigh(boost::bind(&ChassisGimbalShooterManual::mouseLeftPress, this));
  mouse_left_event_.setFalling(boost::bind(&ChassisGimbalShooterManual::mouseLeftRelease, this));
  mouse_right_event_.setActiveHigh(boost::bind(&ChassisGimbalShooterManual::mouseRightPress, this));
  mouse_right_event_.setFalling(boost::bind(&ChassisGimbalShooterManual::mouseRightRelease, this));
}

void ChassisGimbalShooterManual::run()
{
  ChassisGimbalManual::run();
  switch_detection_srv_->setEnemyColor(data_.referee_.referee_data_);
  shooter_calibration_->update(ros::Time::now());
}

void ChassisGimbalShooterManual::checkReferee()
{
  ChassisGimbalManual::checkReferee();
  shooter_power_on_event_.update(data_.referee_.referee_data_.game_robot_status_.mains_power_shooter_output_);
  self_inspection_event_.update(data_.referee_.referee_data_.game_status_.game_progress_ == 2);
  game_start_event_.update(data_.referee_.referee_data_.game_status_.game_progress_ == 4);
}

void ChassisGimbalShooterManual::checkKeyboard()
{
  ChassisGimbalManual::checkKeyboard();
  e_event_.update(data_.dbus_data_.key_e);
  g_event_.update(data_.dbus_data_.key_g);
  q_event_.update((!data_.dbus_data_.key_ctrl) & data_.dbus_data_.key_q);
  f_event_.update(data_.dbus_data_.key_f);
  b_event_.update((!data_.dbus_data_.key_ctrl) & data_.dbus_data_.key_b);
  ctrl_c_event_.update(data_.dbus_data_.key_ctrl & data_.dbus_data_.key_c);
  ctrl_v_event_.update(data_.dbus_data_.key_ctrl & data_.dbus_data_.key_v);
  ctrl_r_event_.update(data_.dbus_data_.key_ctrl & data_.dbus_data_.key_r);
  ctrl_b_event_.update(data_.dbus_data_.key_ctrl & data_.dbus_data_.key_b);
  shift_event_.update(data_.dbus_data_.key_shift);
  mouse_left_event_.update(data_.dbus_data_.p_l);
  mouse_right_event_.update(data_.dbus_data_.p_r);
}

void ChassisGimbalShooterManual::sendCommand(const ros::Time& time)
{
  ChassisGimbalManual::sendCommand(time);
  shooter_cmd_sender_->sendCommand(time);
}

void ChassisGimbalShooterManual::remoteControlTurnOff()
{
  ChassisGimbalManual::remoteControlTurnOff();
  shooter_cmd_sender_->setZero();
  shooter_calibration_->stop();
}

void ChassisGimbalShooterManual::remoteControlTurnOn()
{
  ChassisGimbalManual::remoteControlTurnOn();
  shooter_calibration_->stopController();
}

void ChassisGimbalShooterManual::robotDie()
{
  ManualBase::robotDie();
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::STOP);
}

void ChassisGimbalShooterManual::chassisOutputOn()
{
  ChassisGimbalManual::chassisOutputOn();
  chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::CHARGE);
}

void ChassisGimbalShooterManual::shooterOutputOn()
{
  ChassisGimbalManual::shooterOutputOn();
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::STOP);
  shooter_calibration_->reset();
}

void ChassisGimbalShooterManual::drawUi(const ros::Time& time)
{
  ChassisGimbalManual::drawUi(time);
  if (data_.referee_.referee_data_.robot_id_ != rm_common::RobotId::BLUE_HERO &&
      data_.referee_.referee_data_.robot_id_ != rm_common::RobotId::RED_HERO)
    trigger_change_ui_->update("target", switch_detection_srv_->getTarget(), shooter_cmd_sender_->getBurstMode(),
                               switch_detection_srv_->getArmorTarget(),
                               switch_detection_srv_->getColor() == rm_msgs::StatusChangeRequest::RED);
  else
    trigger_change_ui_->update("target", gimbal_cmd_sender_->getEject() ? 1 : 0, shooter_cmd_sender_->getBurstMode(),
                               switch_detection_srv_->getArmorTarget(),
                               switch_detection_srv_->getColor() == rm_msgs::StatusChangeRequest::RED);
  trigger_change_ui_->update("exposure", switch_detection_srv_->getExposureLevel(), false);
  fixed_ui_->update();
}

void ChassisGimbalShooterManual::updateRc()
{
  ChassisGimbalManual::updateRc();
  if (shooter_cmd_sender_->getMsg()->mode != rm_msgs::ShootCmd::STOP)
    gimbal_cmd_sender_->setBulletSpeed(shooter_cmd_sender_->getSpeed());
}

void ChassisGimbalShooterManual::updatePc()
{
  ChassisGimbalManual::updatePc();
  if (chassis_cmd_sender_->power_limit_->getState() != rm_common::PowerLimit::CHARGE)
  {
    if (!data_.dbus_data_.key_shift && chassis_cmd_sender_->getMsg()->mode == rm_msgs::ChassisCmd::FOLLOW &&
        std::sqrt(std::pow(vel_cmd_sender_->getMsg()->linear.x, 2) + std::pow(vel_cmd_sender_->getMsg()->linear.y, 2)) >
            0.0)
      chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
    else if (data_.referee_.referee_data_.capacity_data.chassis_power_ < 1.0)
      chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::BURST);
  }
}

void ChassisGimbalShooterManual::rightSwitchDownRise()
{
  ChassisGimbalManual::rightSwitchDownRise();
  chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::CHARGE);
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::STOP);
}

void ChassisGimbalShooterManual::rightSwitchMidRise()
{
  ChassisGimbalManual::rightSwitchMidRise();
  chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::CHARGE);
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::STOP);
}

void ChassisGimbalShooterManual::rightSwitchUpRise()
{
  ChassisGimbalManual::rightSwitchUpRise();
  chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::CHARGE);
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::STOP);
}

void ChassisGimbalShooterManual::leftSwitchDownRise()
{
  ChassisGimbalManual::leftSwitchDownRise();
  gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::RATE);
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::STOP);
}

void ChassisGimbalShooterManual::leftSwitchMidRise()
{
  ChassisGimbalManual::leftSwitchMidRise();
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::READY);
}

void ChassisGimbalShooterManual::leftSwitchMidOn(ros::Duration duration)
{
  if (data_.track_data_.id == 0)
    gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::RATE);
  else
    gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::TRACK);
}

void ChassisGimbalShooterManual::leftSwitchUpRise()
{
  ChassisGimbalManual::leftSwitchUpRise();
  gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::TRACK);
}

void ChassisGimbalShooterManual::leftSwitchUpOn(ros::Duration duration)
{
  if (data_.track_data_.id == 0)
    gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::RATE);
  else
    gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::TRACK);
  if (duration > ros::Duration(1.))
  {
    shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::PUSH);
    shooter_cmd_sender_->checkError(data_.gimbal_des_error_, ros::Time::now());
  }
  else if (duration < ros::Duration(0.02))
    shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::PUSH);
  else
    shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::READY);
}

void ChassisGimbalShooterManual::mouseLeftPress()
{
  shooter_cmd_sender_->setMode(rm_msgs::ShootCmd::PUSH);
  if (data_.dbus_data_.p_r)
    shooter_cmd_sender_->checkError(data_.gimbal_des_error_, ros::Time::now());
}

void ChassisGimbalShooterManual::mouseRightPress()
{
  if (data_.track_data_.id == 0)
    gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::RATE);
  else
  {
    gimbal_cmd_sender_->setMode(rm_msgs::GimbalCmd::TRACK);
    gimbal_cmd_sender_->setBulletSpeed(shooter_cmd_sender_->getSpeed());
  }
}

void ChassisGimbalShooterManual::gPress()
{
  if (chassis_cmd_sender_->getMsg()->mode == rm_msgs::ChassisCmd::GYRO)
  {
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
    vel_cmd_sender_->setAngularZVel(0.0);
    if (!data_.dbus_data_.key_shift)
      chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
  }
  else
  {
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::GYRO);
    vel_cmd_sender_->setAngularZVel(1.0);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::BURST);
  }
}

void ChassisGimbalShooterManual::ePress()
{
  if (chassis_cmd_sender_->getMsg()->mode == rm_msgs::ChassisCmd::TWIST)
  {
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
  }
  else
  {
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::TWIST);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
  }
}

void ChassisGimbalShooterManual::bPress()
{
  chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::CHARGE);
}

void ChassisGimbalShooterManual::wPress()
{
  ChassisGimbalManual::wPress();
  if ((data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::BLUE_HERO ||
       data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::RED_HERO) &&
      gimbal_cmd_sender_->getEject())
  {
    gimbal_cmd_sender_->setEject(false);
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
  }
}

void ChassisGimbalShooterManual::aPress()
{
  ChassisGimbalManual::aPress();
  if ((data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::BLUE_HERO ||
       data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::RED_HERO) &&
      gimbal_cmd_sender_->getEject())
  {
    gimbal_cmd_sender_->setEject(false);
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
  }
}

void ChassisGimbalShooterManual::sPress()
{
  ChassisGimbalManual::sPress();
  if ((data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::BLUE_HERO ||
       data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::RED_HERO) &&
      gimbal_cmd_sender_->getEject())
  {
    gimbal_cmd_sender_->setEject(false);
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
  }
}

void ChassisGimbalShooterManual::dPress()
{
  ChassisGimbalManual::dPress();
  if ((data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::BLUE_HERO ||
       data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::RED_HERO) &&
      gimbal_cmd_sender_->getEject())
  {
    gimbal_cmd_sender_->setEject(false);
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
  }
}

void ChassisGimbalShooterManual::shiftPress()
{
  if (chassis_cmd_sender_->getMsg()->mode != rm_msgs::ChassisCmd::FOLLOW)
  {
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
    vel_cmd_sender_->setAngularZVel(0.);
  }
  chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::BURST);
}

void ChassisGimbalShooterManual::shiftRelease()
{
  if (chassis_cmd_sender_->getMsg()->mode != rm_msgs::ChassisCmd::GYRO)
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::NORMAL);
}

void ChassisGimbalShooterManual::ctrlCPress()
{
  switch_detection_srv_->switchArmorTargetType();
  switch_detection_srv_->callService();
}

void ChassisGimbalShooterManual::ctrlVPress()
{
  switch_detection_srv_->switchEnemyColor();
  switch_detection_srv_->callService();
}

void ChassisGimbalShooterManual::ctrlRPress()
{
  if (data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::BLUE_HERO ||
      data_.referee_.referee_data_.robot_id_ == rm_common::RobotId::RED_HERO)
  {
    chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::GYRO);
    chassis_cmd_sender_->power_limit_->updateState(rm_common::PowerLimit::BURST);
    gimbal_cmd_sender_->setEject(true);
  }
  else
  {
    switch_detection_srv_->switchTargetType();
    switch_detection_srv_->callService();
    if (switch_detection_srv_->getTarget())
      chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::GYRO);
    else
      chassis_cmd_sender_->setMode(rm_msgs::ChassisCmd::FOLLOW);
  }
}

void ChassisGimbalShooterManual::ctrlBPress()
{
  switch_detection_srv_->switchExposureLevel();
  switch_detection_srv_->callService();
}

}  // namespace rm_manual
