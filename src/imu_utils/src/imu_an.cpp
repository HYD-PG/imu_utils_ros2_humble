//#define BACKWARD_HAS_DW 1
//#include "backward.hpp"
// namespace backward
//{
// backward::SignalHandling sh;
//}

#include "acc_lib/allan_acc.h"
#include "acc_lib/fitallan_acc.h"
#include "gyr_lib/allan_gyr.h"
#include "gyr_lib/fitallan_gyr.h"
#include <code_utils/ros_utils.h>
#include <geometry_msgs/msg/vector3_stamped.hpp>
#include <iostream>
#include <fstream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <queue>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>

std::mutex m_buf;

std::queue< sensor_msgs::msg::Imu::SharedPtr>imu_buf;
imu::AllanGyr* gyr_x;
imu::AllanGyr* gyr_y;
imu::AllanGyr* gyr_z;
imu::AllanAcc* acc_x;
imu::AllanAcc* acc_y;
imu::AllanAcc* acc_z;
double start_t;
bool start       = true;
bool end         = false;
int max_time_min = 10;
std::string data_save_path;

void
imu_callback( const sensor_msgs::msg::Imu::SharedPtr imu_msg )
{
    //    m_buf.lock( );
    //    imu_buf.push( imu_msg );
    //    m_buf.unlock( );
    double time = imu_msg->header.stamp.sec + imu_msg->header.stamp.nanosec * 1e-9;
    gyr_x->pushRadPerSec( imu_msg->angular_velocity.x, time ); // 数据是变成 度/小时
    gyr_y->pushRadPerSec( imu_msg->angular_velocity.y, time );
    gyr_z->pushRadPerSec( imu_msg->angular_velocity.z, time );
    acc_x->pushMPerSec2( imu_msg->linear_acceleration.x, time ); // 数据是变成 米/秒^2
    acc_y->pushMPerSec2( imu_msg->linear_acceleration.y, time );
    acc_z->pushMPerSec2( imu_msg->linear_acceleration.z, time );

    if ( start )
    {
        start_t = time;
        start   = false;
    }
    else
    {
        double time_min = ( time - start_t ) / 60;
        if ( time_min > max_time_min )
            end = true;
    }
}

void
writeData1( const std::string sensor_name, //
            const std::vector< double >& gyro_ts_x,
            const std::vector< double >& gyro_d )
{
    std::ofstream out_t;
    std::ofstream out_x;
    out_t.open( data_save_path + "data_" + sensor_name + "_t.txt", std::ios::trunc );
    out_x.open( data_save_path + "data_" + sensor_name + "_x.txt", std::ios::trunc );
    out_t << std::setprecision( 10 );
    out_x << std::setprecision( 10 );
    for ( unsigned int index = 0; index < gyro_ts_x.size( ); ++index )
    {
        out_t << gyro_ts_x[index] << '\n';
        out_x << gyro_d[index] << '\n';
    }
    out_t.close( );
    out_x.close( );
}

void
writeData3( const std::string sensor_name,
            const std::vector< double >& gyro_ts_x,
            const std::vector< double >& gyro_d_x,
            const std::vector< double >& gyro_d_y,
            const std::vector< double >& gyro_d_z )
{
    std::ofstream out_t;
    std::ofstream out_x;
    std::ofstream out_y;
    std::ofstream out_z;
    out_t.open( data_save_path + "data_" + sensor_name + "_t.txt", std::ios::trunc );
    out_x.open( data_save_path + "data_" + sensor_name + "_x.txt", std::ios::trunc );
    out_y.open( data_save_path + "data_" + sensor_name + "_y.txt", std::ios::trunc );
    out_z.open( data_save_path + "data_" + sensor_name + "_z.txt", std::ios::trunc );
    out_t << std::setprecision( 10 );
    out_x << std::setprecision( 10 );
    out_y << std::setprecision( 10 );
    out_z << std::setprecision( 10 );

    for ( int index = 0; index < gyro_ts_x.size( ); ++index )
    {
        out_t << gyro_ts_x[index] << '\n';
        out_x << gyro_d_x[index] << '\n';
        out_y << gyro_d_y[index] << '\n';
        out_z << gyro_d_z[index] << '\n';
    }

    out_t.close( );
    out_x.close( );
    out_y.close( );
    out_z.close( );
}

