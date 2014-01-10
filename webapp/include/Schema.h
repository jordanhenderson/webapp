/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */
 
#ifndef SCHEMA_H
#define SCHEMA_H

#define WEBAPP_PARAM_BASEPATH 0
#define WEBAPP_PARAM_DBPATH 1
#define WEBAPP_STATIC_STRINGS 3
#define WEBAPP_LEN_SESSIONID 1
#define WEBAPP_NUM_THREADS 8
#define INT_INTERVAL(i) sizeof(int)*i
#define WEBAPP_DEFAULT_QUEUESIZE 1023
#define WEBAPP_SCRIPTS 4
#define WEBAPP_PARAM_PORT 0
#define WEBAPP_PORT_DEFAULT 5000
//APP specific definitions
//PROTOCOL SCHEMA DEFINITIONS
#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define PROTOCOL_LENGTH_SIZEINFO sizeof(int) * PROTOCOL_VARS
#endif
