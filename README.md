# GBPlay: Online Game Boy Multiplayer

GBPlay aims to enable Game Boy and Game Boy Color multiplayer over the internet
using original hardware. This [has](http://pepijndevos.nl/TCPoke/) been done
[before](https://www.youtube.com/watch?v=KtHu693wE9o), but existing projects are
limited to one game, require a hard-wired connection to a PC, and are generally
inaccessible to users without a technical background.

Our goal is to create a solution that supports as much of the Game Boy library
as possible and is plug and play, such that everything can be done entirely from
the Game Boy. This will take the form of a small dongle with a link cable
connector and Wi-Fi connectivity. Configuration will be done via a custom Game
Boy cartridge, with a mobile-friendly web page as a backup option.

Through this, we endeavor to learn more about hardware design, embedded systems,
and retro devices as we find out just how far old technology can be pushed
beyond what it was ever intended to do.

The project is currently in an early/exploratory stage.
Read more at [blog.gbplay.io](https://blog.gbplay.io).

## Repository structure

| Directory   | Description                                               |
| ----------- | --------------------------------------------------------- |
| `arduino/`  | Code for Arduino-based USB to Game Boy link cable adapter |
| `captures/` | Annotated link cable communication logs for various games |
| `docs/`     | Contents of [blog.gbplay.io](https://blog.gbplay.io)      |
| `esp/`      | Code for firmware of ESP32-based Game Boy Wi-Fi interface |
| `server/`   | Code for backend server                                   |
| `tools/`    | Test scripts for development and debugging                |
