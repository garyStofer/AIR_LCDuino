/* This file controls build time features */
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  BUILD OPTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// comment/uncomment for additional features 
// Note: Wind and RPM are mutually excluisive as they currently make use of the same IO pin and the Pin-Change interrupt. 

// #define WetBulbTemp
#define WITH_RPM 
//#define WITH_WIND 