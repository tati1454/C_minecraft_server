#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "utils/buffer.h"
#include "utils/hashtable.h"

void eventloopEntry(ServerCtx *ctx);
void addNewSocket(ServerCtx *ctx, int socket);
void removeSocket(ServerCtx *ctx, int socket);
ConnectionCtx *allocateConnectionCtx();

void acceptIncomingConnectionHandler(ServerCtx *ctx);
void handleNewDataEvent(ServerCtx *ctx, int fd, int availableBytes);
void handleDisconnectionEvent(ServerCtx *ctx, int fd);

ServerCtx *createServerContext()
{
    ServerCtx *ctx = malloc(sizeof(ServerCtx));
    memset(ctx, 0, sizeof(ServerCtx));

    ctx->pollfdSet = malloc(0);
    ctx->connectionCtxs = hashtableCreate(10, sizeof(ConnectionCtx));

    return ctx;
}

void setNewConnectionHandler(ServerCtx *ctx, void (*handler)(ServerCtx *, ConnectionCtx *))
{
    ctx->handleNewConnectionEvent = handler;
}

void setInputDataHandler(ServerCtx *ctx, void (*handler)(ServerCtx *, ConnectionCtx *))
{
    ctx->handleInputDataEvent = handler;
}

void setDisconnectHandler(ServerCtx *ctx, void (*handler)(ServerCtx *, ConnectionCtx *))
{
    ctx->handleDisconnectEvent = handler;
}

int bindServer(ServerCtx *ctx, short port, char *addr)
{
    ctx->fd = socket(AF_INET, SOCK_STREAM, 0);

    addNewSocket(ctx, ctx->fd);

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(addr);

    int err = bind(ctx->fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    return err;
}

int startListening(ServerCtx *ctx, int backlog)
{
    int err = listen(ctx->fd, backlog);
    return err;
}

int sendData(int socket, char *buffer, int size)
{
    int err = send(socket, buffer, size, 0);
    return err;
}

int startEventLoop(ServerCtx *ctx)
{
    int err = pthread_create(&ctx->eventloopthread, NULL, (void(*)) & eventloopEntry, ctx);
    return err;
}

void stopServer(ServerCtx *ctx)
{
    pthread_cancel(ctx->eventloopthread);

    for (int i = 0; i < ctx->pollfdSetCount; ++i)
    {
        shutdown(ctx->pollfdSet[i].fd, SHUT_RDWR);
        close(ctx->pollfdSet[i].fd);
    }

    hashtableDestroy(ctx->connectionCtxs);
    free(ctx->pollfdSet);
    free(ctx);
}

void eventloopEntry(ServerCtx *ctx)
{
    for (;;)
    {
        int events = poll(ctx->pollfdSet, ctx->pollfdSetCount, -1);

        for (int i = 0; (i < ctx->pollfdSetCount && events > 0); ++i)
        {
            struct pollfd *currentPollfd = &ctx->pollfdSet[i];

            if ((currentPollfd->revents == POLLIN) && currentPollfd->fd == ctx->fd)
            {
                currentPollfd->revents = 0;
                acceptIncomingConnectionHandler(ctx);
            }
            else if ((currentPollfd->revents & POLLIN) > 0)
            {
                int availableBytes;
                ioctl(currentPollfd->fd, FIONREAD, &availableBytes);

                if (availableBytes == 0)
                {
                    handleDisconnectionEvent(ctx, currentPollfd->fd);
                    events--;
                    continue;
                }
                currentPollfd->revents = 0;
                handleNewDataEvent(ctx, currentPollfd->fd, availableBytes);
            }
            else
            {
                continue;
            }

            events--;
        }
    }
}

void acceptIncomingConnectionHandler(ServerCtx *ctx)
{
    int clientSocket = accept(ctx->fd, NULL, NULL);
    addNewSocket(ctx, clientSocket);

    ConnectionCtx connectionContext;
    memset(&connectionContext, 0, sizeof(connectionContext));

    connectionContext.fd = clientSocket;
    connectionContext.data = createBuffer();
    connectionContext.response = createBuffer();

    hashtableSetElement(ctx->connectionCtxs, clientSocket, &connectionContext);

    ConnectionCtx *connectionctx = hashtableGetElement(ctx->connectionCtxs, clientSocket);
    ctx->handleNewConnectionEvent(ctx, connectionctx);
}

void handleNewDataEvent(ServerCtx *ctx, int fd, int availableBytes)
{
    ConnectionCtx *connectionContext = hashtableGetElement(ctx->connectionCtxs, fd);
    writeBufferFromFd(connectionContext->data, fd, availableBytes);

    for (;;)
    {
        ctx->handleInputDataEvent(ctx, connectionContext);

        if (connectionContext->response->size != 0)
            readAndSaveInFd(connectionContext->response, connectionContext->fd, connectionContext->response->size);

        if (connectionContext->data->size == 0) break;
    }
}

void handleDisconnectionEvent(ServerCtx *ctx, int fd)
{
    ConnectionCtx *connectionContext = hashtableGetElement(ctx->connectionCtxs, fd);

    ctx->handleDisconnectEvent(ctx, connectionContext);

    releaseBuffer(connectionContext->data);
    releaseBuffer(connectionContext->response);

    removeSocket(ctx, fd);
    hashtableDeleteElement(ctx->connectionCtxs, fd);
}

void addNewSocket(ServerCtx *ctx, int socket)
{
    ctx->pollfdSetCount++;
    ctx->pollfdSet = realloc(ctx->pollfdSet, ctx->pollfdSetCount * sizeof(struct pollfd));

    struct pollfd socketPollFd;
    memset(&socketPollFd, 0, sizeof(socketPollFd));

    socketPollFd.fd = socket;
    socketPollFd.events = POLLIN;

    ctx->pollfdSet[ctx->pollfdSetCount - 1] = socketPollFd;
}

void removeSocket(ServerCtx *ctx, int socket)
{
    for (int i = 0; i < ctx->pollfdSetCount; ++i)
    {
        if (ctx->pollfdSet[i].fd != socket)
            continue;

        ctx->pollfdSetCount--;
        ctx->pollfdSet = realloc(ctx->pollfdSet, ctx->pollfdSetCount * sizeof(struct pollfd));

        struct pollfd tempPollFdSet[ctx->pollfdSetCount];
        memset(tempPollFdSet, 0, ctx->pollfdSetCount * 8);

        memcpy(tempPollFdSet, ctx->pollfdSet, i * sizeof(struct pollfd));
        memcpy(&tempPollFdSet[i], &ctx->pollfdSet[i + 1], (ctx->pollfdSetCount - i) * sizeof(struct pollfd));

        memset(ctx->pollfdSet, 0, ctx->pollfdSetCount * 8);
        memcpy(ctx->pollfdSet, tempPollFdSet, ctx->pollfdSetCount * 8);

        return;
    }
}
