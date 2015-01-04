#include <jsonlite.h>
#include "M2XStreamClient.h"

#include "mbed.h"
#include "GPS.h"    //GPS

//------------------------------------------------------------------------------------
// You need to configure these cellular modem / SIM parameters.
// These parameters are ignored for LISA-C200 variants and can be left NULL.
//------------------------------------------------------------------------------------
#include "MDM.h"
//! Set your secret SIM pin here (e.g. "1234"). Check your SIM manual.
#define SIMPIN      NULL
/*! The APN of your network operator SIM, sometimes it is "internet" check your 
    contract with the network operator. You can also try to look-up your settings in 
    google: https://www.google.de/search?q=APN+list */
#define APN         NULL
//! Set the user name for your APN, or NULL if not needed
#define USERNAME    NULL
//! Set the password for your APN, or NULL if not needed
#define PASSWORD    NULL 
//------------------------------------------------------------------------------------

char feedId[] = "a7865631942a5c3deb3c0faa35bf15ed"; // Feed you want to post to --DEvice Id
char m2xKey[] = "5227426c8da68752fd0b0a4e0a40405c"; // Your M2X access key -- API KEY

char name[] = "<location name>"; // Name of current location of datasource
double latitude = 33.007872;
double longitude = -96.751614; // You can also read those values from a GPS
double elevation = 697.00;
bool location_valid = false;

Client client;
M2XStreamClient m2xClient(&client, m2xKey);

void on_data_point_found(const char* at, const char* value, int index, void* context, int type) {
  printf("Found a data point, index: %d\r\n", index);
  printf("At: %s Value: %s\r\n", at, value);
}

void on_location_found(const char* name,
                       double latitude,
                       double longitude,
                       double elevation,
                       const char* timestamp,
                       int index,
                       void* context) {
  printf("Found a location, index: %d\r\n", index);
  printf("Name: %s  Latitude: %lf  Longitude: %lf\r\n", name, latitude, longitude);
  printf("Elevation: %lf  Timestamp: %s\r\n", elevation, timestamp);
}

int main() {
//#if 0 // defined(TARGET_FF_MORPHO)
  MDMSerial mdm(D8,D2); // use the serrial port on D2 / D8
//#else
//  MDMSerial mdm;
//#endif  
  GPSI2C gps;
  //mdm.setDebug(4); // enable this for debugging issues 
  if (!mdm.connect(SIMPIN, APN,USERNAME,PASSWORD))
    return -1;
 
  char buf[256];
  
  Timer tmr;
  tmr.reset();
  tmr.start();
  while (true) {
    int ret;
    // extract the location information from the GPS NMEA data
    while ((ret = gps.getMessage(buf, sizeof(buf))) > 0) {
        int len = LENGTH(ret);
        if ((PROTOCOL(ret) == GPSParser::NMEA) && (len > 6)) {
            // talker is $GA=Galileo $GB=Beidou $GL=Glonass $GN=Combined $GP=GPS
            if ((buf[0] == '$') || buf[1] == 'G') {
                #define _CHECK_TALKER(s) ((buf[3] == s[0]) && (buf[4] == s[1]) && (buf[5] == s[2]))
                if (_CHECK_TALKER("GGA")) {
                    char ch;
                    if (gps.getNmeaAngle(2,buf,len,latitude) && 
                        gps.getNmeaAngle(4,buf,len,longitude) && 
                        gps.getNmeaItem(6,buf,len,ch) &&
                        gps.getNmeaItem(9,buf,len,elevation)) {
                       printf("GPS Location: %.5f %.5f %.1f %c\r\n", latitude, longitude, elevation, ch); 
                       location_valid = ch == '1' || ch == '2' || ch == '6';
                    }
                }
            }
        }
    }
    
    if (tmr.read_ms() > 1000) {
        tmr.reset();
        tmr.start();
        int response;
        
        MDMParser::NetStatus status;
        if (mdm.checkNetStatus(&status)) {
            sprintf(buf, "%d", status.rssi);
            response = m2xClient.updateStreamValue(feedId, "rssi", buf);
            printf("Put response code: %d\r\n", response);
            if (response == -1) while (true) ;
        }
#define READING
#ifdef READING
        // read signal strength
        response = m2xClient.listStreamValues(feedId, "rssi", on_data_point_found, NULL);
        printf("Fetch response code: %d\r\n", response);
        if (response == -1) while (true) ;
#endif    
        // update location
        if (location_valid) {
            response = m2xClient.updateLocation(feedId, name, latitude, longitude, elevation);
            printf("updateLocation response code: %d\r\n", response);
            if (response == -1) while (true) ;
        }
#ifdef READING
        // read location
        response = m2xClient.readLocation(feedId, on_location_found, NULL);
        printf("readLocation response code: %d\r\n", response);
        if (response == -1) while (true) ;
#endif
    }
    else {
        delay(100);
    }
  }

  mdm.disconnect();
  mdm.powerOff();
}