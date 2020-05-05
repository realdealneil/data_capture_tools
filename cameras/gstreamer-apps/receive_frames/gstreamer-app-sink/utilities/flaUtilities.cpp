/***********************************************************************
 * Utilities for FLA Application
 *
 * Copyright 2015, Scientific Systems Company, Inc.  All Rights Reserved
 *
 * Author: Neil Johnson
 *
 * Date: 10/26/2015
 *
 **********************************************************************/

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include "flaUtilities.h"

double g_start_time;

uint32_t verbosity = 1;

#if 0
/**
 *  double ms_time(void): returns the current time as a double floating point
 *  time since an arbitrary initial time.  We use the monotonic timer which can't
 *  go backwards.
 */
double ms_time(void)
{
    struct timespec now_timespec;
    clock_gettime(CLOCK_MONOTONIC, &now_timespec);
    return ((double)now_timespec.tv_sec)*1000.0 + ((double)now_timespec.tv_nsec)*1.0e-6;
}
#endif

bool g_usingDiagnosticsScreen=false;
bool printfdLoggingEnabled=false;
std::string printfdLogPath="";
FILE *printfdLogFile = stdout;

bool dirExists(const char *path)
{
    struct stat info;

    if(stat( path, &info ) != 0)
        return false;
    else if (info.st_mode & S_IFDIR)
        return true;
    else
        return false;
}

bool fileExists(const char *path)
{
    struct stat info;

    if(stat( path, &info ) != 0)
        return false;
    else if (info.st_mode & S_IFREG )
        return true;
    else
        return false;
}

bool pathExists(const char *path)
{
    struct stat info;

    if(stat( path, &info ) != 0)
        return false;
    else
        return true;
}

bool isSymlink(const char * path)
{
    struct stat info;
    if (lstat( path, &info) != 0)
    {
        printf("lstat failed on %s!\n", path);
        return false;
    }

    //printf("isSymlink:\n");
    switch(info.st_mode & S_IFMT) {
        case S_IFLNK:
            //printf("This is a link!\n");
            return true;
        case S_IFREG:
            //printf("This is NOT a link!\n");
            return false;
        default:
            //printf("I don't know what this is!\n");
            return false;
    }
}

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y_%m_%d_%H_%M_%S", &tstruct);

    return buf;
}

float unwrap_angle_radians(float in)
{
    float temp = in;
    while (temp > M_PI) {
        temp -= 2*M_PI;
    }
    while (temp < -M_PI) {
        temp += 2*M_PI;
    }
    return temp;
}

int quat2euler(float q0, float q1, float q2, float q3, float* o_roll, float* o_pitch, float* o_yaw)
{
    *o_roll = atan2( 2.0*(q0*q1+q2*q3), 1.0-2.0*(q1*q1 + q2*q2) );

    float temp = 2.0*(q0*q2 - q3*q1);
    if (temp > 1.0) {
        temp=1.0;
    } else if (temp < -1.0) {
        temp=-1.0;
    }

    *o_pitch = asin( temp );

    *o_yaw = atan2( 2.0*(q0*q3 + q1*q2), 1.0-2.0*(q2*q2 + q3*q3) );




    //takes quat(enu) to euler(enu)
    /*
    float temp = -2*(i_q0*i_q2-i_q3*i_q1);
    if( temp > 1.0)
    {
        temp = 1.0;
    }
    else if(temp < -1.0)
    {
        temp = -1.0;
    }
    *o_pitch = asin(temp);
    if(fabs(i_q0*i_q1 + i_q2*i_q3 - 0.5) < 1e-10)
    {
        *o_roll = 0;
        *o_yaw = 2*atan2(i_q0,i_q3);
    }
    else if(fabs(i_q0*i_q1 + i_q2*i_q3 + 0.5) < 1e-10)
    {
        *o_roll = 0;
        *o_yaw = -2*atan2(i_q0,i_q3);
    }
    else
    {
        *o_roll = atan2(2*(i_q3*i_q0+i_q1*i_q2),(i_q3*i_q3-i_q0*i_q0-i_q1*i_q1+i_q2*i_q2));
        *o_yaw = atan2(2*(i_q0*i_q1+i_q3*i_q2),(i_q3*i_q3+i_q0*i_q0-i_q1*i_q1-i_q2*i_q2));
    }

    if(*o_yaw > M_PI)
    {
        *o_yaw -= 2.0*M_PI;
    }
    if(*o_yaw < -M_PI)
    {
        *o_yaw += 2.0*M_PI;
    }*/

    return 0;
}

float saturate_float(float in, float up_limit, float down_limit)
{
    if (in > up_limit)
        return up_limit;
    else if (in < down_limit)
        return down_limit;
    else
        return in;
}

int32_t saturate_int(int32_t in, int32_t up_limit, int32_t down_limit)
{
    if (in > up_limit)
        return up_limit;
    else if (in < down_limit)
        return down_limit;
    else
        return in;
}

/*int euler2quat(float i_roll, float i_pitch, float i_yaw, float* o_q0, float* o_q1, float* o_q2, float* o_q3)
{
    float sphi = sin(0.5*i_roll);
    float stheta = sin(0.5*i_pitch);
    float spsi = sin(0.5*i_yaw);
    float cphi = cos(0.5*i_roll);
    float ctheta = cos(0.5*i_pitch);
    float cpsi = cos(0.5*i_yaw);

    *o_q0 = cpsi*ctheta*sphi - spsi*stheta*cphi;
    *o_q1 = cpsi*stheta*cphi + spsi*ctheta*sphi;
    *o_q2 = spsi*ctheta*cphi - cpsi*stheta*sphi;
    *o_q3 = cpsi*ctheta*cphi + spsi*stheta*sphi;

    return 0;
}*/

//! Checks whether a path (like in the sysfs) equals an expected value.  Returns a bool
bool checkPathExpectedValue(std::string path, std::string expect)
{
    std::stringstream ss;
    ss << "cat " << path;

    FILE* file = popen(ss.str().c_str(), "r");

    // use fscanf to read:
    char buffer[1024];
    int temp = fscanf(file, "%1024s", buffer);
    pclose(file);

    std::string output(buffer);

    if (output != expect) {
        std::cout << "Checked output of '" << ss.str() << "'" << std::endl;
        std::cout << "  Output: '" << output << "'" << std::endl;
        std::cout << "  Should be: '" << expect << "'" << std::endl;
        std::cout << "  Resolve this issue and then re-run the app.  " << std::endl;
        return false;
    }

    return true;
}
