#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uuid/uuid.h>

#include "handlers.h"
#include "net/server.h"
#include "net/mcprotocol.h"

extern HashTable* clientsSessions;

void handleHandshakingState(ServerCtx *ctx, ConnectionCtx* connectionContext, MCPacket *packet)
{
    switch (packet->id)
    {
    case HANDSHAKE:
    {
        in_HandshakePacket handshakePacket;
        readHandshakingPacket(packet, &handshakePacket);

        printf("Handshake packet received. Next state: %d\n", handshakePacket.nextState);
        SessionCtx *session = hashtableGetElement(clientsSessions, connectionContext->fd);

        session->status = handshakePacket.nextState;

        free(handshakePacket.serverAddress);
    }break;
    
    default:
        break;
    }
}

void handleStatusState(ServerCtx *ctx, ConnectionCtx* connectionContext, MCPacket *packet)
{
    switch(packet->id)
    {
    case STATUS_REQUEST:
    {
        printf("Status request packet received.\n");

        mc_string jsonTest = "{\"version\": {\"name\": \"1.14.4\",\"protocol\": 498},\"players\": {\"max\": 100,\"online\": 5,\"sample\": [{\"name\": \"thinkofdeath\",\"id\": \"4566e69f-c907-48ee-8d71-d7ba5aa00d20\"}]},\"description\": {\"text\": \"Hello world\"},\"favicon\": \"data:image/png;base64,<data>\"}";

        out_ResponseStatusPacket responsePacket;
        responsePacket.jsonResponse = jsonTest;

        MCPacket packet;
        mcPacketCreate(&packet);

        writeStatusResponsePacket(&packet, &responsePacket);
        mcPacketWrite(connectionContext->response, &packet);

        mcPacketDestroy(&packet);
    }break;

    case STATUS_PING:
    {
        in_PingStatusPacket pingPacket;
        readPingPacket(packet, &pingPacket);

        printf("Ping request received with payload %ld.\n", pingPacket.payload);

        out_PongStatusPacket pongPacket;
        pongPacket.payload = pingPacket.payload;

        MCPacket packet;
        mcPacketCreate(&packet);

        writePongPacket(&packet, &pongPacket);
        mcPacketWrite(connectionContext->response, &packet);
        mcPacketDestroy(&packet);
    }break;
    }
}

void handleLoginState(ServerCtx *ctx, ConnectionCtx* connectionContext, MCPacket *packet)
{
    switch(packet->id)
    {
    case LOGIN_START:
    {
        in_LoginStartPacket loginStartPacket;
        readLoginStart(packet, &loginStartPacket);

        SessionCtx *session = hashtableGetElement(clientsSessions, connectionContext->fd);

        createPlayer(&session->player, loginStartPacket.name);

        printf("New user wants to log in with username %s\n", loginStartPacket.name);

        out_LoginSuccessPacket loginSuccessPacket;
        loginSuccessPacket.username = session->player.nickname;
        uuid_unparse(session->player.uuid, loginSuccessPacket.uuid);

        MCPacket outPacket;
        mcPacketCreate(&outPacket);

        writeLoginSuccess(&outPacket, &loginSuccessPacket);
        mcPacketWrite(connectionContext->response, &outPacket);

        free(loginStartPacket.name);
        mcPacketDestroy(&outPacket);
    }
    }
}

void newDataHandler(ServerCtx *ctx, ConnectionCtx* connectionContext)
{
    int readedBytes;

    MCPacket packet;
    mcPacketCreate(&packet);

    readedBytes = mcPacketRead(connectionContext->data, &packet);
    if(readedBytes == -1)
    {
        printf("Invalid packet\n");
        return;
    }

    SessionCtx *session = hashtableGetElement(clientsSessions, connectionContext->fd);

    switch (session->status)
    {
    case HANDSHAKING:
        handleHandshakingState(ctx, connectionContext, &packet);
        break;

    case STATUS:
        handleStatusState(ctx, connectionContext, &packet);
        break;

    case LOGIN:
        handleLoginState(ctx, connectionContext, &packet);
        break;
    
    default:
        break;
    }

    mcPacketDestroy(&packet);
}

void newConnectionHandler(ServerCtx *ctx, ConnectionCtx* connectionContext)
{
    SessionCtx session;
    session.status = HANDSHAKING;

    hashtableSetElement(clientsSessions, connectionContext->fd, &session);

    printf("New connection!\n");
}

void disconnectionHandler(ServerCtx *ctx, ConnectionCtx* connectionContext)
{
    printf("Client disconnected\n");
}