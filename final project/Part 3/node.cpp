#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "select.h"
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <map>
#include <sstream>
#include <sys/ioctl.h>
#include <net/if.h>

using namespace std;

struct contact
{
    int id;
    char *ip = (char *)malloc(12);
    int port;
    int sock_num;

    contact(int id, char *ip, int port, int sock_num)
        : id(id), port(port), sock_num(sock_num)
    {
        memcpy(this->ip, ip, 12);
    }

} typedef contact;

struct packet
{
    int msg_id;
    int source_id;
    int destination_id;
    int trail_msg;
    int func_id;

    packet(char *server_packet)
    {

        this->msg_id = *((int *)(server_packet));
        this->source_id = *((int *)(server_packet + 4));
        this->destination_id = *((int *)(server_packet + 2 * sizeof(int)));
        this->trail_msg = *((int *)(server_packet + 3 * sizeof(int)));
        this->func_id = *((int *)(server_packet + 4 * sizeof(int)));
    }

} typedef packet;

struct node
{
    int id;
    char *ip = (char *)malloc(12);
    unsigned int port;
    int sock;
    int msg_id;

    // when i want to send
    map<int, contact> id_to_contacts;

    // when i get a msg
    map<int, contact> sock_to_contacts;
    map<int, pair<int, vector<int>>> paths; // k= dest , v= <len, path>
    map<int, pair<int, int>> pending_route; // k= msg_id pending for reponse from connected node
                                            // v= <discover_msg source, source_id> ;
    map<int, int> until_nack;
    map<int, vector<string>> pending_to_send;

    node(int port, char *ip, int id) : port(port), id(id)
    {
        memcpy(this->ip, ip, 12);
    }

    char *create_packet(int msg_id, int source_id, int destination_id, int trail_msg, int func_id)
    {
        char *client_packet = (char *)malloc(20);

        memcpy(client_packet, &msg_id, sizeof(int));
        memcpy(client_packet + 4, &source_id, sizeof(int));
        memcpy(client_packet + 2 * sizeof(int), &destination_id, sizeof(int));
        memcpy(client_packet + 3 * sizeof(int), &trail_msg, sizeof(int));
        memcpy(client_packet + 4 * sizeof(int), &func_id, sizeof(int));
        return client_packet;
    }

    void accept_connection(int socket_server)
    {
        struct sockaddr_in node_addr;
        int new_server_sock = 0;

        socklen_t len = sizeof(node_addr);
        new_server_sock = accept(socket_server, (struct sockaddr *)&node_addr, &len); /* accept connection - return val is the sockt we speek throw */

        if (new_server_sock < 0)
        {
            cout << "nack" << endl;
        }

        add_fd_to_monitoring(new_server_sock);
        char *packet_client = (char *)malloc(512);
        if (recv(new_server_sock, packet_client, 512, 0) < 0) /* recv connect packet */
        {
            cout << "nack" << endl;
        }

        // extract
        packet client_packet{packet_client};
        free(packet_client);

        // create packet to send
        int client_msg_id = client_packet.msg_id;
        char *payload = (char *)malloc(492);
        memcpy(payload, &client_msg_id, sizeof(int));

        char *server_packet = (char *)malloc(512);
        memcpy(server_packet,
               this->create_packet(this->msg_id++, this->id, client_packet.source_id, 0, 1), 20);
        memcpy(server_packet + 20, payload, 492);

        if (send(new_server_sock, server_packet, 512, 0) < 0)
        {
            cout << "nack" << endl;
        }

        free(server_packet);
        free(payload);

        contact new_nei(client_packet.source_id, inet_ntoa(node_addr.sin_addr),
                        (int)ntohs(node_addr.sin_port), new_server_sock);

        this->id_to_contacts.insert({client_packet.source_id, new_nei});
        this->sock_to_contacts.insert({new_server_sock, new_nei});
    }

