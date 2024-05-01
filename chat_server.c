#include "dllist.h"
#include "fields.h"
#include "jrb.h"
#include "jval.h"
#include "sockettome.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// USAGE: ./bin/chat_server [hydra#.eecs.utk.edu] [port num > 8000] [chat room names]

/*IMPORTANT NOTES
  * r chat rooms
    c clients
    total threads = r + c + 1

  * one thread in a while loop waiting for clents to attach to the socket
    once it detects a client it will make a new client thread

  * one thread for each chat room, blocked on condition variable unique to the room thread
    when unblocked it means that the server has recieved input from a client
    this input will go on a list for that server
    for each string on the list it should traverse the list of clients connected and send each of them the string
    then wait on the condition variable again

  * one thread for each client, blocked reading from the socket
    once it recieves a line of text from the socket it will construct the string from it
    then put the string on the chat room list and signal the chat room

  * use fdopen twice on each connection.
    client threads call fgets() and fputs() on stdio buffers until the clients name/chat-room obtained
    then client thread only calls fgets
    chat room threads call fputs() and fflush()

  * If EOF is read from client infor, close the buffer and kill the thread

  * after a client joins a chat room, if EOF on fgets() then close the input buffer.
    close the output buffer if the chat room thread is not trying to write to it.
    remove the client from the chat room list and kill the client thread

  * if the chat room thread detects a problem on fputs() or fflush() then remove the client from room list
    close the output buffer.
    client should be dealt with automatically sonce EOF on fgets()


    1-5: 2 to 10 clients joining and exiting.
    6-10: 2 clients, one room, one line of text.
    11-15: 3 to 12 clients, one room, one line of text.
    16-20: 2 to 12 clients, one room, two lines of text.
    21-25: 3 to 12 clients, two rooms, two lines of text.
    26-30: 3 to 12 clients, one room, one line of text per client.
    31-35: 3 to 12 clients, two rooms, one line of text per client.
    36-40: 3 to 12 clients, three rooms, one line of text per client.
    41-100: 4 to 24 clients, four to ten rooms, 30 to 259 lines of text.


when a client connects:
  1. sends client info about the chat rooms, listed alphabetically. with people in room ordered on when they joined
  2. the format is:
              room_1_name: 1st_person_to_join, 2nd_person_to_join, ...
              room_2_name: (even if no one is in still print this)
              room_3_name: 1st_person_to_join
  3. then prompt user for their name then the chat room
  4. error check the input including EOF
  5. when someone joins a line is sent to everyone in the chat room that the person has joined
  6. input format:
              Enter your chat name (no spaces):
              Dr-Plank
              Enter chat room:
              Bridge
              (second client connects)
              Enter your chat name (no spaces):
              Goofus
              Enter chat room:
              Bridge
              (in the bridge chat room)
              Goofus has joined
*/
/*TODO
  1. make linked list node struct
  2. Chat room struct- name, node->head, node->tail, node->output, mutex user_list_lock, mutex output_list_lock, thread_cond output_cond
  3. client struct- file descriptors, struct chat_room, num of rooms in
*/

struct list_node {
  char *str;               // data
  FILE *fout;              // file stream
  struct list_node *flink; // front link
  struct list_node *blink; // back link
};

struct chat_room {
  char *room_name;               // room name
  struct list_node *client_head; // client list head
  struct list_node *client_tail; // client list tail
  struct list_node *output_head; // output list head
  pthread_mutex_t client_lock;   // client list mutex lock
  pthread_mutex_t output_lock;   // output list mutex lock
  pthread_cond_t output_cond;    // output conditional
};

struct client {
  int num_rooms_in;        // num of rooms client is a part of
  int fd;                  // file descriptors
  struct chat_room *rooms; // chat rooms client is in
};

int main() {
}