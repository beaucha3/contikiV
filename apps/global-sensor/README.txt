Global-Sensor app
-----------------
-----------------

Overview
--------

This app provides a way to manage a network of sensors.  If a sensor implements this, the user can set the active sensor for all sensors on the network and read all sensors on the network.  This is currently done via the shell.  Eventually, the goal is to be able to poll individual sensors and be able to have individual sensors sleep for a given amount of time.


Network Structure
-----------------
The network of sensors will consist of the main sensor network, which actually takes the readings, and a control node, which sends commands to the sensors and receives readings and sends them on the USB port.  Any node implementing this app can be used as a control node.


Shell Commands
--------------

Syntax:

The syntax of shell commands is

  COMMAND SENSOR [addr0.addr1 [data]]

COMMAND is the command to be sent -
  sensor-sel - Sets the active sensor of the current node.
  sensor-read - Reads the active sensor of the current node.
  sensor-setall - Sets the active sensor for all nodes on the network.
  sensor-readall - Reads all nodes on the network and prints the data.

SENSOR is the sensor to set (ignored by sensor-read and sensor-readall)
  v - battery sensor
  i - sht11 battery indicator
  t - sht11 temperature
  h - sht11 humidity
  l - light photosynthetic
  s - ligth total solar
  x - Unsets current sensor

  Note:  Sensors do NOT have to be unset before the active sensor is changed.

addr0.addr1 -
  Rime address of the node to read from or set. Address 0.0 is a NULL address that is essentially ignored.

data - 
  An integer of data (0-255).  Currently, no command uses this, but a
  sleep-read command would make use of this.  If data is included in the argument, addr0.addr1 must be set.



