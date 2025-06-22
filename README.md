
<!-- PROJECT SCREEN SHOT -->
![Product Name Screen Shot][product-screenshot]

## Genisys Protocol

Genisys is a master/slave communications protocol commonly used in the rail industry.
The protocol is typically used for non vital coms between field devices (PLC/RTU) and SCADA systems.
Genisys is very similar to MODBUS with the main difference being
Genisys uses event based polling and MODBUS uses full frame polling.


## About The Project

**GenisysSlave-Simulator:** A Microsoft Windows application the allows a user to simulate a Genisys UDP slave device over a IP network.
This application may be useful to SCADA developers that need to connect to a 
Genisys slave device without having a physical device.

**GenisysSlave-Lib:** A Microsoft Windows C++ library containing a basic Genisys slave protocol implementation. Library has networking, protocol and data frame classes. 

**Warning** - This application/library should NEVER be used in an active rail environment.
The source code dose minimal error checking/handling and was NOT written to any safety standard.
Should be used for rudimentary testing purposes only.

## Usage (For Users)

Download the latest version of "GenisysSlave-Simulator" from the **Releases** page.

**Receiving:** Is the local UDP port number the application is listening on.

**Transmitting:** Is the port the Genisys master is listening on.

**Slave ID:** Is the Genisys slave ID the simulator is listening for.

**Frame Size:** Is the number of data bytes hosted on the Genisys slave.

**Start/Stop:** Toggles the Genisys slave server to start and stop.

**Last Master IP:** Shows the IP address of the last received message from a Genisys master.
This can help determine if this slave has multiple masters polling it.

**Responce Counter:** Is the current number of responce messages this slave has send to the master.

**Notes:**
The parameters above can not be changed while the server is running.
Server must be stoped and restarted to register the updates.


## Limitations
- UDP only (RS232 and TCP to be designed)
- Genisys Slave/Server only (Genisys Master/Client to be designed)
- CRC checksums are not currently validated (They can be anything and still pass)
- LSY::DataFrame objects are not currently thread safe. These objects will be used by the Genisys lib and by user code, so they should be thread safe.


## Known Issues
- The library can't handle alot of messages for different slave IDs hosted on the same UDP port. This can likely be fixed by improving byte buffer processing.
- Multiple Genisys masters polling the same Genisys slave may lead to sync issues.


## License
MIT (see LICENSE.txt)


## Development Tools
C++, Winsock, wxWidgets (wxFormBuilder), Visual Studio 2022.





<!-- MARKDOWN LINKS & IMAGES -->
[product-screenshot]: docs/screenshot.PNG