    void send_message(vector<string> res /*send, id, len, message*/)
    {
        // data
        if (res.size() > 4)
            std::cout << "nack" << std::endl;

        stringstream str_id(res.at(1));
        int dest_id = 0;
        str_id >> dest_id;

        stringstream str_len(res.at(2));
        int len = 0;
        str_len >> len;

        std::string message = res.at(3);

        for (uint i = 4; i < res.size(); i++)
            message += "," + res[i];

        // send to ni
        if (this->id_to_contacts.find(dest_id) != this->id_to_contacts.end())
        {
            // create packet to send
            char *payload = (char *)malloc(492);
            memcpy(payload, &len, sizeof(int));
            memcpy(payload + sizeof(int), message.c_str(), message.length());

            char *client_packet = (char *)malloc(512);
            memcpy(client_packet, this->create_packet(this->msg_id++, this->id, dest_id, 0, 32 /*send*/), 20);
            memcpy(client_packet + 20, payload, 492);

            // send...
            if (send(this->id_to_contacts.at(dest_id).sock_num, client_packet, 512, 0) < 0)
            {
                cout << "failed to send" << endl;
            }
        }
        else if (this->paths.find(dest_id) != this->paths.end()) // we have a path
        {

            this->relay(message, dest_id);
        }
        else // DISCOVER - we need to find path
        {
            for (auto const &[key_id, contact] : this->id_to_contacts)
            {

                char client_packet[512];
                bzero(&client_packet, sizeof(client_packet));

                memcpy(client_packet,
                       this->create_packet(this->msg_id++, this->id, key_id, 0, 8), 20);

                // payload dest_id
                memcpy(client_packet + 20, &dest_id, sizeof(int));

                if (send(this->id_to_contacts.at(key_id).sock_num, client_packet, 512, 0) < 0)
                {
                    cout << "failed to send" << endl;
                }
            }

            this->pending_to_send.insert({dest_id, res});
        }
    }

    void recv_message(char *message)
    {
        packet client_packet{message};

        // print msg
        cout << message + 6 * sizeof(int) << endl;

        // create packet to send
        int client_msg_id = client_packet.msg_id;
        char *payload = (char *)malloc(492);
        memcpy(payload, &client_msg_id, sizeof(int));

        char *server_packet = (char *)malloc(512);
        memcpy(server_packet, this->create_packet(this->msg_id++, this->id, client_packet.source_id, 0, 1 /*ack*/), 20);
        memcpy(server_packet + 20, payload, 492);

        // send
        int dest_id = client_packet.source_id;
        if (send(this->id_to_contacts.at(dest_id).sock_num, server_packet, 512, 0) < 0)
        {
            cout << "failed to send" << endl;
        }
    }

    void send_nack(int msg_id, int dest_id)
    {
        // create packet to send
        char *payload = (char *)malloc(492);
        memcpy(payload, &msg_id, sizeof(int));

        char *nack_packet = (char *)malloc(512);
        memcpy(nack_packet, this->create_packet(this->msg_id++, this->id, dest_id, 0, 2 /*nack*/), 20);
        memcpy(nack_packet + 20, payload, 492);

        // send...
        if (send(this->id_to_contacts.at(dest_id).sock_num, nack_packet, 512, 0) < 0)
        {
            cout << "failed to send" << endl;
        }
    }

    void remove_nei(int fd)
    {
        int id_to_rmv = this->sock_to_contacts.at(fd).id;
        this->sock_to_contacts.erase(fd);
        this->id_to_contacts.erase(id_to_rmv);
    }

    void relay(string message, int dst)
    {
        // create packet to send
        int next_node = this->paths.at(dst).second.at(1);
        int len = message.length();
        char *payload = (char *)malloc(492);
        memcpy(payload, &dst, sizeof(int));
        memcpy(payload + sizeof(int), &len, sizeof(int));
        memcpy(payload + 2 * sizeof(int), message.c_str(), message.length());

        cout << "need to relay " << payload + 2 * sizeof(int) << endl;

        char *realy_packet = (char *)malloc(512);
        memcpy(realy_packet, this->create_packet(this->msg_id++, this->id, next_node, 0, 64 /*send*/), 20);
        memcpy(realy_packet + 20, payload, 492);

        // send...
        if (send(this->id_to_contacts.at(next_node).sock_num, realy_packet, 512, 0) < 0)
        {
            cout << "failed to send" << endl;
        }
    }

    void recv_relay(char *message)
    {
        packet client_packet{message};

        // print msg
        cout << message + 7 * sizeof(int) << endl;
    }

} typedef node;

