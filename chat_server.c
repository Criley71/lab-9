#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dllist.h"
#include "jrb.h"
#include "sockettome.h"

struct server {
  JRB chat_rooms_jrb;
};

typedef struct chat_room {
  char *name;
  Dllist messages;
  Dllist clients;
  pthread_t tid;
  pthread_mutex_t *lock;
  pthread_cond_t *cond;
} Chat_room;

struct client {
  char name[1000];
  FILE *fin;
  FILE *fout;
  int fd;
  pthread_t tid;
  struct chat_room *room;
};
struct server *server; // global so all func have access to list of chat rooms



Chat_room *join_room(struct client *client, char *name) {
  JRB tmp;
  jrb_traverse(tmp, server->chat_rooms_jrb) {
    struct chat_room *room = (struct chat_room *)tmp->val.v;
    if (strcmp(room->name, name) == 0) {
      pthread_mutex_lock(room->lock);
      dll_append(room->clients, new_jval_v((void *)client));
      pthread_mutex_unlock(room->lock);
      return room;
    }
  }
  return NULL;
}



void send_message(struct client *client, char *m) {

  pthread_mutex_lock(client->room->lock);
  dll_append(client->room->messages, new_jval_s(strdup(m)));
  pthread_cond_signal(client->room->cond);
  pthread_mutex_unlock(client->room->lock);
}



void close_client(struct client *c) {
  close(c->fd);
  fclose(c->fin);
  fclose(c->fout);
  free(c);
}

void close_room(struct chat_room *r) {
  Dllist tmp;
  dll_traverse(tmp, r->clients) {
    free(tmp->val.s);
  }
  free_dllist(r->clients);

  dll_traverse(tmp, r->messages) {
    free(tmp->val.s);
  }
  free_dllist(r->messages);
  free(r->name);
  free(r->lock);
  free(r->cond);
  free(r);
}



void *handle_room_thread(void *arg) {
  struct chat_room *room = arg;

  while (1 == 1) {
    room->messages = new_dllist();
    pthread_mutex_lock(room->lock);
    while (dll_empty(room->messages)) {
      pthread_cond_wait(room->cond, room->lock);
    }

    Dllist tmp1;
    Dllist tmp2;

    dll_traverse(tmp1, room->messages) {
      if (tmp1 == NULL) {
        break;
      }
      char *msg_to_send = tmp1->val.s;
      dll_traverse(tmp2, room->clients) {
        struct client *client_in_room = (struct client *)tmp2->val.v;
        fprintf(client_in_room->fout, msg_to_send);
        fflush(client_in_room->fout);
      }
    }

    dll_traverse(tmp1, room->messages) {
      free(tmp1->val.s);
    }
    free_dllist(room->messages);
    pthread_mutex_unlock(room->lock);
  }
  close_room(room);
  return NULL;
}



void *handle_client_thread(void *arg) {
  struct client *client = arg;
  client->fin = fdopen(client->fd, "r");
  if (client->fin == NULL) {
    perror("fdopen fin");
    exit(1);
  }
  client->fout = fdopen(client->fd, "w");
  if (client->fout == NULL) {
    perror("fdopen fout");
    exit(1);
  }

  Dllist dll_tmp;
  JRB jrb_tmp;
  char buf[10000];

  fprintf(client->fout, "Chat Rooms:\n\n");
  jrb_traverse(jrb_tmp, server->chat_rooms_jrb) {
    struct chat_room *room = (struct chat_room *)jrb_tmp->val.v;
    sprintf(buf, "%s:", room->name);
    pthread_mutex_lock(room->lock);

    // loop through each room and list out each user in the room
    dll_traverse(dll_tmp, room->clients) {
      struct client *cli = (struct client *)dll_tmp->val.v;
      strcat(buf, " ");
      strcat(buf, cli->name);
    }
    // print list to the client
    pthread_mutex_unlock(room->lock);
    strcat(buf, "\n");
    fprintf(client->fout, buf);
  }

  fprintf(client->fout, "\nEnter your chat name (no spaces):\n");
  fflush(client->fout);
  if (fscanf(client->fin, "%s", client->name) != 1) {
    pthread_exit(NULL);
  }

  do {
    fprintf(client->fout, "Enter chat room:\n");
    fflush(client->fout);

    if (fscanf(client->fin, "%s", buf) != 1) {
      pthread_exit(NULL);
    }
    client->room = join_room(client, buf);
    if (client->room == NULL) {
      fprintf(client->fout, "No chat room %s.\n", buf);
    }
  } while (client->room == NULL);

  char line_buf[10000];
  sprintf(buf, "%s has joined\n", client->name);
  send_message(client, buf);

  while (fgets(line_buf, 10000, client->fin) != NULL) {
    if (strcmp(line_buf, "\n") == 0) {
      continue; // ignore empty input
    }
    sprintf(buf, "%s: %s", client->name, line_buf);
    send_message(client, buf);
  }
  // once the loop ends then the user quit so we need to remove them from dllist
  pthread_mutex_lock(client->room->lock);

  dll_traverse(dll_tmp, client->room->clients) {
    struct client *cli = (struct client *)dll_tmp->val.v;
    if (strcmp(cli->name, client->name) == 0) {
      dll_delete_node(dll_tmp);
      break;
    }
  }

  sprintf(buf, "%s has left\n", client->name);
  dll_append(client->room->messages, new_jval_s(strdup(buf)));

  pthread_cond_signal(client->room->cond);
  pthread_mutex_unlock(client->room->lock);
  close_client(client);
  return NULL;
}




void stop_the_server() {
  JRB tmp;
  signal(SIGINT, stop_the_server);
  jrb_traverse(tmp, server->chat_rooms_jrb) {
    struct chat_room *room = (struct chat_room *)tmp->val.v;
    pthread_detach(room->tid);
    close_room(room);
  }
  jrb_free_tree(server->chat_rooms_jrb);
  free(server);
  exit(0);
}



int main(int argc, char **argv) {
  if(argc < 4) {
    fprintf(stderr, "usage: chat_server [port#] [Chat-room-names]\n");
    exit(1);
  }
  int port = atoi(argv[2]);
if(port < 8000){
  printf("port needs to be greater than 8000 port = %d\n", port);
  exit(1);
}
  server = (struct server*)malloc(sizeof(struct server));
  server->chat_rooms_jrb = make_jrb();
  for(int i = 3; i < argc; i++){
    struct chat_room* room = (struct chat_room*)malloc(sizeof(struct chat_room));
    room->name = strdup(argv[i]);
    room->clients = new_dllist();
    room->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    room->lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(room->lock, NULL);
    pthread_cond_init(room->cond, NULL);

    jrb_insert_str(server->chat_rooms_jrb, room->name, new_jval_v((void*)room));
    if(pthread_create(&room->tid, NULL, handle_room_thread, room) != 0){
      perror("pthread_create room thread");
      exit(1);
    }
  }
  printf("On port %d\n", port);
  signal(SIGINT, stop_the_server);

  int socket = serve_socket(port);
  printf("socket: %d\n", socket);
  while(1 == 1){
    int fd = accept_connection(socket);
    struct client* client = (struct client*)malloc(sizeof(struct client));
    client->fd = fd;

    if(pthread_create(&client->tid, NULL, handle_client_thread, client) != 0){
      perror("pthread_create client");
      exit(1);
    }
  }
  stop_the_server();
  return 0;
}