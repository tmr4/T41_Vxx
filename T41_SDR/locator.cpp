// modified from: https://github.com/DD4WH/Pocket_FT8

#include <math.h>
#include <string.h>

#ifndef uint8_t
typedef __uint8_t uint8_t;
#endif

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//const double EARTH_RAD = 6371;  //radius in km
const double EARTH_RAD = 3958.8;  //radius in miles

float latitude, longitude;
float stationLatitude, stationLongitude;
float targetLatitude, targetLongitude;

#define PI        3.1415926535897932384626433832795f

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ProcessLocator(char locator[]) {
	uint8_t A1, A2, N1, N2;
	uint8_t A1_value, A2_value, N1_value, N2_value;
	float Latitude_1, Latitude_2, Latitude_3;
	float Longitude_1, Longitude_2, Longitude_3;

	A1 = locator[0];
	A2 = locator[1];
	N1 = locator[2];
	N2 = locator[3];

	A1_value = A1 - 65;
	A2_value = A2 - 65;
	N1_value = N1 - 48;
	N2_value = N2 - 48;

	Latitude_1 = (float)A2_value * 10;
	Latitude_2 = (float)N2_value;
	Latitude_3 = (11.0 / 24.0 + 1.0 / 48.0) - 90.0;
	latitude = Latitude_1 + Latitude_2 + Latitude_3;

	Longitude_1 = (float)A1_value * 20.0;
	Longitude_2 = (float)N1_value * 2.0;
	Longitude_3 = 11.0 / 12.0 +  1.0 / 24.0;
	longitude =  Longitude_1  +  Longitude_2 + Longitude_3 - 180.0;
}

void SetStationCoordinates(char station[]) {
	ProcessLocator(station);
	stationLatitude = latitude;
	stationLongitude = longitude;
}

// convert degrees to radians
double deg2rad(double deg)
{
  return deg * (PI / 180.0);
}

int ValidateLocator(char locator[]) {
  uint8_t A1, A2, N1, N2;
  uint8_t test = 0;

  A1 = locator[0] - 65;
  A2 = locator[1] - 65;
  N1 = locator[2] - 48;
  N2= locator [3] - 48;

  if(A1 >= 0 && A1 <= 17) test++;
  if(A2 > 0 && A2 < 17) test++; //block RR73 Artic and Anartica
  if(N1 >= 0 && N1 <= 9) test++;
  if(N2 >= 0 && N2 <= 9) test++;

  if(test == 4) {
    return 1;
  }
  else {
    return 0;
  }
}

// distance (miles) on earth's surface from point 1 to point 2
double Distance(double lat1, double lon1, double lat2, double lon2) {
  double lat1r = deg2rad(lat1);
  double lon1r = deg2rad(lon1);
  double lat2r = deg2rad(lat2);
  double lon2r = deg2rad(lon2);
  return acos(sin(lat1r) * sin(lat2r)+cos(lat1r) * cos(lat2r) * cos(lon2r-lon1r)) * EARTH_RAD;
}

float TargetDistance(char target[]) {
	float targetDistance;

	ProcessLocator(target);
	targetLatitude = latitude;
	targetLongitude = longitude;

	targetDistance = (float) Distance((double)stationLatitude,(double)stationLongitude,(double)targetLatitude,(double)targetLongitude);

	return targetDistance;
}

int CalcLocatorDistance(char *msg) {
  float distance = 0;
  //char Target_Locator[] = "    ";
  int locatorPos = strlen(msg) - 4;

  // copy
  //strcpy(Target_Locator, locator + strlen(locator) - 4);

  //if(ValidateLocator(Target_Locator) == 1) {
  if(ValidateLocator(&msg[locatorPos])  == 1) {
    distance = TargetDistance(&msg[locatorPos]);
  }

  return (int)distance;
}
