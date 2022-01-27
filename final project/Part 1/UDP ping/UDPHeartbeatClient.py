from socket import *
from time import time, sleep
import random

# Create a UDP socket
# Notice the use of SOCK_DGRAM for UDP packets
clientSocket = socket(AF_INET, SOCK_DGRAM)
# clientSocket.bind(('172.29.131.48', 5656))


host = 'localhost'
port = 12000

for i in range(10):
    rand = random.randint(0, 10)

    # If rand is less is than 4, we consider the packet lost and do not send it
    if rand < 4:
        print('Heartbeat packet sequence number ', i, "lost\n")
        continue

    heartbeat = str(time()) + '|' + str(i)
    clientSocket.sendto(heartbeat.encode('UTF-8'), (host, port))

    print('Heartbeat number', i, 'sent to server IP=', str(host), '\n')

    sleep(1)

clientSocket.close()

