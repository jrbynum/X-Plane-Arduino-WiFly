//
// * Example UDP X-Plane Comms using a rn-171 or RN-XV wifly wireless sheild
// *
// * This sketch implements a simple UDP server that reads a UDP packet from x-plane
// * NOTE: The  #define SERIAL_BUFFER_SIZE 64 must be changed in HardwareSerial.cpp in the core lib directory
// * its located usually in Arduino\hardware\arduino\cores\arduino\ 
// * I changed it to #define SERIAL_BUFFER_SIZE 512, this will allow for up to 14 parameters to be selected from x-plane
// * before a serial buffer overflow occurs. Having said that... I can only get six to work with the current code... to many
// * serial.prints makes it two slow...
// * 
// *   Coded by Rusty Bynum
// *   jrbynum@shaw.ca
// *
// *   Beta - 0.0.1
// *
// *   X-Plane UDP packet Receive and parsing functions working 
// *
// *   TODO - ************************************************************
// *   Cleanup and optimize the code
// *   ADD Send Functionality
// *   ADD Encoder and Switch reads
// *   Add LCD
// *   Add Pots
// */  *******************************************************************

#include <WiFlyHQ.h>

//******************************************
/* Change these to match your WiFi network */
//******************************************
const char mySSID[] = "BYNUM25";
const char myPassword[] = "7804311040";


//**********************************************************
// eight unions to get the float values from the packet data
//**********************************************************

union {
  char bytevalue[4];   // --- first variable -  char or byte array 
  float floatvalue;    // ---- second variable - float
}  
p1;                   // ---- put the 4 bytes in p1.bytevalue, get the float from p1.floatvalue (or backwards)
                      // the same for P2 - p8 - can rework the code to just use one of these.....

union {
  char bytevalue[4];         
  float floatvalue;          
}  
p2;              
union {
  char bytevalue[4];        
  float floatvalue;          
}  
p3;              
union {
  char bytevalue[4];         
  float floatvalue;          
}  
p4;              
union {
  char bytevalue[4];         
  float floatvalue;          
}  
p5;              
union {
  char bytevalue[4];         
  float floatvalue;          
}  
p6;              
union {
  char bytevalue[4];        
  float floatvalue;          
}  
p7;              
union {
  char bytevalue[4];         
  float floatvalue;          
}  
p8;              


//**************************************
//Variable and buffers
//**************************************
char UDPBuffer[1500];
int index;
int parameters;
uint32_t lastSend = 0;
//int bytesToProcess = 0;
int numbytes;
boolean DataFlag = false;

//**************************************
//Functions
//*************************************
void terminal();
void PrintData(int ii);
void processBuffer(int numbytes);
int GetPacket();

//********************
//the wifly object
//********************
WiFly wifly;

//***************************************************************************
//setup
//set everything up
//Serial debug baud set to 115200 for speed
//wifly uses Mega Serial1 @ 230400 for speed
//remove the serial debug for final code and all the associated serial.prints
//****************************************************************************
void setup()
{
  char buf[32];

  Serial.begin(115200);
  Serial.println("Starting");
  Serial.print("Free memory: ");
  Serial.println(wifly.getFreeMemory(),DEC);

  Serial1.begin(230400);

  if (!wifly.begin(&Serial1, &Serial)) {
    Serial.println("Failed to start wifly");
    terminal();
  }

  if (wifly.getFlushTimeout() != 10) {
    Serial.println("Restoring flush timeout to 10msecs");
    wifly.setFlushTimeout(10);
    wifly.save();
    wifly.reboot();
  }

  /* Join wifi network if not already associated */
  if (!wifly.isAssociated()) {
    /* Setup the WiFly to connect to a wifi network */
    Serial.println("Joining network");
    wifly.setSSID(mySSID);
    wifly.setPassphrase(myPassword);
    wifly.enableDHCP();

    if (wifly.join()) {
      Serial.println("Joined wifi network");
    } 
    else {
      Serial.println("Failed to join wifi network");
      terminal();
    }
  } 
  else {
    Serial.println("Already joined network");
  }



  /* Setup for UDP packets, sent automatically */
  wifly.setIpProtocol(WIFLY_PROTOCOL_UDP);
  if (wifly.getPort() != 49000) {
    wifly.setPort(49000);
    /* local port does not take effect until the WiFly has rebooted (2.32) */
    wifly.save();
    Serial.println(F("Set port to 49000, rebooting to make it work"));
    wifly.reboot();
    delay(3000);
  }

  //********** will implement this when we do the write back to x-plane
  //    wifly.setHost("192.168.1.24", 8042);	// Send UDP packet to this server and port

  Serial.print("MAC: ");
  Serial.println(wifly.getMAC(buf, sizeof(buf)));
  Serial.print("IP: ");
  Serial.println(wifly.getIP(buf, sizeof(buf)));
  Serial.print("Netmask: ");
  Serial.println(wifly.getNetmask(buf, sizeof(buf)));
  Serial.print("Gateway: ");
  Serial.println(wifly.getGateway(buf, sizeof(buf)));
  Serial.print("Port: ");
  Serial.println(wifly.getPort());

  wifly.setDeviceID("Wifly-UDP");
  Serial.print("DeviceID: ");
  Serial.println(wifly.getDeviceID(buf, sizeof(buf)));


  Serial.println("WiFly ready");
}


