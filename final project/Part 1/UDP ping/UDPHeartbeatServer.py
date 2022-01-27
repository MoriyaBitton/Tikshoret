# UDPPingerServer.py
# We will need the following module to generate randomized lost packets

import sys
from socket import *
from time import time, sleep

# Create a UDP socket
# Notice the use of SOCK_DGRAM for UDP packets
serverSocket = socket(AF_INET, SOCK_DGRAM)


# Assign IP address and port number to socket
serverSocket.bind(('', 12000))          # '' IS a Symbolic name meaning all available interfaces
print("UDP server on port 12000 listening for client Heartbeat...\n")

expected_sequence = 0

while True:

    try:
        # Receive the client packet along with the address it is coming from
        heartbeat, address = serverSocket.recvfrom(1024)      # 1024 just defines the buffer size
        received_time = time()

        serverSocket.settimeout(5)  # set timeout after receive the first heartbeat

        heartbeat = heartbeat.decode("UTF-8")
        client_time, sequence_num = heartbeat.split("|")

        time_dif = received_time - float(client_time)

        sequence_num = int(sequence_num)
        sequence_diff = sequence_num - expected_sequence

        if sequence_diff != 0:
            for i in range(expected_sequence, sequence_num):
                print('Heartbeat packet sequence number ', i, "lost\n")

        print('Heartbeat number', sequence_num, 'from IP=', str(address[0]), ', time diff=', time_dif, 'secs\n')

        expected_sequence = sequence_num + 1

    except timeout:
        sys.exit('Client application has stopped.')
