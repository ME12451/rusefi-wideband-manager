#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>
#include <sys/mman.h>
#include <iostream>
#include "service_protocol.h"
#include "wideband_bitrate.h"
#include "wideband_calibration.h"
using systime_t=uint32_t;
struct CANRxFrame{uint32_t IDE=1,RTR=0,EID=0,DLC=8;uint8_t data8[8]={};};
using CANTxFrame=CANRxFrame;
#define CAN_IDE_EXT 1
#define CAN_RTR_DATA 0
#define CAN_ANY_MAILBOX 0
#define TIME_S2I(n) ((n)*1000u)
#define TIME_MS2I(n) (n)
#define MSG_OK 0
static int CAND1;
static uint32_t testTime;
static int resets,flashWrites,transmits,txResult;
static bool failFlash;
static CANTxFrame lastTx;
static systime_t chVTGetSystemTimeX(){return testTime;}
static systime_t chVTTimeElapsedSinceX(systime_t start){return testTime-start;}
static void chThdSleepMilliseconds(int n){testTime+=n;}
static void NVIC_SystemReset(){resets++;}
static int canTransmitTimeout(int*,int,CANTxFrame* f,int){lastTx=*f;transmits++;return txResult;}
static unsigned testHw=4;
static size_t BoardGetHwId(){return testHw;}
using SensorType=wbo::SensorType;
#define BOARD_DEFAULT_SENSOR_TYPE SensorType::LSU49
struct Configuration{
 wbo::CalibrationRecord calibration=wbo::MakeCalibrationRecord(0);
 SensorType sensorType=SensorType::LSU49;
 struct {uint8_t RusEfiIdx=0;bool RusEfiTx=true,RusEfiTxDiag=true;} afr[1];
 void LoadDefaults(){calibration=wbo::MakeCalibrationRecord(0);sensorType=SensorType::LSU49;afr[0].RusEfiIdx=0;afr[0].RusEfiTx=afr[0].RusEfiTxDiag=true;}
};
static Configuration config,savedConfig;
static wbo::CanBitrate savedRate=wbo::CanBitrate::Kbit500;
static float softwareCalibrationGain;
static void chSysLock(){}
static void chSysUnlock(){}
static wbo::CanBitrate ReadCanBitrate(){return savedRate;}
static bool WriteConfiguration(wbo::CanBitrate rate){flashWrites++;if(failFlash)return false;savedConfig=config;savedRate=rate;return true;}
[[maybe_unused]] static Configuration* GetConfiguration(){config=savedConfig;return &config;}
static bool WriteConfigurationUnlocked(wbo::CanBitrate rate){return WriteConfiguration(rate);}
static bool ConfigurationMatchesFlash(wbo::CanBitrate rate){return !failFlash&&rate==savedRate&&config.sensorType==savedConfig.sensorType&&config.afr[0].RusEfiIdx==savedConfig.afr[0].RusEfiIdx&&config.afr[0].RusEfiTx==savedConfig.afr[0].RusEfiTx&&config.afr[0].RusEfiTxDiag==savedConfig.afr[0].RusEfiTxDiag&&std::memcmp(&config.calibration,&savedConfig.calibration,sizeof(config.calibration))==0;}
static SensorType GetSensorType(){return config.sensorType;}
static uint16_t GetSoftwareCalibration(){return wbo::DecodeCalibration(config.calibration);}
static bool IsSensorCalibrationReady(){return wbo::HasRequiredCalibration(config.sensorType,GetSoftwareCalibration());}
static wbo::CalibrationResult SetSoftwareCalibration(uint16_t value,SensorType type){
 if(!wbo::IsCalibrationValueValid(value))return wbo::CalibrationResult::Invalid;
 if(type!=SensorType::LSU49&&type!=SensorType::LSU49_RTRIM_SW)return wbo::CalibrationResult::WrongSensor;
 if(failFlash)return wbo::CalibrationResult::FlashError;
 config.calibration=wbo::MakeCalibrationRecord(value);config.sensorType=type;savedConfig=config;return wbo::CalibrationResult::Saved;
}
struct TestSampler{float ac=.12f,ma=-2.5f;float GetNernstAc()const{return ac;}float GetPumpNominalCurrent()const{return ma;}};
static TestSampler sampler;
static const TestSampler& GetSampler(int){return sampler;}
static unsigned GetHeaterState(int){return 1;}
struct TestHeater{float GetHeaterEffectiveVoltage()const{return 7.5f;}};
static const TestHeater& GetHeaterController(int){static TestHeater h;return h;}
static float GetPumpCurrent(int){return 1.25f;}
static float GetRemoteBatteryVoltage(){return 12.f;}
