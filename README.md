# Slack Bot 🤖: IoT automation with ESP32 & RTM API

![ESP32 Slack Bot](/img/Slack_bot.png)

## Project Background

When I joined [empathy.co](https://www.empathy.co/) in order to lead the Labs Team, one of the first questions I asked myself was: 

How could I explain to new colleagues what’s the mission regarding empathy LABs (eLABS)? 🤔

People who know me closely know that I’m a new technology enthusiast. I like to prototype ideas 💡 that could become a solution to a problem. I love the ‘Do It Yourself’ philosophy and Maker culture as I consider it’s a great example of this design process: 

***Research → Draft → Prototype → Review → Refine***

That’s why I considered that the best way to let you understand the mission of eLABS was by doing something practical, something that could serve as a great example and... What better way than prototyping something useful?

That’s the reason I came up with the following idea:

At [empathy.co](https://www.empathy.co/) we use a Slack workspace as a communication channel as many companies out there nowadays. I had never used Slack previously, but since I joined the company and started using it the following weeks I came up with the idea that some tasks could be automated with a Slack Bot integration.

So this marks the start point of any kind of prototype: 

## 1. The idea 💡

The idea of this integration is to use a microcontroller [ESP32](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview) directly connected to the Internet via Wi-Fi in order to create a Bot designed to interact with users via chat messages. 

A bot 🤖 is the same as a regular app: it can access the same range of APIs and do all of the *‘magical things’* that a Slack App can do, but when you build a bot for your Slack App, you're giving that app a face, a name, a personality and encouraging users to talk to it.

Bots can automate some actions or even enhance user notifications with digital displays like a Neopixel LED strip 🌈

Integration examples:

* Enhance notifications with digital displays (user mention, channel, etc)
* Monitoring (room conditions, occupancy, etc)
* Remote control of any mechanism (remote control door opener, IoT, etc)
* ...
* Any idea you would like to automate! 💡🤔💭

## 2. The prototype ✍️

Once you get the idea, the prototyping phase starts. 

I usually draw a basic draft of the architecture of the application in a notebook following some kind of my own *‘Design Thinking’* process.

Below you can find the very first draft idea I realized, how could we automate e.g., the parking door opening:

With the blink of an eye, you can easily understand how it will work or get the point. Am I right? 😉

## 3. The build 🛠️

When you build something related to any 3rd party vendor services, there’s no other way to learn them than researching and reading through their online documentation. Luckily, Slack has a great documentation published online about their available [APIs](https://api.slack.com/start).

The prototype uses an ESP32 controller implementing the [Real Time Messaging API](https://api.slack.com/rtm) which is based on a WebSocket connection.

One of the most valuable things about the Maker community is their love and passion to share the projects as [Open Source](https://opensource.com/resources/what-open-source) and under [Open Source Licenses](https://opensource.org/licenses) that makes it possible for you to reuse their work. Luckily I was able to find on the Internet that a user [@urish](https://github.com/urish) had the same idea as me (4 years ago though!) and implemented a Slack Bot with the same API.

I looked into their source code available which helped me not to start from scratch the build process. But, what happened? I realized that **some things did not work as I expected or were missing…** 😟

So I developed my own source code based on his work (thanks again!) and started to code my own ‘fork’.

### 4. Code improvements ✨:

- **Project is portable:** you can bring with you the prototype wherever you want and you will just need to follow the Wi-Fi configuration portal to set up your Wi-Fi network to be used.

    - No need to hardcode your Wi-Fi credentials anymore as it uses a [Wi-Fi manager](https://github.com/tzapu/WiFiManager) library!

- **Personal/channel/group notifications:** you could set up the prototype to be used as a personal notification gadget at the side of your desk. 

    - For instance, you could show some animations on a NeoPixel Ring / Strip or even play an alarm sound whenever e.g., your user name (@MemberID) is mentioned on any channel!

- **Project based on ESP32:** which means you have more GPIOs available to connect any sensors, actuators, displays, lights...

    - The ESP32 is much more powerful 💪!

- **Parse any word from any message sent to the bot:** you can look for any word sent on any message. 

    - It opens great possibilities to develop your own solutions based on personalised commands 🤔💡💭!

- **Project build with [PlatformIO](https://platformio.org/):** no need to install manually libraries or depencies. Just download source code, open it with PlatformIO, connect your ESP32 board, select the right Serial port and upload.

    - [PlatformIO](https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation) will automatically download all the necessary resources 🎩✨! You will just need to setup your own configuration variables accordingly


## Code configuration:

**Slack SSL SHA-1 fingerprint**

```cpp
#define SLACK_SSL_FINGERPRINT "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3"
```
> If Slack changes the SSL you would need to update with the new SHA-1 fingerprint.
You can check it by looking at certificate details when accessing the Slack API website. 

**Slack App and Bot integration**

1) Get ```token``` by creating new App at https://api.slack.com/apps
2) Add a **Bot User** to your App
3) Then **install your App** to your Slack **workspace** to generate the ```Bot User OAuth Access Token```
4) Set the following OAuth Scopes: ```channels:read``` / ```chat:write:bot``` / ```bot``` / ```incoming-webhook```
5) Locate your ```<@MEMBER_ID>``` in your Slack Space: you can find it in ***Profile & account -> ... (3 vertical dots) -> Copy Member ID***
6) Then **setup your own values** in source code '*/src/**main.cpp***'

```cpp
#define SLACK_BOT_TOKEN "Your_Bot_User_OAuth_Access_Token"
#define MEMBERID "<@MEMBER_ID>"
```
**Wi-Fi Configuration**

The code uses the **[WifiManager](https://github.com/tzapu/WiFiManager)** library, which eases the W-Fi configuration.

Once the code has been uploaded, the ESP32 will create by default a **Wi-Fi *Access Point*** called **“EmpahtyBot_AP”** and *password* **“labs2020”**.

Connect to it using any WiFi enabled device with a browser (computer, phone, tablet) and then the Web Captive Portal will start. You will be able to **scan and set up your Wi-Fi** connection.

Select your Wi-Fi SSID, introduce your password, **click save** & you’re ready to go!

> If you would need to reset the settings to dafault, just uncomment the below line & upload code to the board again.
```c++
// wm.resetSettings();
```

## DEMO SETUP CONFIGURATION

**Hardware Setup**

- **16 LED NeoPixel ring** connected to GPIO32
- **BME280 sensor** (I2C: connected as → SDA GPIO21 - SCL GPIO22 - GND - 3.3V)
- **LED** connected to GPIO27
- **Servo** connected to GPIO26
- **RCWL-0516** Microwave proximity sensor connected to GPIO35
- **12V Relay** connected to GPIO12 (relay is connected to a 12V Sound Alarm as example)

**Default Commands**

- **red / blue / green** →  The NeoPixel ring will show the color selected

- **off** →  The NeoPixel ring will go off

- **:door:** →  If the door emoji 🚪 is sent, the servo will be activated (as example, in the setup configuration demo the servo press down the button of a remote controller which opens a Garage door)

 - **room_status** → The bot will reply back to the user with the information from the BME280 sensor (temperature & pressure) & the microwave sensor will detect if there is someone in the room where the device is placed, being useful to know if it is free or not.

If the message sent by any user contains your ```<@MEMBER_ID>``` in the Slack ```#channel``` where the Bot has been integrated, the NeoPixel ring will show you an animation & then the sound alarm connected to the 12V relay will be turned on for a few seconds.

Check video DEMO setup configuration [example](https://twitter.com/AlexCorvis84/status/1251226999315738626?s=20) running.
