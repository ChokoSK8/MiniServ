#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

#define	BUFFER_MAX_SIZE 4096

typedef struct	s_client
{
	int		fd;
	int		id;
	struct s_client*	prev;
	struct s_client*	next;
}								t_client;

int				nfds;
t_client*	client = NULL;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 2));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	newbuf[len + strlen(add) + 1] = '\n';
	newbuf[len + strlen(add) + 2] = 0;
	return (newbuf);
}

void	ft_write_stdin(char*	str)
{
	write(1, str, strlen(str));
	write(1, "\n", 1);
}

void	ft_write_stderr(char*	str)
{
	write(2, str, strlen(str));
	write(2, "\n", 1);
}

void	ft_exit(char *str, int status)
{
	ft_write_stderr(str);
	exit(status);
}

void	init_fdset(int sockfd, fd_set* readfds)
{
	t_client*	clt = client;

	FD_ZERO(readfds);
	FD_SET(sockfd, readfds);
	while (clt)
	{
		if (clt->fd > nfds)
			nfds = clt->fd;
		FD_SET(clt->fd, readfds);
		clt = clt->next;
	}
	write(1, "\n", 1);
}

size_t	client_size()
{
	size_t	size = 0;
	t_client*	clt = client;

	while (clt)
	{
		clt = clt->next;
		++size;
	}
	return (size);
}

t_client*	client_last()
{
	t_client*	last = client;

	while (last && last->next)
		last = last->next;
	return (last);
}

int		client_id_by_fd(int _fd)
{
	t_client*	clt = client;

	while (clt && clt->fd != _fd)
		clt = clt->next;
	if (!clt)
		ft_exit("client_id_by_fd failed...", 2);
	return (clt->id);
}

void	ft_select(fd_set* readfds)
{
	if (select(nfds + 1, readfds, NULL, NULL, NULL) < 0)
		ft_exit("Fatal error", 1);
}

int		add_client(int _fd)
{
	t_client*	last = client_last();
	t_client*	new = malloc(sizeof(t_client));

	if (!new)
		ft_exit("Fatal error", 1);
	new->fd = _fd;
	new->next = NULL;
	if (!last)
	{
		client = new;
		client->prev = NULL;
		client->id = 0;
	}
	else
	{
		new->prev = last;
		last->next = new;
		new->id = last->id + 1;
	}
	return (new->id);
}

void	ft_send(char *msg, int sender_fd)
{
	t_client*	clt = client;

	char c = sender_fd + 48;
	write(1, &c, 1);
	write(1, " | ", 3);
	while (clt)
	{
		
		if (clt->fd != sender_fd)
		{
			if (send(clt->fd, msg, strlen(msg), MSG_DONTWAIT) < 0)
				ft_exit("Fatal error", 1);
		}
		else
		{
			c = clt->fd + 48;
			write(1, &c, 1);
		}
		clt = clt->next;
	}
	write(1, "\n", 1);
}

void	ft_send_by_client(char* msg, int sender_fd)
{
	int				sender_id = client_id_by_fd(sender_fd);
	char*			final_msg = malloc(50);

	if (!final_msg)
		ft_exit("Fatal error", 1);
	sprintf(final_msg, "client %d: ", sender_id);
	final_msg = str_join(final_msg, msg);
	if (!final_msg)
		ft_exit("Fatal error", 1);
	ft_send(final_msg, sender_fd);
	free(final_msg);
}

void	ft_send_by_server(int config, int id)
{
	char	msg[100];

	if (config == 1)
		sprintf(msg, "server: client %d just arrived\n", id);
	else
		sprintf(msg, "server: client %d just left\n", id);
	ft_send(msg, 0);
}

void	ft_accept(int sockfd)
{
	struct sockaddr_in	cli;
	int									len = sizeof(cli);
	int									connfd;
	int									id;

	connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len);
	if (connfd < 0)
		ft_exit("server accept failed...", 1);
	id = add_client(connfd);
	ft_send_by_server(1, id);
}

void	client_remove(int _fd)
{
	t_client*	clt = client;

	while (clt && clt->fd != _fd)
		clt = clt->next;
	if (!clt)
		ft_exit("Removint client failed...", 2);
	if (client_size() != 1)
	{
		if (clt->prev)
		{
			clt->prev->next = clt->next;
			if (clt->next)
				clt->next->prev = clt->prev->next;
		}
		else
		{
			client = clt->next;
			client->prev = NULL;
		}
		clt->prev = NULL;
		clt->next = NULL;
	}
	else
	{
		client = NULL;
	}
	free(clt);
	close(_fd);
}

void	ft_recv(fd_set* readfds)
{
	t_client*	clt = client;
	int				bytes;
	char			buffer[BUFFER_MAX_SIZE];

	char c;
	memset(buffer, 0, BUFFER_MAX_SIZE);
	while (clt)
	{
		write(1, "FD_ISSET: ", 10);
		c = clt->fd + 48;
		write(1, &c, 1);
		write(1, " ; ", 3);
		if (FD_ISSET(clt->fd, readfds))
		{
			bytes = recv(clt->fd, buffer, BUFFER_MAX_SIZE - 1, MSG_DONTWAIT);
			if (!bytes)
			{
				ft_send_by_server(2, clt->id);
				client_remove(clt->fd);
			}
			else
			{
				ft_send_by_client(buffer, clt->fd);
			}
			memset(buffer, 0, BUFFER_MAX_SIZE);
		}
		clt = clt->next;
	}
	write(1, "\n", 1);
}

void	server_loop(int sockfd)
{
	fd_set	readfds;

	nfds = sockfd;
	while (1)
	{
		init_fdset(sockfd, &readfds);
		ft_select(&readfds);
		if (FD_ISSET(sockfd, &readfds))
			ft_accept(sockfd);
		ft_recv(&readfds);
	}
}

int main(int ac, char **av)
{
	int									sockfd;
	struct sockaddr_in	servaddr; 

	if (ac != 2)
		return (1);

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		ft_write_stdin("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		ft_write_stdin("Socket successfully created..\n"); 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		ft_write_stdin("socket bind failed...\n"); 
		exit(0); 
	} 
	else
		ft_write_stdin("Socket successfully binded..\n");
	if (listen(sockfd, 10) != 0) {
		ft_write_stdin("cannot listen\n"); 
		exit(0); 
	}
	server_loop(sockfd);
	close(sockfd);
	return (0);
}