void
writeYAML( const std::string data_path,
           const std::string sensor_name,
           const imu::FitAllanGyr& gyr_x,
           const imu::FitAllanGyr& gyr_y,
           const imu::FitAllanGyr& gyr_z,
           const imu::FitAllanAcc& acc_x,
           const imu::FitAllanAcc& acc_y,
           const imu::FitAllanAcc& acc_z )
{
    cv::FileStorage fs( data_path + sensor_name + "_imu_param.yaml", cv::FileStorage::WRITE );

    fs << "type"
       << "IMU";

    fs << "name" << sensor_name;

    fs << "Gyr";
    fs << "{";
    fs << "unit"
       << " rad/s";

    fs << "avg-axis";
    fs << "{";
    fs << std::string( "gyr_n" )
       << ( gyr_x.getWhiteNoise( ) + gyr_y.getWhiteNoise( ) + gyr_z.getWhiteNoise( ) ) / 3;
    fs << std::string( "gyr_b" )
       << ( gyr_x.getBiasInstability( ) + gyr_y.getBiasInstability( ) + gyr_z.getBiasInstability( ) ) / 3;
    fs << std::string("gyr_bw")
       << ( gyr_x.getK() + gyr_y.getK() + gyr_z.getK()) / (3 * (57.3 * 60 * 3600));
    fs << "}";

    fs << "x-axis";
    fs << "{";
    fs << std::string( "gyr_n" ) << gyr_x.getWhiteNoise( );
    fs << std::string( "gyr_b" ) << gyr_x.getBiasInstability( );
    fs << std::string( "gyr_bw" ) << gyr_x.getK() / (57.3 * 60 * 3600);
    fs << "}";

    fs << "y-axis";
    fs << "{";
    fs << std::string( "gyr_n" ) << gyr_y.getWhiteNoise( );
    fs << std::string( "gyr_b" ) << gyr_y.getBiasInstability( );
    fs << std::string( "gyr_bw" ) << gyr_y.getK() / (57.3 * 60 * 3600);

    fs << "}";

    fs << "z-axis";
    fs << "{";
    fs << std::string( "gyr_n" ) << gyr_z.getWhiteNoise( );
    fs << std::string( "gyr_b" ) << gyr_z.getBiasInstability( );
    fs << std::string( "gyr_bw" ) << gyr_z.getK() / (57.3 * 60 * 3600);

    fs << "}";

    fs << "}";

    fs << "Acc";
    fs << "{";
    fs << "unit"
       << " m/s^2";

    fs << "avg-axis";
    fs << "{";
    fs << std::string( "acc_n" )
       << ( acc_x.getWhiteNoise( ) + acc_y.getWhiteNoise( ) + acc_z.getWhiteNoise( ) ) / 3;
    fs << std::string( "acc_b" )
       << ( acc_x.getBiasInstability( ) + acc_y.getBiasInstability( ) + acc_z.getBiasInstability( ) ) / 3;
    fs << "acc_bw" 
       << ( acc_x.getK() + acc_y.getK() + acc_z.getK()) / 3;
    fs << "}";

    fs << "x-axis";
    fs << "{";
    fs << std::string( "acc_n" ) << acc_x.getWhiteNoise( );
    fs << std::string( "acc_b" ) << acc_x.getBiasInstability( );
    fs << std::string( "acc_bw") << acc_x.getK();
    fs << "}";

    fs << "y-axis";
    fs << "{";
    fs << std::string( "acc_n" ) << acc_y.getWhiteNoise( );
    fs << std::string( "acc_b" ) << acc_y.getBiasInstability( );
    fs << std::string( "acc_bw") << acc_y.getK();

    fs << "}";

    fs << "z-axis";
    fs << "{";
    fs << std::string( "acc_n" ) << acc_z.getWhiteNoise( );
    fs << std::string( "acc_b" ) << acc_z.getBiasInstability( );
    fs << std::string( "acc_bw") << acc_z.getK();

    fs << "}";

    fs << "}";

    fs.release( );
}

