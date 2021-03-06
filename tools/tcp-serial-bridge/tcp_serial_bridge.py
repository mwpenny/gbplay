#!/usr/bin/python3
import argparse
import threading
from gb_tcp import GBSerialTCPServer, GBSerialTCPClient, BGBProxyTCPClient

arg_parser = argparse.ArgumentParser(description='Links 2 Game Boys via a serial <-> TCP bridge.')
arg_subparsers = arg_parser.add_subparsers(dest='mode', required=True, help='Operation modes')

server_parser = arg_subparsers.add_parser('server', help='Run TCP server')
server_parser.add_argument('--protocol', required=True, type=str, help='name of game protocol (see game_protocols folder)')
server_parser.add_argument('--trace', default=False, action='store_true', help='enable communication logging')
server_parser.add_argument('--host', type=str, help='host to listen on')
server_parser.add_argument('--port', type=int, help='port to listen on')

client_parser = arg_subparsers.add_parser('client', help='Run TCP client using a Game Boy over serial')
client_parser.add_argument('--server-host', type=str, help='server host to connect to')
client_parser.add_argument('--server-port', type=int, help='server port to connect to')
client_parser.add_argument('serial_port', type=str, help='serial port of the Game Boy')

client_parser = arg_subparsers.add_parser('bgb-proxy', help='Run proxy server which forwards BGB link cable data over TCP')
client_parser.add_argument('--server-host', type=str, help='server host to connect to')
client_parser.add_argument('--server-port', type=int, help='server port to connect to')
client_parser.add_argument('--listen-port', type=int, default=8765, help='port for BGB proxy server to listen on')

local_parser = arg_subparsers.add_parser('local', help='Run TCP server and a client for each Game Boy')
local_parser.add_argument('--protocol', required=True, type=str, help='name of game protocol (see game_protocols folder)')
local_parser.add_argument('--trace', default=False, action='store_true', help='enable communication logging')
local_parser.add_argument('gb1_port', type=str, help='serial port of the first Game Boy')
local_parser.add_argument('gb2_port', type=str, help='serial port of the second Game Boy')

args = arg_parser.parse_args()

if args.mode == 'server':
    kwargs = { 'protocol': args.protocol, 'trace': args.trace } | {
        k: getattr(args, k) for k in ['host', 'port']
        if getattr(args, k) is not None
    }
    GBSerialTCPServer(**kwargs).run()
elif args.mode == 'client':
    kwargs = { 'serial_port': args.serial_port } | {
        k: getattr(args, k) for k in ['server_host', 'server_port']
        if getattr(args, k) is not None
    }
    GBSerialTCPClient(**kwargs).connect()
elif args.mode == 'bgb-proxy':
    kwargs = {
        k: getattr(args, k) for k in ['server_host', 'server_port', 'listen_port']
        if getattr(args, k) is not None
    }
    BGBProxyTCPClient(**kwargs).connect()
elif args.mode == 'local':
    server = GBSerialTCPServer(args.protocol, trace=args.trace)
    gb1_client = GBSerialTCPClient(args.gb1_port)
    gb2_client = GBSerialTCPClient(args.gb2_port)

    threading.Thread(target=lambda: server.run()).start()
    threading.Thread(target=lambda: gb1_client.connect()).start()
    threading.Thread(target=lambda: gb2_client.connect()).start()
else:
    raise Exception(f'Unknown mode:', args.connection_type)