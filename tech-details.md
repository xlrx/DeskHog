# DeskHog technical details

Microcontrollers are a pain. They've got limited memory and, for our purposes here, you've got to write C++ 🫠

But in exchange, our code can touch reality like no other kind of project. Here's what we're dealing with.

### Core and task isolation

If you've ever written mobile code, you'll feel right at home: we can only update the UI via the UI thread, otherwise the board crashes.

We've got two cores and multiple "tasks" assigned between them – task is [FreeRTOS](https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/FreeRTOS/BasicMultiThreading)-speak for threads:

**Core 0 (Protocol CPU) tasks:**

- WiFi
- Web portal server
- Insight parsing
- NeoPixel control

**Core 1 (Application CPU) tasks:**

- LVGL tick (maintains timing, animations, etc for the graphics library) 
- UI: screen drawing and input handling

We have to keep this stuff carefully isolated or we're going to crash.

### Buttons

<img width="500" alt="diagram" src="https://github.com/user-attachments/assets/14ea2440-90d8-4540-bebb-045c18fbbc99" />

If the board isn't responding:

- Hold **▼ (Page down/D0)**
- Press **Reset**
- Release **▼ (Page down/D0)**

The board will restart in bootloader mode, where it can be re-flashed using PlatformIO.

### Pin definitions

You don't have to guess the pin definitions. You'll find them documented here:

~/.platformio/packages/framework-arduinoespressif32/variants/adafruit_feather_esp32s3_reversetft/pins_arduino.h

## UI progress

- Status card: working
- WiFi provisioning card with QR Code: working
- Friend card to give you (mild) reassurance: working
- Numeric card for Big Number insights: working
- Funnel card: needs a redesign; probably should be horizontal layout instead, won't display more than three steps right now
- Line graph card: working decently, but could use more detail
- Other insights: not yet supported

## Important components

### Event queue

`EventQueue` is how the project manages communication between tasks and prevents coupling. Events – changes via the web UI, returned requests from the PostHog client – are dispatched out of core 0 to be received by the UI task. Any important data can be safely copied from one context into the other, preventing crashes and other drama.

### Card stack

The UI is a stack of cards. The user navigates between them using built-in buttons (the arrow keys)

`CardNavigationStack` manages the UI presentation of these cards, animating transitions.

`CardController` manages updates to the stack contents. If an insight is deleted or added via web UI, the controller processes that update reactively.

### Web UI

A basic provisioning and configuration UI is provided. You can access it via a QR code on first launch, and by the IP shown in the status screen once WiFi is configured.

Open `html/portal.html` in your browser to preview changes. The contents of `html` are inlined into a single file on each build by `htmlconvert.py`.

**Web portal budget:** Right now the portal costs about 18KB. We'll allow up to **100KB**. All portal assets must be locally available, since the portal needs to work when the device doesn't have WiFi. If you want to try adding a more complex UI framework than hand-rolled JS and HTML, you're welcome to try as long as its build system is quick and the final static output is under 100KB.

### Cards

`ProvisioningCard` displays a QR code to connect to the device. If WiFi is connected, it displays connection stats.

`InsightCard` visualizes PostHog data. Numeric card is working best. The rest need help.

`FriendCard` lets Max the hedgehog visit with you and provide encouragement.

### Insight parser and PostHog client

`InsightParser` ingests PostHog API responses and makes them available to the UI. `PostHogClient` constructs requests and dispatches responses.

### LVGL

This project relies on the powerful [LVGL project](https://docs.lvgl.io/9.2/intro/index.html) at [v9.2.2](https://registry.platformio.org/libraries/lvgl/lvgl?version=9.2.2) for drawing, animation and other UI tasks.

### Config manager and captive portal

`ConfigManager` handles persistent storage and retrieval of credentials and insights. `CaptivePortal` provides the web server and interacts with `ConfigManager` to read and write to persistent storage.
