from socket import *
import time

# Create a UDP socket
# Notice the use of SOCK_DGRAM for UDP packets
clientSocket = socket(AF_INET, SOCK_DGRAM)
# clientSocket.bind(('172.29.131.48', 5656))
clientSocket.settimeout(1)


host = 'localhost'
port = 12000

min_time = 1
max_time = -1
sum_time = 0
count_received_packets = 0

for i in range(10):

    start = time.time()
    msg = 'PING, sequence_number: ' + str(i) + ', time: '+ time.ctime(start)

    try:
        clientSocket.sendto(msg.encode('UTF-8'), (host, port))

        msg, address = clientSocket.recvfrom(1024)
        end = time.time()
        print('echo from server IP='+ str(address[0])+ ' port='+ str(address[1]) )
        print('message: ' + msg.decode("UTF-8"))

        elapsed = end - start
        print("RTT: " + str(elapsed) + " seconds\n")

        count_received_packets += 1
        sum_time += elapsed
        max_time = max(max_time, elapsed)
        min_time = min(min_time, elapsed)

    except timeout:
        print("Requested Time out for  sequence_number: " + str(i) + "\n")

clientSocket.close()

avg = sum_time/count_received_packets
packet_loss = 100 -(count_received_packets/10.0)*100

print('------UDP ping statistic------')
print('10 packets transmitted,', count_received_packets, 'received,', packet_loss, '% packet loss')
print('RTT max=', max_time, ' seconds, ', 'min=', min_time, ' seconds, ', 'avg=', avg, ' seconds, ')