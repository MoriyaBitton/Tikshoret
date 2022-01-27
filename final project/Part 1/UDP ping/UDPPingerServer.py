# UDPPingerServer.py
# We will need the following module to generate randomized lost packets

import random
from socket import *

# Create a UDP socket
# Notice the use of SOCK_DGRAM for UDP packets
serverSocket = socket(AF_INET, SOCK_DGRAM)

# Assign IP address and port number to socket
serverSocket.bind(('', 12000))          # '' IS a Symbolic name meaning all available interfaces
print("UDP server on port 12000 listening...")

while True:
    # Generate random number in the range of 0 to 10
    rand = random.randint(0, 10)

    # Receive the client packet along with the address it is coming from
    message, address = serverSocket.recvfrom(1024)      # 1024 just defines the buffer size

    # If rand is less is than 4, we consider the packet lost and do not respond
    if rand < 4:
        continue

    print('message: ' + message.decode("UTF-8") + ' from client IP='+ str(address[0])+ ' port='+ str(address[1]))

    # Otherwise, the server responds
    serverSocket.sendto(message, address)
