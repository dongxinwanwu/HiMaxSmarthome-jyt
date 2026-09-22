//
// Created by wjian on 2022/10/14.
//

#ifndef SMARTHOME_SOURCE_INTERFACE_H_
#define SMARTHOME_SOURCE_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* device type for system info message */
typedef enum {
  DEV_TYPE_PANEL = 0x1,      /* switch panel */
  DEV_TYPE_SUPER_SWITCH,     /* screen switch */
  DEV_TYPE_SOCKET,           /* smart socket */
  DEV_TYPE_CURTAIN,          /* smart curtain */
  DEV_TYPE_DOOR_SENSOR,      /* door sensor */
  DEV_TYPE_GAS,              /* gas detector */
  DEV_TYPE_SMOKE,            /* smoke detector */
  DEV_TYPE_WATER,            /* water detector */
  DEV_TYPE_MOTION,           /* motion detector */
  DEV_TYPE_TEMP,             /* temperature sensor */
  DEV_TYPE_HUMIDITY,         /* humidity sensor */
  DEV_TYPE_LIGHT,            /* light sensor */
  DEV_TYPE_CO2,              /* co2 sensor */
  DEV_TYPE_PM25,             /* pm2.5 sensor */
  DEV_TYPE_PM10,             /* pm10 sensor */
  DEV_TYPE_TVOC,             /* tvoc sensor */
  DEV_TYPE_AIR_condition,    /* air condition */
  DEV_TYPE_VETILATION,       /* ventilation */
  DEV_TYPE_HEATER,           /* heater */
} DevType_t;

/* panel key type */
typedef enum {
  PANEL_1_KEY = 0x1,            /* 1-key panel */
  PANEL_2_KEY,                  /* 2-key panel */
  PANEL_3_KEY,                  /* 3-key panel */
  PANEL_4_KEY,                  /* 4-key panel */
  PANEL_5_KEY,                  /* 5-key panel */
  PANEL_6_KEY,                  /* 6-key panel */
} PanelType_t;

typedef enum {
  ACD_SWITCH = 0x1,             /* air condition switch */
  ACD_TEMP,                     /* air condition temperature */
  ACD_MODE,                     /* air condition mode */
  ACD_SPEED,                    /* air condition wind speed */
  ACD_WIND_DIR,                 /* air condition wind direction */
  ACD_MODE_OUTDOOR,             /* air condition mode outdoor */
  ACD_ENV_TEMP,                 /* air condition environment temperature */
  ACD_ERROR_CODE                /* air condition error code */
} AirConditionPoint_t;

typedef enum {
  HEAT_SWITCH = 0x1,            /* heater switch */
  HEAT_TEMP,                    /*  heater temperature */
  HEAT_ENV_TEMP,                /*  heater environment temperature */
  HEAT_VALVE_STATUS,  	        /* heater valve status */
  HEAT_ERROR_CODE               /* heater error code */
} HeaterPoint_t;

typedef enum {
  VENT_SWITCH = 0x1,
  VENT_SPEED,
  VENT_WIND_DIR,
  VENT_ERROR_CODE
} VentilationPoint_t;

typedef enum {
  CURTAIN_SWITCH = 0x1
} CurtainPoint_t;



typedef struct {
  uint16_t id;  /* device id */
  uint16_t pid; /* product id */
  uint8_t did;  /* data point id */

  uint8_t type; /* value的数据类型:枚举、布尔、整数、单浮点、指针 */
  uint32_t value;
  uint32_t oldVal;
  uint8_t property; /* 属性:只读、只写、读写…… */

  uint32_t timestamp;
} Point_t;

// TY_RECV_OBJ_DP_S
typedef struct {
  char *name;
  char *manufacturer;
  char *model;
  char *product;
  char *device;
  char *eib;
  char *uuid;
  char *key;
  char *device_name;
  Point_t *point;
} Device_t;

#endif // SMARTHOME_SOURCE_INTERFACE_H_
