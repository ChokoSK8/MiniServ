#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <stdio.h>

#define MAX_BUFFER_SIZE 4096

typedef struct	s_client
{
	int				fd;
	int				id;
	struct s_client*	next;
	struct s_client*	prev;
}								t_client;

int				max_fd = 0;
t_client*	client = NULL;


void	removeClient(int _fd)
{
	t_client*	next = client;

	while (!next && next->fd != _fd)
		next = next->next;
	if (next)
	{
		close(_fd);
		next->prev->next = next->next;
		next->prev = NULL;
		next->next = NULL;
		free(next);
	}
	else
		printf("Can't find %d", _fd);
}

t_client*	get_last_client(void)
{
	t_client*	last = client;

	if (!last)
		return (NULL);
	while (last->next)
		last = last->next;
	return (last);
}

void	addClient(int _fd)
{
	t_client*	new_client = malloc(sizeof(t_client*));
	t_client*	last_client = get_last_client();

	new_client->fd = _fd;
	new_client->next = NULL;
	if (last_client)
	{
		new_client->id = last_client->id + 1;
		last_client->next = new_client;
		new_client->prev = last_client;
	}
	else
	{
		new_client->id = 0;
		new_client->prev = NULL;
		new_client->next = NULL;
		client = new_client;
	}
	write(0, "new client\n", 12);
}

int		get_id_by_fd(int _fd)
{
	t_client*	next = client;

	if (!next)
		return (0);
	while (next && next->fd != _fd)
		next = next->next;
	if (!next)
		printf("client %d not found", _fd);
	return (next->id);
}

void	send_to_clients(int sender_fd, char* msg, int config)
{
	t_client*	next = client;
	char			from_who[100];
	size_t		msg_len = strlen(msg);
	size_t		from_who_len;
	int				sender_id = get_id_by_fd(sender_fd);

	sprintf(from_who, "client %d: ", sender_id);
	from_who_len = strlen(from_who);
	while (next)
	{
		if (next->fd != sender_fd)
		{
			if (config)
				send(next->fd, from_who, from_who_len, MSG_DONTWAIT);
			send(next->fd, msg, msg_len, MSG_DONTWAIT);
		}
		next = next->next;
	}
}

void	acceptClient(int sockfd, struct sockaddr_in servaddr, int len)
{
	char		buffer[100];

	int fd = accept(sockfd, (struct sockaddr *)&servaddr, (socklen_t*)&len);
	if (fd < 0)
	{
		sprintf(buffer, "fd %d failed\n", fd);
		write(1, buffer, strlen(buffer));
		exit(0);
	}
	write(fd, "hello\n", 6);
	sprintf(buffer, "server: client %d just arrived\n", fd);
	send_to_clients(fd, buffer, 0);
	addClient(fd);
	max_fd = fd;
}

void	build_fdsets(int sockfd, fd_set* readfds)
{
	t_client*	next = client;

	FD_ZERO(readfds);
	FD_SET(sockfd, readfds);
	while (next)
	{
		FD_SET(next->fd, readfds);
		next = next->next;
	}
}

void	recv_from_server(int fd, char* recv_msg)
{
	int		read_bytes;
	int		id = get_id_by_fd(fd);

	read_bytes = recv(fd, recv_msg, MAX_BUFFER_SIZE - 1, 0);
	if (read_bytes > 0)
		send_to_clients(fd, recv_msg, 1);
	else if (read_bytes == 0)
	{
		printf("client %d disconnect", fd);
		removeClient(fd);
	}
}

void	ft_select(int max_fd, int fd, fd_set* readfds)
{
	if (select(max_fd + 1, readfds, NULL, NULL, NULL) < 0)
	{
		printf("select fct failed");
		exit(0);
	}
}

void	receiveMsg(fd_set* readfds)
{
	t_client*	next = client;
	char	recv_msg[MAX_BUFFER_SIZE];

	while (next)
	{
		if (FD_ISSET(next->fd, readfds))
		{
			memset(recv_msg, 0, sizeof(recv_msg));
			recv_from_server(next->fd, recv_msg);
		}
		next = next->next;
	}
}

void	serverLoop(int sockfd, struct sockaddr_in servaddr)
{
	int	len = sizeof(struct sockaddr_in);
	fd_set	readfds;
	fd_set	writefds;

	max_fd = sockfd;
	while (1)
	{
		build_fdsets(sockfd, &readfds);
		ft_select(max_fd, sockfd, &readfds);
		if (FD_ISSET(sockfd, &readfds))
			acceptClient(sockfd, servaddr, len);
		if (client)
			printf("client connected");
		receiveMsg(&readfds);
	}
}

void	createSocket(int* sockfd)
{
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sockfd == -1)
	{
		printf("Failed to create socket");
		exit (0);
	}
}

void	initSockaddr(struct sockaddr_in* servaddr, char* port)
{
	servaddr->sin_family = AF_INET;
	servaddr->sin_addr.s_addr = htonl(2130706433);
	servaddr->sin_port = htons(atoi(port));
}

void	bindSocket(int* sockfd, struct sockaddr_in* servaddr)
{
	if ((bind(*sockfd, (const struct sockaddr *)servaddr, sizeof(*servaddr))) != 0)
	{
		printf("socket bind failed");
		exit(0);
	}
}

void	listenSocket(int* sockfd)
{
	if (listen(*sockfd, 10) < 0)
	{
		printf("cannot listen");
		exit (0);
	}
}

int		main(int ac, char **av)
{
	if (ac != 2)
	{
		printf("Wrong number of arguments");
		exit (0);
	}

	int									sockfd;
	struct sockaddr_in	servaddr;

	createSocket(&sockfd);
	initSockaddr(&servaddr, av[1]);
	bindSocket(&sockfd, &servaddr);
	listenSocket(&sockfd);

	serverLoop(sockfd, servaddr);
	close(sockfd);
	return (0);
}
