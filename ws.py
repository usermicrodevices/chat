#!/usr/bin/env python3

import asyncio
import sys
import argparse
import ssl
import json
import websockets

parser = argparse.ArgumentParser(description="Send a chat message over WebSocket and print all responses.")
parser.add_argument("url", help="ws:// or wss:// URL (e.g., ws://localhost:8080)")
parser.add_argument("--sender", default="test_user", help="Sender name (default: test_user)")
parser.add_argument("--room", default="general", help="Room name (default: general)")
parser.add_argument("--content", default="Hello from Python", help="Message content")
parser.add_argument("--once", action="store_true", help="Send one message and then exit after receiving the first response")
parser.add_argument("--cert-verify", action="store_true", help="Verify TLS certs for wss (default: verify)")
args = parser.parse_args()

message = json.dumps({
    "sender": args.sender,
    "content": args.content,
    "room": args.room
})

async def main():
    ssl_ctx = None
    if args.url.startswith("wss://"):
        ssl_ctx = ssl.create_default_context()
        if not args.cert_verify:
            ssl_ctx.check_hostname = False
            ssl_ctx.verify_mode = ssl.CERT_NONE
    try:
        async with websockets.connect(args.url, ssl=ssl_ctx) as ws:
            await ws.send(message)
            print(f"Sent: {message}")
            if args.once:
                response = await ws.recv()
                print(f"Recv: {response}")
            else:
                try:
                    async for msg in ws:
                        try:
                            print(f"Recv: {msg}")
                        except KeyboardInterrupt as e:
                            sys.exit('KeyboardInterrupt\n')
                        except Exception as e:
                            print(f"Error: {e}", file=sys.stderr)
                            sys.exit(1)
                except KeyboardInterrupt as e:
                    sys.exit('KeyboardInterrupt\n')
                except asyncio.CancelledError as e:
                    print(f"Error: {e}", file=sys.stderr)
                    return
                except Exception as e:
                    print(f"Error: {e}", file=sys.stderr)
                    sys.exit(1)
    except KeyboardInterrupt as e:
        sys.exit('KeyboardInterrupt\n')
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    print("\nPress Ctrl+C after every waiting operation", file=sys.stdout)
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user", file=sys.stderr)
        sys.exit(0)
