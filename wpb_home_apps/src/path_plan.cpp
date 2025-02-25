/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2017-2020, Waterplus http://www.6-robot.com
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the WaterPlus nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  FOOTPRINTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/
/* @author Zhang Wanjie                                             */
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <vector>
#include "action_manager.h"
#include <sound_play/SoundRequest.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <waterplus_map_tools/GetWaypointByName.h>

using namespace std;

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
static ros::Publisher spk_pub;
static CActionManager action_manager;
static ros::ServiceClient cliGetWPName;
static waterplus_map_tools::GetWaypointByName srvName;

//鏈夐檺鐘舵€佹満
#define STATE_READY       0
#define STATE_WAIT_ENTR   1
#define STATE_GOTO        2
#define STATE_DONE        3

static int nState = STATE_WAIT_ENTR;  //绋嬪簭鍚姩鏃跺垵濮嬬姸鎬?

// 鍒濆鍖栬埅鐐归亶鍘嗚剼鏈?
static vector<string> arWaypoint;
static int nWaypointIndex = 0;
static void Init_waypoints()
{
    arWaypoint.push_back("1");
    arWaypoint.push_back("2");
    arWaypoint.push_back("3");
    arWaypoint.push_back("4");
    arWaypoint.push_back("5");
}

static void Speak(string inStr)
{
    sound_play::SoundRequest sp;
    sp.sound = sound_play::SoundRequest::SAY;
    sp.command = sound_play::SoundRequest::PLAY_ONCE;
    sp.arg = inStr;
    spk_pub.publish(sp);
}

static int nOpenCount = 0;
void EntranceCB(const std_msgs::String::ConstPtr & msg)
{
    //ROS_WARN("[EntranceCB] - %s",msg->data.c_str());
    string strDoor = msg->data;
    if(strDoor == "door open")
    {
        nOpenCount ++;
    }
    else
    {
        nOpenCount = 0;
    }
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "wpb_home_path_plan");
    Init_waypoints();
    action_manager.Init();

    ros::NodeHandle n;
    ros::Subscriber sub_ent = n.subscribe("/wpb_home/entrance_detect", 10, EntranceCB);
    cliGetWPName = n.serviceClient<waterplus_map_tools::GetWaypointByName>("/waterplus/get_waypoint_name");
    spk_pub = n.advertise<sound_play::SoundRequest>("/robotsound", 20);

    string strGoto;
    ROS_INFO("[main] wpb_home_path_plan");
    ros::Rate r(10);
    while(ros::ok())
    {
        if(nState == STATE_WAIT_ENTR)
        {
            //绛夊緟寮€闂?涓€鏃︽娴嬪埌寮€闂?渚垮幓寰€鍙戜护鍦扮偣
            if(nOpenCount > 20) 
            {
                strGoto = "start";     //start鏄満鍦板唴鐨勮捣鐐?璇峰湪鍦板浘閲岃缃繖涓埅鐐?
                srvName.request.name = strGoto;
                if (cliGetWPName.call(srvName))
                {
                    std::string name = srvName.response.name;
                    float x = srvName.response.pose.position.x;
                    float y = srvName.response.pose.position.y;
                    ROS_INFO("Get_wp_name: name = %s (%.2f,%.2f)", strGoto.c_str(),x,y);

                    MoveBaseClient ac("move_base", true);
                    if(!ac.waitForServer(ros::Duration(5.0)))
                    {
                        ROS_INFO("The move_base action server is no running. action abort...");
                    }
                    else
                    {
                        move_base_msgs::MoveBaseGoal goal;
                        goal.target_pose.header.frame_id = "map";
                        goal.target_pose.header.stamp = ros::Time::now();
                        goal.target_pose.pose = srvName.response.pose;
                        ac.sendGoal(goal);
                        ac.waitForResult();
                        if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
                        {
                            ROS_INFO("Arrived at %s!",strGoto.c_str());
                            //鍒拌揪"start"鑸偣,寮€濮嬫墽琛岃埅鐐归亶鍘嗚剼鏈?
                            Speak("I am ready.");
                            ros::spinOnce();
                            sleep(3);

                            nState = STATE_GOTO; 
                            strGoto = arWaypoint[nWaypointIndex];
                            nWaypointIndex ++;
                        }
                        else
                            ROS_INFO("Failed to get to %s ...",strGoto.c_str() );
                    }
                    
                }
                else
                {
                    ROS_ERROR("Failed to call service GetWaypointByName");
                }
            }
        }
        if(nState == STATE_GOTO)
        {
            // 姝ｅ紡鐨勮埅鐐归亶鍘嗚剼鏈?
            srvName.request.name = strGoto;
            if (cliGetWPName.call(srvName))
            {
                std::string name = srvName.response.name;
                float x = srvName.response.pose.position.x;
                float y = srvName.response.pose.position.y;
                ROS_INFO("Get_wp_name: name = %s (%.2f,%.2f)", strGoto.c_str(),x,y);

                MoveBaseClient ac("move_base", true);
                if(!ac.waitForServer(ros::Duration(5.0)))
                {
                    ROS_INFO("The move_base action server is no running. action abort...");
                }
                else
                {
                    move_base_msgs::MoveBaseGoal goal;
                    goal.target_pose.header.frame_id = "map";
                    goal.target_pose.header.stamp = ros::Time::now();
                    goal.target_pose.pose = srvName.response.pose;
                    ac.sendGoal(goal);
                    ac.waitForResult();
                    if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
                    {
                        ROS_INFO("Arrived at %s!",strGoto.c_str());
                        //鍒拌揪"start"鑸偣,寮€濮嬫墽琛岃埅鐐归亶鍘嗚剼鏈?
                        string strSpeak = "I have got to waypoint " + strGoto;
                        Speak(strSpeak);
                        ros::spinOnce();
                        sleep(3);

                        int nNumWayponts = arWaypoint.size();
                        if(nWaypointIndex < nNumWayponts)
                        {
                            // 鑸偣鏈亶鍘嗗畬锛岀户缁笅涓€涓埅鐐?
                            strGoto = arWaypoint[nWaypointIndex];
                            nWaypointIndex ++;
                            nState = STATE_GOTO; 
                        }
                        else
                        {
                            // 鑸偣閬嶅巻瀹屾垚锛岀粨鏉?
                            Speak("I am done.");
                            ros::spinOnce();
                            nState = STATE_DONE;
                        }
                    }
                    else
                        ROS_INFO("Failed to get to %s ...",strGoto.c_str() );
                }
                
            }
            else
            {
                ROS_ERROR("Failed to call service GetWaypointByName");
            }
        }
        if(nState == STATE_GOTO)
        {

        }
        ros::spinOnce();
        r.sleep();
    }

    return 0;
}
