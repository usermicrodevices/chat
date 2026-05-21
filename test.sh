curl -m1 -i -N -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==" -H "Sec-WebSocket-Version: 13" http://localhost:8080/ 2>/dev/null

#Send one message and stay connected (to see all broadcasts)
python3 ws.py ws://localhost:8080 --sender alice --content "Hello everyone" --room general

#Send a message and exit after the first response
python3 ws.py ws://localhost:8080 --sender bob --content "Hi" --room lobby --once

#Use custom sender and room
python3 ws.py ws://localhost:8080 --sender charlie --room random --content "Anybody here?"
