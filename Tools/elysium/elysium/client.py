import time
import enet
from .generated import (
    PacketType, InvokeRequest, InvokeResponse,
    PACKET_HEADER_SIZE, INVOKE_HEADER_SIZE
)


class ElysiumClient:
    """
    ENet client connection to an Elysium server.
    Can be used standalone or as a context manager:

        with ElysiumClient("127.0.0.1", 7777) as client:
            response = client.invoke(1, ProcedureId.PING, PingRequest(123).to_bytes())
    """

    def __init__(self, host: str = "127.0.0.1", port: int = 7777, 
                 channels: int = 2):
        self.host    = host
        self.port    = port
        self.channels = channels
        self._host   = None
        self._peer   = None

    def connect(self, timeout_ms: int = 3000):
        self._host = enet.Host(None, 1, self.channels, 0, 0)
        address    = enet.Address(self.host.encode(), self.port)
        self._peer = self._host.connect(address, self.channels, 0)

        deadline = time.time() + timeout_ms / 1000
        while time.time() < deadline:
            event = self._host.service(50)
            if event and event.type == enet.EVENT_TYPE_CONNECT:
                return
        raise TimeoutError(f"Could not connect to {self.host}:{self.port}")

    def disconnect(self):
        if self._peer:
            self._peer.disconnect()
        if self._host:
            self._host.service(100)
            self._host.flush()
        self._peer = None
        self._host = None

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *_):
        self.disconnect()

    def send_reliable(self, data: bytes, channel: int = 0):
        packet = enet.Packet(data, enet.PACKET_FLAG_RELIABLE)
        self._peer.send(channel, packet)
        self._host.flush()

    def send_unreliable(self, data: bytes, channel: int = 1):
        packet = enet.Packet(data, 0)
        self._peer.send(channel, packet)
        self._host.flush()

    def receive(self, timeout_ms: int = 1000) -> bytes | None:
        deadline = time.time() + timeout_ms / 1000
        while time.time() < deadline:
            event = self._host.service(50)
            if event and event.type == enet.EVENT_TYPE_RECEIVE:
                return bytes(event.packet.data)
        return None

    def invoke(self, invoke_id: int, procedure_id: int,
               payload: bytes, tick: int = 0,
               timeout_ms: int = 1000) -> InvokeResponse | None:
        """Send an InvokeRequest and wait for the matching InvokeResponse."""
        import time
        request = InvokeRequest(invoke_id, procedure_id, payload, tick)
        self.send_reliable(request.to_bytes())
        deadline = time.time() + timeout_ms / 1000
        while time.time() < deadline:
            remaining = max(50, int((deadline - time.time()) * 1000))
            raw = self.receive(timeout_ms=remaining)
            if raw is None:
                return None
            if len(raw) < PACKET_HEADER_SIZE + INVOKE_HEADER_SIZE:
                continue
            if raw[0] != PacketType.INVOKE_RESPONSE:
                continue
            response = InvokeResponse.from_bytes(raw)
            if response.invoke_header.invoke_id == invoke_id:
                return response
        return None