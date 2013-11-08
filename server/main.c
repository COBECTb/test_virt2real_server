#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 4444
#define MAXNOISE_SEC 1
#define BUFFER_SIZE 32
#define DEVICE "/dev/v2r_pins"
//#define DEVICE "/dev/null"

const char *init1 = "set con44 pwm1";
const char *init2 = "set con43 pwm0";
const char *init3 = "set con42 pwm3";

const char *stop1 = "set con30 output:0";
const char *stop2 = "set con31 output:0";
const char *stop3 = "set con32 output:0";
const char *stop4 = "set con33 output:0";

int main(int argc, const char * argv[])
{
	pid_t proc_id = 0;
	unsigned char buffer[BUFFER_SIZE];
	char command_buffer[300];
	struct sockaddr_in server_addr, client_addr;
	int socket_fd = -1;

	FILE *file;
	file = fopen(DEVICE, "r+");

	if(file)
	{
		fwrite(init1, 14, 1, file);
		fflush(file);
		fwrite(init2, 14, 1, file);
		fflush(file);
		fwrite(init3, 14, 1, file);
		fflush(file);

		fwrite(stop1, 18, 1, file);
		fflush(file);
		fwrite(stop2, 18, 1, file);
		fflush(file);
		fwrite(stop3, 18, 1, file);
		fflush(file);
		fwrite(stop4, 18, 1, file);
		fflush(file);

		socklen_t client_addr_size = sizeof(client_addr);

		while(1)
		{
			if(proc_id > 0)
			{
				system("killall -2 gst-launch-0.10");
				kill(proc_id, SIGTERM);
				waitpid(proc_id, NULL, 0);

				proc_id = 0;
			}

			if(socket_fd >= 0) close(socket_fd);
			socket_fd= socket(PF_INET, SOCK_STREAM, 0);

			if(socket_fd >= 0)
			{
				memset(&server_addr, 0, sizeof(struct sockaddr_in));
				server_addr.sin_family = AF_INET;
				server_addr.sin_addr.s_addr = 0;
				server_addr.sin_port = htons(PORT);

				const int value = 1;
				setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

				if(bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) >= 0)
				{
					if(listen(socket_fd, 1) >= 0)
					{
						fwrite(stop1, 18, 1, file);
						fflush(file);
						fwrite(stop2, 18, 1, file);
						fflush(file);
						fwrite(stop3, 18, 1, file);
						fflush(file);
						fwrite(stop4, 18, 1, file);
						fflush(file);

						fd_set	read_fds;
						fd_set	error_fds;
						int err;

						FD_ZERO(&read_fds);
						FD_SET(socket_fd, &read_fds);
						FD_ZERO(&error_fds);
						FD_SET(socket_fd, &error_fds);

						err = select(socket_fd+1, &read_fds, NULL, &error_fds, NULL);
						printf("Client connected!\n");

						if(err > 0)
						{
							if(FD_ISSET(socket_fd, &error_fds))
							{
								printf("Error selecting for connect\n");
							}
							else if(FD_ISSET(socket_fd, &read_fds))
							{

								int client_handle = accept(socket_fd, (struct sockaddr *)&client_addr, &client_addr_size);
								if(client_handle >= 0) // Есть присоединение
								{
									char * client_ip = inet_ntoa(client_addr.sin_addr);
									printf("Client '%s' accepted!\n", client_ip);
									int connected = 1;
									close(socket_fd);

									setsockopt(client_handle, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

									// Запуск видео
									proc_id = fork();

									if(!proc_id)
									{
										close(client_handle);
										fclose(file);

										execl("./h264.sh", "h264.sh", client_ip, NULL);

									}
									else if(proc_id < 0)
									{
										printf("error create fork\n");
									}
									else if(proc_id > 0)
									{
										printf("creating proc_id = %d\n", proc_id);

										while(connected)
										{
											struct timeval timeout;
											timeout.tv_sec = MAXNOISE_SEC;
											timeout.tv_usec = 0;

											FD_ZERO(&read_fds);
											FD_SET(client_handle, &read_fds);
											FD_ZERO(&error_fds);
											FD_SET(client_handle, &error_fds);

											err = select(client_handle+1, &read_fds, NULL, &error_fds, NULL);
	//										err = select(client_handle+1, &read_fds, NULL, &error_fds, &timeout);
											if(err > 0)
											{
												if(FD_ISSET(client_handle, &error_fds))
												{
													printf("Client disconnected!\n");
													close(client_handle);
													connected = 0;
												}
												else if(FD_ISSET(client_handle, &read_fds))
												{
													ssize_t count = recv(client_handle, &buffer, BUFFER_SIZE, 0);
													if(count == -1)
													{
														printf("Client disconnected!\n");
														close(client_handle);
														connected = 0;
													}
													else if(!count)
													{
														printf("Client disconnected!\n");
														close(client_handle);
														connected = 0;
													}
													else
													{
														unsigned int cmd;
														unsigned long commands_count = count/4;
														for(cmd = 0; cmd != commands_count; ++cmd)
														{
															unsigned int type = buffer[4*cmd];
															unsigned int number = buffer[4*cmd +1];
															unsigned int value = buffer[4*cmd + 2] + 256*buffer[4*cmd + 3];
															unsigned int cmd_len;

															if(0 == type) // Вращение камеры
															{
	//															printf("[%d/%lu]: 'set pwm%d duty:%d period:476190'", cmd, commands_count, number, value);
																cmd_len = sprintf(command_buffer, "set pwm%d duty:%d period:476190", number, value);
	//															printf(" len = %d\n",cmd_len);
															}
															else if(1 == type) // Колёса
															{
	//															printf("[%d/%lu]: 'set con%d output:%d'", cmd, commands_count, number, value);
																cmd_len = sprintf(command_buffer, "set con%d output:%d", number, value);
	//															printf(" len = %d\n",cmd_len);
															}
															else // Возможность расширения протокола
															{
																continue;
															}

															fwrite(command_buffer, cmd_len, 1, file);
															fflush(file);
														}
													}
												}
											}
											else if(!err)
											{
												// Тут если сработал таймер
												printf("сработал таймер\n");
											}
											else
											{
												printf("Client disconnected!\n");
												close(client_handle);
												connected = 0;
											}
										} // while(connected)
									}
								}
							}
						}
						else
						{
							printf("Error select()\n");
						}
					}
					else
					{
						printf("Error listen socket\n");
					}
				}
				else
				{
					printf("Error bind socket\n");
				}
			}
			else
			{
				printf("Error create socket\n");
			}
		} // while
	}
	else
	{
		printf("Error open file\n");
	}
}