int main(int argc, char const *argv[])
{
    // ***to get ip address of my computer - start
    char ip_address[15];
    int fd;
    struct ifreq ifr;

    /*AF_INET - to define network interface IPv4*/
    /*Creating soket for it.*/
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /*AF_INET - to define IPv4 Address type.*/
    ifr.ifr_addr.sa_family = AF_INET;

    /*eth0 - define the ifr_name - port name
    where network attached.*/
    memcpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

    /*Accessing network interface information by
    passing address using ioctl.*/
    ioctl(fd, SIOCGIFADDR, &ifr);
    /*closing fd*/
    close(fd);

    /*Extract IP Address*/
    strcpy(ip_address, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    // ***to get ip address of my computer - start

    char buff[1025];

    // port
    int my_port = atoi(argv[1]); /* Port fron user */
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));

    // allocate sockaddr_in
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_address);
    serv_addr.sin_port = htons(my_port);

    // sockets
    int socket_server = 0;
    socket_server = socket(AF_INET, SOCK_STREAM, 0); /* open socket TCP - listen to connect request */

    if (socket_server < 0)
    {
        printf("Error while creating socket\n");
        return -1;
    }

    int yes = 1;
    if (setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    // bind  sockets to port
    if (bind(socket_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) /* connect socket to ip & port */
    {
        printf("Couldn't bind to the port\n");
        return -1;
    }

    add_fd_to_monitoring(socket_server); /* add new socket to sockets list */

    if (listen(socket_server, 10) < 0) /* socket is listening */
    {
        printf("Error while listening\n");
        return -1;
    }

    int ret = 0;

    // create node
    char *ip = (char *)malloc(12);
    memcpy(ip, ip_address, 12);
    node node{my_port, ip, -1};

    // pending for id
    while (node.id == -1)
    {
        ret = wait_for_input();
        if (ret == 0)
        {
            read(ret, buff, 1025);
            string s(buff);
            string token = s.substr(0, s.find(','));

            if (token == "setid")
            {
                size_t pos = s.find(',');
                int id = atoi((buff + 6));
                node.id = id;
                node.msg_id = (id - 1) * 100; /* init uniqe msg id */
                cout << "ack" << endl;
            }
            else
            {
                cout << "cant do action without id" << endl;
            }
        }
    }

    while (true) // 0- input, 1- output, 2- error, (3+)- request from outside (socket server == 3)
    {
        char buff[1025];
        bzero(&buff, sizeof(buff));

        ret = wait_for_input(); /* fd who got replay */

        if (ret == 0) /* I want to do something */
        {
            read(ret, buff, 1025);
            string s(buff);
            string token = s.substr(0, s.find(','));
            string peers = s.substr(0, s.find('\n'));

            if (token == "connect") /* I want to connect net */
            {
                // init inf from user
                /* port to connect with */
                char *str_port = (buff + s.find(':') + 1);
                int port = atoi(str_port);

                /* ip to connect with */
                buff[s.find(':')] = '\0';
                char *ip = (buff + s.find(',') + 1);

                // open socket to contact - I ipen the TCP socket
                int new_client_sock = 0;
                new_client_sock = socket(AF_INET, SOCK_STREAM, 0);

                struct sockaddr_in nei_addr;
                nei_addr.sin_family = AF_INET;
                nei_addr.sin_addr.s_addr = inet_addr(ip);
                nei_addr.sin_port = htons(port);

                if (connect(new_client_sock, (struct sockaddr *)&nei_addr, sizeof(nei_addr)) < 0) // I send connecting request
                {
                    cout << "nack" << endl;
                } // connect success

                add_fd_to_monitoring(new_client_sock); /* after connection is success add socket to the list */

                // create data to server
                char *client_packet = (char *)malloc(512);
                memcpy(client_packet, node.create_packet(node.msg_id++, node.id, 0, 0, 4), 20); /* send "connect" packet */

                if (send(new_client_sock, client_packet, 512, 0) < 0) /* send "connect" packet request */
                {
                    cout << "nack" << endl;
                } // send "connect" packet request success
                free(client_packet);

                char *server_packet = (char *)malloc(512);
                if (recv(new_client_sock, server_packet, 512, 0) < 0) /* recv "connect" packet request */
                {
                    cout << "nack" << endl;
                } // recv "connect" packet request success

                // extract data from server packet
                packet packet{server_packet};
                free(server_packet);

                cout << "ack" << endl;
                cout << packet.source_id << endl;

                // add node to contact
                contact new_nei{packet.source_id, ip, port, new_client_sock};
                node.id_to_contacts.insert({packet.source_id, new_nei});
                node.sock_to_contacts.insert({new_client_sock, new_nei});

                node.paths.insert({packet.source_id, {2, {node.id, packet.source_id}}});
            }

            if (token == "send")
            {
                /* split - start */
                size_t pos_start = 0, pos_end, delim_len = 1;
                string token1;
                vector<string> res;

                while ((pos_end = s.find(',', pos_start)) != string::npos)
                {
                    token1 = s.substr(pos_start, pos_end - pos_start);
                    pos_start = pos_end + delim_len;
                    res.push_back(token1);
                }

                res.push_back(s.substr(pos_start));
                /* split - end */

                node.send_message(res);
            }

            if (token == "setid")
            {
                cout << "nack" << endl;
            }

            if (token == "route")
            {
                int id_to_route = atoi((buff + 6));
                if (node.id_to_contacts.find(id_to_route) != node.id_to_contacts.end())
                {
                    cout << "ack" << endl;
                    cout << node.id << "->" << id_to_route << endl;
                }
                else if (node.paths.find(id_to_route) != node.paths.end())
                {
                    cout << "ack" << endl;

                    vector<int> v_path = node.paths.at(id_to_route).second;
                    int len_path = node.paths.at(id_to_route).first;
                    for (int i = 0; i < len_path - 1; i++)
                    {
                        cout << v_path[i] << "->";
                    }
                    cout << v_path[len_path - 1] << endl;
                }
                else
                {
                    cout << "nack" << endl;
                }
            }

            if (peers == "peers")
            {
                cout << "ack" << endl;
                int size = node.id_to_contacts.size();
                for (auto it = node.id_to_contacts.begin(); it != node.id_to_contacts.end(); it++)
                {
                    cout << it->first;
                    if (size != 1)
                    {
                        cout << ",";
                    }
                    size--;
                }
                cout << endl;
            }
            continue;
        }

        // if a node try to connect
        if (ret == socket_server)
        {
            node.accept_connection(socket_server);
            continue;
        }

        // not stdin/out/err
        if (ret > 2)
        {
            char *client_packet = (char *)malloc(512);
            int bytes = recv(ret, client_packet, 512, 0);

            if (bytes <= 0)
            {
                if (bytes == 0)
                {
                    remove_fd(ret);
                    close(ret);

                    node.remove_nei(ret);
                }
                else
                {
                    cout << "failed recv" << endl;
                }
            }
            else
            {
                packet packet{client_packet};

                // ack
                if (packet.func_id == 1)
                {
                    cout << "ack" << endl;
                }

                // nack
                if (packet.func_id == 2)
                {
                    int org_msg = *((int *)(client_packet + 5 * sizeof(int)));

                    if (node.pending_route.find(org_msg) != node.pending_route.end())
                    {
                        int dest_id = node.pending_route.at(org_msg).second;
                        node.until_nack[dest_id]--;

                        // if all return nack
                        if (node.until_nack[dest_id] == 0)
                        {
                            node.until_nack.erase(dest_id);
                            node.send_nack(node.pending_route.at(org_msg).first, dest_id);
                        }

                        node.pending_route.erase(org_msg);
                    }
                }

                // discover
                if (packet.func_id == 8)
                {
                    int dest_id = *((int *)(client_packet + 5 * sizeof(int)));
                    int from_id = packet.source_id;

                    // we know a path to dest
                    if (node.paths.find(dest_id) != node.paths.end())
                    {

                        char server_packet[512];
                        bzero(&server_packet, sizeof(server_packet));

                        memcpy(server_packet,
                               node.create_packet(node.msg_id++, node.id, from_id, 0, 16), 20);

                        // payload dest_id
                        vector<int> v_path = node.paths.at(dest_id).second;
                        int len_path = node.paths.at(dest_id).first;

                        int payload[len_path + 2];
                        bzero(&payload, sizeof(payload));

                        payload[0] = packet.msg_id;
                        payload[1] = len_path;

                        // insert path to payload
                        for (int i = 0; i < len_path; i++)
                        {
                            payload[i + 2] = v_path[i];
                        }

                        memcpy(server_packet + 20, payload, (len_path + 2) * sizeof(int));

                        if (send(node.id_to_contacts.at(from_id).sock_num, server_packet, 512, 0) < 0)
                        {
                            cout << "failed to send" << endl;
                        }
                    }

                    // dest id connected to this node - send route
                    else if (node.id_to_contacts.find(dest_id) != node.id_to_contacts.end())
                    {
                        char server_packet[512];
                        bzero(&server_packet, sizeof(server_packet));

                        memcpy(server_packet,
                               node.create_packet(node.msg_id++, node.id, from_id, 0, 16), 20);

                        // payload dest_id
                        int payload[] = {packet.msg_id, 2, node.id, dest_id};
                        memcpy(server_packet + 20, payload, 4 * sizeof(int));

                        if (send(node.id_to_contacts.at(from_id).sock_num, server_packet, 512, 0) < 0)
                        {
                            cout << "failed to send" << endl;
                        }
                        // cout << "send route to " << from_id << endl;
                    }
                    else // not connected so send discover to connected nodes
                    {
                        bool nack = true;

                        for (auto const &[key_id, contact] : node.id_to_contacts)
                        {
                            if (key_id == from_id)
                                continue;

                            char server_packet[512];
                            bzero(&server_packet, sizeof(server_packet));

                            memcpy(server_packet,
                                   node.create_packet(node.msg_id, node.id, key_id, 0, 8), 20);

                            // payload dest_id
                            memcpy(server_packet + 20, &dest_id, sizeof(int));

                            if (send(node.id_to_contacts.at(key_id).sock_num, server_packet, 512, 0) < 0)
                            {
                                cout << "failed to send" << endl;
                            }

                            nack = false;

                            node.pending_route.insert({node.msg_id, {packet.msg_id, packet.source_id}});
                            node.until_nack[packet.source_id]++;

                            // cout << "send discover to " << key_id << endl;
                            // cout << "msg id pending for route " << node.msg_id << endl;

                            node.msg_id++;
                        }

                        // no nei to send them route -> send nack
                        if (nack == true)
                        {
                            node.send_nack(packet.msg_id, packet.source_id);
                        }
                    }
                }

                // route
                if (packet.func_id == 16)
                {
                    int org_msg = *((int *)(client_packet + 5 * sizeof(int)));
                    int len_path = *((int *)(client_packet + 6 * sizeof(int)));

                    // cout << "len path " << len_path << endl;
                    // cout << "src route msg_id " << org_msg << endl;

                    int path[len_path + 1];
                    bzero(&path, sizeof(path));
                    path[0] = node.id;

                    vector<int> v_path;
                    v_path.push_back(node.id);

                    int i = 0;
                    for (i = 1; i <= len_path; i++)
                    {
                        int n = *((int *)(client_packet + (6 + i) * sizeof(int)));
                        path[i] = n;
                        v_path.push_back(n);
                    }

                    node.paths.insert({path[len_path], {len_path + 1, v_path}});

                    // detect other paths
                    int count = len_path;
                    while (count >= 3) // smallest path
                    {
                        vector<int> v_small_path;
                        v_small_path.push_back(node.id);
                        int i = 0;
                        for (i = 1; i < count; i++)
                        {
                            v_small_path.push_back(*((int *)(client_packet + (6 + i) * sizeof(int))));
                        }
                        node.paths.insert({path[count - 1], {count, v_small_path}});
                        count--;
                    }

                    // if we need to relay the route
                    if (node.pending_route.find(org_msg) != node.pending_route.end())
                    {
                        char route_packet[512];
                        bzero(&route_packet, sizeof(route_packet));

                        int dest_id = node.pending_route.at(org_msg).second;
                        memcpy(route_packet,
                               node.create_packet(node.msg_id++, node.id, dest_id, 0, 16), 20);

                        // payload path
                        int new_len = len_path + 1;
                        int reply_msg = node.pending_route.at(org_msg).first;

                        memcpy(route_packet + 20, &reply_msg, sizeof(int));
                        memcpy(route_packet + 24, &new_len, sizeof(int));
                        memcpy(route_packet + 28, path, new_len * sizeof(int));

                        if (send(node.id_to_contacts.at(dest_id).sock_num, route_packet, 512, 0) < 0)
                        {
                            cout << "failed to send" << endl;
                        }

                        cout << "send route to " << dest_id << endl;
                        node.pending_route.erase(org_msg);
                    }
                    else if (node.pending_to_send.find(path[len_path]) != node.pending_to_send.end())
                    {
                        node.send_message(node.pending_to_send.at(path[len_path]));
                        node.pending_to_send.erase(path[len_path]);
                        cout << "ack" << endl;
                    }
                }

                // send
                if (packet.func_id == 32)
                {

                    node.recv_message(client_packet);
                }
                // free(client_packet);

                if (packet.func_id == 64)
                {
                    int dst = *((int *)(client_packet + 5 * sizeof(int)));

                    if (dst == node.id)
                    {
                        node.recv_relay(client_packet);
                    }
                    else
                    {
                        string message(client_packet + 7 * sizeof(int));
                        int next_node = node.paths.at(dst).second.at(1);
                        node.relay(message, next_node);
                    }
                }
            }
        }
    }
    return 0;
}