int
main( int argc, char** argv )
{
    rclcpp::init( argc, argv);
    auto n = rclcpp::Node::make_shared("imu_an");
    n->get_logger().set_level( rclcpp::Logger::Level::Debug);

    std::string IMU_TOPIC;
    std::string IMU_NAME;
    int max_cluster;

    IMU_TOPIC      = ros_utils::readParam< std::string >( *n, "imu_topic" );
    IMU_NAME       = ros_utils::readParam< std::string >( *n, "imu_name" );
    data_save_path = ros_utils::readParam< std::string >( *n, "data_save_path" );
    max_time_min   = ros_utils::readParam< int >( *n, "max_time_min" );
    max_cluster    = ros_utils::readParam< int >( *n, "max_cluster" );


     auto sub_imu = n->create_subscription<sensor_msgs::msg::Imu>(IMU_TOPIC, 
            rclcpp::QoS(rclcpp::KeepLast(2000)), imu_callback);
    //    ros::Publisher pub = n.advertise< geometry_msgs::Vector3Stamped >( ALLAN_TOPIC,
    //    2000 );

    gyr_x = new imu::AllanGyr( "gyr x", max_cluster );
    gyr_y = new imu::AllanGyr( "gyr y", max_cluster );
    gyr_z = new imu::AllanGyr( "gyr z", max_cluster );
    acc_x = new imu::AllanAcc( "acc x", max_cluster );
    acc_y = new imu::AllanAcc( "acc y", max_cluster );
    acc_z = new imu::AllanAcc( "acc z", max_cluster );
    std::cout << "wait for imu data." << std::endl;
    rclcpp::Rate loop(100);

    std::cout<<end<<std::endl;
    while (rclcpp::ok() && !end)
    {   
        loop.sleep( );
        rclcpp::spin_some(n);
    }
    std::cout<<"rclcpp ok: "<<rclcpp::ok()<<std::endl;
    ///
    gyr_x->calc( );
    std::vector< double > gyro_v_x  = gyr_x->getVariance( );
    std::vector< double > gyro_d_x  = gyr_x->getDeviation( );
    std::vector< double > gyro_ts_x = gyr_x->getTimes( );

    gyr_y->calc( );
    std::vector< double > gyro_v_y  = gyr_y->getVariance( );
    std::vector< double > gyro_d_y  = gyr_y->getDeviation( );
    std::vector< double > gyro_ts_y = gyr_y->getTimes( );

    gyr_z->calc( );
    std::vector< double > gyro_v_z  = gyr_z->getVariance( );
    std::vector< double > gyro_d_z  = gyr_z->getDeviation( );
    std::vector< double > gyro_ts_z = gyr_z->getTimes( );

    std::cout << "Gyro X " << std::endl;
    imu::FitAllanGyr fit_gyr_x( gyro_v_x, gyro_ts_x, gyr_x->getFreq( ) );
    std::cout << "  bias " << gyr_x->getAvgValue( ) / 3600 << " degree/s" << std::endl;
    std::cout << "-------------------" << std::endl;

    std::cout << "Gyro y " << std::endl;
    imu::FitAllanGyr fit_gyr_y( gyro_v_y, gyro_ts_y, gyr_y->getFreq( ) );
    std::cout << "  bias " << gyr_y->getAvgValue( ) / 3600 << " degree/s" << std::endl;
    std::cout << "-------------------" << std::endl;

    std::cout << "Gyro z " << std::endl;
    imu::FitAllanGyr fit_gyr_z( gyro_v_z, gyro_ts_z, gyr_z->getFreq( ) );
    std::cout << "  bias " << gyr_z->getAvgValue( ) / 3600 << " degree/s" << std::endl;
    std::cout << "-------------------" << std::endl;

    std::vector< double > gyro_sim_d_x = fit_gyr_x.calcSimDeviation( gyro_ts_x );
    std::vector< double > gyro_sim_d_y = fit_gyr_y.calcSimDeviation( gyro_ts_y );
    std::vector< double > gyro_sim_d_z = fit_gyr_z.calcSimDeviation( gyro_ts_z );

    writeData3( IMU_NAME + "_sim_gyr", gyro_ts_x, gyro_sim_d_x, gyro_sim_d_y, gyro_sim_d_z );
    writeData3( IMU_NAME + "_gyr", gyro_ts_x, gyro_d_x, gyro_d_y, gyro_d_z );

    std::cout << "==============================================" << std::endl;
    std::cout << "==============================================" << std::endl;

    acc_x->calc( );
    std::vector< double > acc_v_x  = acc_x->getVariance( );
    std::vector< double > acc_d_x  = acc_x->getDeviation( );
    std::vector< double > acc_ts_x = acc_x->getTimes( );

    acc_y->calc( );
    std::vector< double > acc_v_y  = acc_y->getVariance( );
    std::vector< double > acc_d_y  = acc_y->getDeviation( );
    std::vector< double > acc_ts_y = acc_y->getTimes( );

    acc_z->calc( );
    std::vector< double > acc_v_z  = acc_z->getVariance( );
    std::vector< double > acc_d_z  = acc_z->getDeviation( );
    std::vector< double > acc_ts_z = acc_z->getTimes( );

    std::cout << "acc X " << std::endl;
    imu::FitAllanAcc fit_acc_x( acc_v_x, acc_ts_x, acc_x->getFreq( ) );
    std::cout << "-------------------" << std::endl;

    std::cout << "acc y " << std::endl;
    imu::FitAllanAcc fit_acc_y( acc_v_y, acc_ts_y, acc_y->getFreq( ) );
    std::cout << "-------------------" << std::endl;

    std::cout << "acc z " << std::endl;
    imu::FitAllanAcc fit_acc_z( acc_v_z, acc_ts_z, acc_z->getFreq( ) );
    std::cout << "-------------------" << std::endl;

    std::vector< double > acc_sim_d_x = fit_acc_x.calcSimDeviation( acc_ts_x );
    std::vector< double > acc_sim_d_y = fit_acc_y.calcSimDeviation( acc_ts_x );
    std::vector< double > acc_sim_d_z = fit_acc_z.calcSimDeviation( acc_ts_x );

    writeData3( IMU_NAME + "_sim_acc", acc_ts_x, acc_sim_d_x, acc_sim_d_y, acc_sim_d_z );
    writeData3( IMU_NAME + "_acc", acc_ts_x, acc_d_x, acc_d_y, acc_d_z );

    writeYAML( data_save_path, IMU_NAME, fit_gyr_x, fit_gyr_y, fit_gyr_z, fit_acc_x, fit_acc_y, fit_acc_z );

    return 0;
}
