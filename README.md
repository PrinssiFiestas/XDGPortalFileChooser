# XDG Portal File Chooser

Simple example how to invoke file opening dialog portably withing Linux. Portable here just means that this opens the file opening dialog that is native to the window manager using the appropriate XDG Portal. Probably won't work on Windows or Mac (didn't try, don't care). 

The program opens up the file opening dialog and simply just prints the selected file to the standard output. It is implemented using low-level libdbus API. Official [libdbus docs](https://dbus.freedesktop.org/doc/api/html/index.html) state that "if you use this low-level API directly, you're signing up for some pain" as the very first thing of the API documentation main page. However, the main benefit of using this API is that it practically introduces no dependencies since libdbus is quite fundamental to modern Linux desktops. Besides, I like pain, makes you feel alive. 

## Building

```bash
make
```

## Running

```bash
./portal
```

