/***********************************************************************
 * flaUtilities.h: Put commonly used utilities here (like timing, printf
 *   debugging, etc.
 *
 * Copyright 2015, Scientific Systems Company, Inc.  All Rights Reserved
 *
 * Author: Neil Johnson
 *
 * Date: 10/26/2015
 *
 **********************************************************************/

#ifndef FLAUTILITIES_H
#define FLAUTILITIES_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

//! Global constants:
#ifndef M_PI
#define M_PI                                  3.141592653589793
#endif
#define D2R                                 M_PI/180.0
#define R2D                                 180.0/M_PI
#define DEG2RAD                             M_PI/180.0
#define RAD2DEG                             180.0/M_PI

/**
 *  double ms_time(void): returns the current time as a double floating point
 *  time since an arbitrary initial time.  We use the monotonic timer which can't
 *  go backwards.
 */
static inline double ms_time(void)
{
    struct timespec now_timespec;
    clock_gettime(CLOCK_MONOTONIC, &now_timespec);
    return ((double)now_timespec.tv_sec)*1000.0 + ((double)now_timespec.tv_nsec)*1.0e-6;
}

/**
 *  float ms_time(void): returns the current time as a double floating point
 *  time since an arbitrary initial time.  We use the monotonic timer which can't
 *  go backwards.
 */
static inline float ms_timef(void)
{
    struct timespec now_timespec;
    clock_gettime(CLOCK_MONOTONIC, &now_timespec);
    return ((float)now_timespec.tv_sec)*1000.0f + ((float)now_timespec.tv_nsec)*1.0e-6f;
}

float saturate_float(float in, float up_limit, float down_limit);
int32_t saturate_int(int32_t in, int32_t up_limit, int32_t down_limit);

bool dirExists(const char *path);
bool isSymlink(const char * path);

static int makeFolder(const char * path)
{
    char cmd[1024];
    snprintf(cmd, 1024, "mkdir -p %s", path);
    return system(cmd);
}

const std::string currentDateTime();

float unwrap_angle_radians(float in);

//int euler2quat(float i_roll, float i_pitch, float i_yaw, float* o_q0, float* o_q1, float* o_q2, float* o_q3);
int quat2euler(float i_q0, float i_q1, float i_q2, float i_q3, float* o_roll, float* o_pitch, float* o_yaw);

//! Checks whether a path (like in the sysfs) equals an expected value.  Returns a bool
bool checkPathExpectedValue(std::string path, std::string expect);


#endif // FLA_UTILITIES_H
