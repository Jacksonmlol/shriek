# Shriek

![logo](icon.png)

a simple chat program cuz discord sucks and i dont trust signal.

![screenshot](screenshot.png)

## what is this

its a chat server + client written in C. theres no voice or video or any of that nonsense.
just text. the way god intended.

## building

you need gcc, make, and ncurses development headers.

```
# on debian/ubuntu
apt install build-essential libncurses-dev

# on arch
pacman -S base-devel ncurses

# on fedora
dnf install gcc make ncurses-devel
```

then just:

```
make
```

this gives you `shriek-server` and `shriek-client`.

## running the server

```
./shriek-server
```

it listens on port 44375 by default. thats just the port i picked.
hit ctrl-c to shut it down.

## running the client

```
./shriek-client <server address> [port] [username]
```

if you dont give a username it will ask you for one. port defaults to 44375.

example:

```
./shriek-client 192.168.1.50
./shriek-client localhost 44375 myname
```

## the protocol

its tcp with json messages. each message is a json object followed by the character ␃ (unicode 0x2403). the server sends and receives different types of messages like join, send_message, user_message, system_message, room_update, etc.

if you want to connect with something else like netcat you can do it manually if you know what youre doing.

## does it work

mostly. the client uses ncurses so its not pretty but it works. the server handles multiple people at once. i tested it.

## will this replace discord

no. use matrix or something. i wrote this for fun.
