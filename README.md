# ABB_Solar_Power_Inverter_2MQTT4HA
Arduino code to use ESP8266 to send inverter data to Home Assistant over MQTT.

I wanted data from my ABB solar power inverter available on Home Assistant.

I found Home Assistant has an integration to do exactly this, but I didn't want to run the wire from where my inverter is to where my HA server is.  
https://www.home-assistant.io/integrations/aurora_abb_powerone/

I found this github repository that developed the library to enable communication from microcontrollers to the inverter via the RS-485 communication port.
https://github.com/xreef/ABB_Aurora_Solar_Inverter_Library

I used this library to write an Arduino IDE sketch for the ESP8266 to pass information from the inverter to MQTT.

Hardware: I chose to use an ESP8266 with a RS-485 to 3V UART converter found on Amazon.

This is not intended to be a guide, but the code can be a shortcut for someone else trying to do the same thing.  Yes, you can tell from the code that I'm not a professional programmer.
