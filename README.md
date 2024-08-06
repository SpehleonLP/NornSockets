
# Norn Sockets

NornSockets is a program that provides a uniform interface for webapps to interact with creatures games. It can connect to Creatures 1/2 via the DDE interface; C2E (windows) via the shared memory interface; and lC2E, macC2E via TCP.  And it will create a uniform websockets interface to interact with them.

It will also handle things for you like converting from standard unicode to the obsolete cp1252 encoding used by Creatures games; as well as things like, when you send CAOS to windows C2E it needs to be preficed with "execute"; but on mac/linux it can't be, but it needs to end in rscr or the engine crashes.  If your CAOS has extra whitespace C1 will crash; etc. NornSockets will handle this for you, just send your CAOS and it will deal with all that.

## Known Issues
* Seems to have difficulty connecting to C1/C2
* lC2E kicks it from the socket for some reason
* Debug log window doesn't wanna open properly.

## How it works
* Nornsockets is a windows service/posix daemon that launches on startup and always runs in the background.
* every 2 seconds or so NornSockets will poll the system to see if one of the games is open and try to access the shared memory interface/DDE interface/TCP interface.

## How to use:

* It will open a websockets server on port 34013, you can connect to it with:

	    let socket;
	    socket = new WebSocket('ws://localhost:34013');
		// note: browsers will only allow this if the page is a file:// if the URL is an HTTP/HTTPS,
		// the browser will refuse to connect because NornSockets does not have an SSL certificate.

* Upon connection NornSockets the webapp should wait for a message indicating it's ready to communicate, the message looks like this:
    * OnGameOpened \<engine> versionMajor.versionMinor \<name>
        * Example 1: `OnGameOpened Vivarium 1.0 Creatures` -> creatures 1
        * Example 2: `OnGameOpened C2E 2.286 Docking Station`
* Make sure you listen for closing messages!:
    * Example 1: `OnGameClosed Vivarium 2.0 Creatures 2` -> creatures 2
    * Example 2: `OnGameClosed C2E 2.286 Docking Station`
* Send your CAOS as a text mode message in the web socket:
    * `socket.send("targ norn outs gtos 0")` -> send the targ norn's moniker back to the server (c2e)
    * `socket.send("targ norn dde: getb monk")` -> send the targ norn's moniker back to the server (C1)

## Communicating from the game to the server

If you are connected to C2E then C2E can send messages to the server rather than a call/response framework.

Nornsocket's will poll C2e's debug log every tick, the results will be filtered as the following:
* `ws://url:port[protocol]` try to open a client websocket connection to the url, it will be rejected if the server does not have an SSL certificate.
* `ws[protocol]:message`, the server will search the open sockets for the given protocol and send all matching sockets the message
* the information in the debug log is understood line by line (note that dbg:outs will always append a newline!), if anything doesn’t match one of these then it will be forwarded to a log file, additionally it will open a console window so the debug log is visible for the user.

## Binary mode messages:

While websockets messages in text mode are forwarded to the game, messages sent in binary mode are interpreted as being for the NornSockets server itself.

Primarily these are used to read/write files in the server's directory.

It should be reading/writing files in the open game's directory.

This does not work.

### How to use:
* Binary message format:
    * 4 bytes - Request
    * CString (null terminated string) - arguments
    * 4 bytes buffer length
    * remaining: binary buffer
* Requests:
	* SAVE - only allowed to overwrite files saved by the server in the server file log.
	* LOAD - open a file and send it back to the web app: first 8 bytes are file length.
	* DLTE - delete a file, only allowed on files saved by the server in the server file log.
	* MOVE - move a file, only allowed on files saved by the server in the server file log.
	* QRCD - convert the URL to a QR code and save it as a bmp.
	* OOPE - open a client websockets connection (this is useful to get around the localhost connection restriction).
	* LOG\0 - write something to the server log.
	* DBG\0 - write something to the dbg console window.

# Credits

Made by: Geat_Masta

Reference material:
* [Creatures Developer Resource](http://double.nz/creatures/)
* [The Shee's Lost Knowledge](https://sheeslostknowledge.blogspot.com/)
* Bedalton
