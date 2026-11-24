
<!-- PROJECT SCREEN SHOT -->
![Product Name Screen Shot][product-screenshot]
Note: This screenshot is of GenisysSlave-Simulator version 0.5.0.9

## Genisys Protocol

Genisys is a master/slave communications protocol commonly used in the rail industry.
The protocol is typically used for non vital coms between field devices (PLC/RTU) and SCADA systems.
Genisys is very similar to MODBUS with the main difference being
Genisys uses event based polling and MODBUS uses full frame polling.

## ☕ Donations

If you find this project useful and would like to support its
continued development, maintenance and future improvements,
please consider making a donation. Donations are entirely voluntary and greatly appreciated.

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/LY3000)

## About The Project

**GenisysSlave-Simulator:** A Microsoft Windows application the allows a user to simulate a Genisys slave device over a IP network or serial com port.
This application may be useful to SCADA developers that need to connect to a 
Genisys slave device without having a physical device.

**GenisysSlave-Lib:** A Microsoft Windows C++ library containing a basic Genisys slave protocol implementation. Library has networking, protocol and data frame classes. 

**Warning** - This application/library should NEVER be used in an active rail environment.
The source code dose minimal error checking/handling and was NOT written to any safety standard.
Should be used for rudimentary testing purposes only.

## Genisys Master (Development)

**GenisysMaster-Lib** is a separate Windows C++ master library for testing slave
implementations over UDP or serial. It supports multiple slave IDs, polling,
recalls, indication callbacks, explicit controls, retries, and communication status.
See [library usage](GenisysMaster-Lib/README.md).

**GenisysMaster-Simulator** is a wxWidgets master test app styled like the slave
simulator. It supports UDP and serial, live indication and editable control bit
grids, manual recalls, and XML configurations compatible with the retired master apps.
In Visual Studio select **GenisysMaster-Simulator** as the startup project and
build **Debug | x64** or **Release | x64**. The project uses the same `WX_WIN`
SDK setting as the slave simulator and copies matching wxWidgets DLLs and both
XML examples beside the executable. Use **Open XML** to load an example, review
the settings, and press **Start**. Click control bits or double-click a Hex cell
to edit a draft, then use **Send changes** or **Send full frame**.

The former master console and Qt GUI are preserved in Git at the
`archive/master-console-and-qt-gui` tag.

## Usage (For Users)

Download the latest version of "GenisysSlave-Simulator" from the **Releases** page.

**Protocol:** Is the network protocol (UDP/TCP/Serial) that the application is using.
Note: The TCP server (slave) will only accept 1 TCP client (master) connection at a time. 

**Receiving:** Is the local UDP/TCP port number the application is listening on.

**Transmitting:** Is the port the Genisys master is listening on.

**Serial Settings:** Is the relevant serial settings (i.e COM Port, Baud, ect...)

**Slave ID:** Is the Genisys slave ID the simulator is listening for.

**Frame Size:** Is the number of data bytes hosted on the Genisys slave.

**Always send recall response:** Makes every poll for the selected slave ID return the full indication table instead of only changed bytes.

**Include mode byte in recall:** Includes the configured response mode byte in explicit recalls and full-table poll responses for the selected slave ID. This is enabled by default.

**Indication data:** Click a bit to toggle it, or double-click a Hex cell to edit the complete byte. A yellow Hex cell indicates data that differs from the last value acknowledged by the master.

**Start/Stop:** Toggles the Genisys slave server to start and stop.

**Last Master IP:** Shows the IP address of the last received message from a Genisys master.
This can help determine if this slave has multiple masters polling it.

**Responce Counter:** Is the current number of responce messages this slave has send to the master.

**Notes:**
The parameters above can not be changed while the server is running.
Server must be stoped and restarted to register the updates.


## Limitations
- The master supports UDP and serial. TCP support is limited to the slave simulator in server mode.
- LSY::DataFrame objects are not currently thread safe. These objects will be used by the Genisys lib and by user code, so they should be thread safe.


## Known Issues
- The library can't handle alot of messages for different slave IDs hosted on the same UDP port. This can likely be fixed by improving byte buffer processing.
- Multiple Genisys masters polling the same Genisys UDP slave may lead to sync issues.
- Only one Genisys master (TCP Client) can connect to the simulator when in TCP mode.
- As of GenisysSlave-Simulator 0.4.0.8 a custom build of wxWidgets is required, the custom build dlls are included in the release but, the standard build should be swaped in the future.


## License
MIT (see LICENSE.txt)


## Development Tools
C++, Winsock, wxWidgets, Visual Studio 2026, Hercules, SerialTool.


## AI Usage
As of GenisysSlave-Simulator 0.4.0.8 AI tools (Github Copilot and Open AI Codex) will be used for code generation, any AI code generated will be human reviewed before merging to git main.

OpenAI Codex developed Genisys Master, including the master library and simulator, with limited manual testing and review by Luke Young.



<!-- MARKDOWN LINKS & IMAGES -->
[product-screenshot]: docs/Genisys-Slave-Simulator-App-0.5.0.9-Screenshot.PNG