//*********************************
//The main loop
//*********************************
void loop()
{
  //***********************************
  //call Getpacket - it does everything
  //***********************************
  GetPacket(); 
  
  //*********************************************
  //For debugging - low level comms to the wifly
  //*********************************************
  if (Serial.available()) {
    /* if the user hits 't', switch to the terminal for debugging */
    if (Serial.read() == 't') {
      terminal();
    }
  }

}
//***********************************************************************
//PrintData - used for debuging - remove for operation
// -pass the index value to see what parameter is being displayed
//***********************************************************************
void PrintData(int ii)
{
  Serial.println(" ");
  Serial.print("DATA - ");
  Serial.print("Bytes: ");
  Serial.print(numbytes);
  Serial.print(" Param: ");
  Serial.print(parameters);
  Serial.print(" Index: ");
  Serial.print(ii);
  Serial.print(" - P1: ");
  Serial.print(p1.floatvalue);
  Serial.print(" P2: ");
  Serial.print(p2.floatvalue);
  Serial.print(" P3: ");
  Serial.print(p3.floatvalue);
  Serial.print(" P4: ");
  Serial.print(p4.floatvalue);
  Serial.print(" P5: ");
  Serial.print(p5.floatvalue);
  Serial.print(" P6: ");
  Serial.print(p6.floatvalue);
  Serial.print(" P7: ");
  Serial.print(p7.floatvalue);
  Serial.print(" P8: ");
  Serial.print(p8.floatvalue);
  Serial.println(" ");

}

//****************************************************************************************
//processBuffer - routine to parse the data into floats, etc.
//pass the number of bytes read from the UDP routine
//****************************************************************************************
void processBuffer(int numbytes)
{
  int i;
  int x = 0;
  int z;
  int index;
 

  //****************************************************************************************************************************
  //the format for an x-plane UDP Data Packet: 41 Bytes - 5 Header Bytes - 4 Index Bytes - 8, 4 byte floats
  //    Header        Index         F1          F2          F3          F4          F5          F5          F7          F8
  //XX XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|XX XX XX XX|
  //D  A  T  A  x  I  x  x  x  B1 B2 B3 B4
  // repeat data does not add the header, just index and floats
  //****************************************************************************************************************************

  //lets calculate the number of parameters that were sent
  //the formula: (numbytes - 5) /36 = parameters
  //each data set is offset by 36 bytes
  parameters = (numbytes - 5) / 36;
  x = 0;
  z = 0;
  //outer loop to get all the parameters in the packet
  for(z = 0; z < parameters; z++)
  { 
    index = UDPBuffer[5 + (z * 36)]; //get the index
    for(i = (9 + (z * 36)); i < (13 + (z * 36)); i++)
    {// inner loop to parse the floats from each parameter
      p1.bytevalue[x] = UDPBuffer[i];
      p2.bytevalue[x] = UDPBuffer[i + 4];
      p3.bytevalue[x] = UDPBuffer[i + 8];
      p4.bytevalue[x] = UDPBuffer[i + 12];
      p5.bytevalue[x] = UDPBuffer[i + 16];
      p6.bytevalue[x] = UDPBuffer[i + 20];
      p7.bytevalue[x] = UDPBuffer[i + 24];
      p8.bytevalue[x] = UDPBuffer[i + 28];
      x++;
    }
    x = 0;
    //-for debuging - print to terminal
    PrintData(index);
  }

}

//************************************************************************
//GetPacket
//Routine to check for UDP data from x-plane
//returns the number of bytes read or 0 if timeout occurs because of no data
//5 second timeout 
//***********************************************************************
int GetPacket()
{
  int i;
  wifly.flush();
  wifly.flushRx(100);

  lastSend = millis();    
  numbytes = wifly.available();
  //       while(wifly.available() < 40)
  //wait for at least one packet - 41 bytes 
  while(numbytes < 41)
  {
    numbytes = wifly.available();
    if ((millis() - lastSend) > 5000) {
      Serial.print("Timeout...");
      Serial.println(" ");
      return 0;
      lastSend = millis();
    }
  }

  // we have at least 41 bytes - lets read until the wifly buffer is empty
  i = 0;
  while(wifly.available())
  {
    UDPBuffer[i++] = wifly.read();
  }
  numbytes = i;
  //process the data  
  processBuffer(numbytes); 
  //return the number of bytes read 
  return numbytes; 
}

#ifdef RUSTY
void serialEvent1()
{
  //wifly.flush();

  if(DataFlag == false)
  {
    bytesToProcess = 0;
    while(wifly.available())
    {
      UDPBuffer[bytesToProcess++] = wifly.read();
    }
    numbytes = bytesToProcess;
    if(numbytes > 40)
      DataFlag = true;
  }

  //    processBuffer(); 
  //    PrintData();

}

#endif

//**************************************************************
//terminal
//used to commincate with the wifly at low-level
//for debuggin - remove in final code
//*************************************************************
void terminal()
{
  Serial.println("Terminal ready");
  while (1) {
    if (wifly.available() > 0) {
      Serial.write(wifly.read());
    }


    if (Serial.available()) {
      wifly.write(Serial.read());
    }
  }
}


