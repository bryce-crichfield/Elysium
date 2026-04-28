import struct
import unittest
import enet

# network_tests.py
from elysium import (
    ElysiumClient, ProcedureId, PacketType,
    PingRequest, PingResponse, InvokeRequest,
    SpawnPlayerRequest, SpawnPlayerResponse,
)

class ElysiumProtocolTest(unittest.TestCase):
    def setUp(self):
        self.client = ElysiumClient("127.0.0.1", 7777)
        self.client.connect()

    def tearDown(self):
        self.client.disconnect()

    def test_ping_round_trip(self):
        """Server should echo clientTick back in the response."""
        client_tick = 12345
        response = self.client.invoke(1, ProcedureId.PING, 
                                      PingRequest(client_tick).to_bytes())

        self.assertIsNotNone(response, "No response received")
        self.assertEqual(response.packet_header.packet_type, PacketType.INVOKE_RESPONSE)
        self.assertEqual(response.invoke_header.invoke_id, 1)
        self.assertEqual(response.invoke_header.procedure_id, ProcedureId.PING)

        ping_resp = PingResponse.from_bytes(response.payload)
        self.assertEqual(ping_resp.echo_client_tick, client_tick)

    def test_truncated_packet_does_not_crash(self):
        """Server should silently reject packets shorter than PacketHeader."""
        self.client.send_reliable(bytes([0x40, 0x00]))
        self.assertIsNone(self.client.receive(timeout_ms=500))

    def test_unknown_procedure_id_does_not_crash(self):
        """Unknown procedure IDs should be rejected cleanly."""
        response = self.client.invoke(2, 0xDEAD, struct.pack('<I', 0))
        self.assertIsNone(response)

    def test_invoke_id_correlation(self):
        """Response invokeId must match request invokeId exactly."""
        for invoke_id in [1, 42, 0xFFFF]:
            with self.subTest(invoke_id=invoke_id):
                response = self.client.invoke(
                    invoke_id, ProcedureId.PING, PingRequest(0).to_bytes())
                self.assertIsNotNone(response)
                self.assertEqual(response.invoke_header.invoke_id, invoke_id)

    def test_wrong_channel_is_ignored(self):
        """Invoke packets on the unreliable channel should be ignored."""
        request = self.client.invoke.__func__  # get the raw packet builder
        from elysium import InvokeRequest
        raw = InvokeRequest(1, ProcedureId.PING, PingRequest(0).to_bytes()).to_bytes()
        self.client.send_reliable(raw, channel=1)  # force channel 1
        self.assertIsNone(self.client.receive(timeout_ms=500))

    def test_spawn_player_noops_on_junk(self):
        """Sending a spawn player request with invalid payload should not crash."""
        response = self.client.invoke(3, ProcedureId.SPAWNPLAYER, b'not a valid spawn payload')
        self.assertIsNone(response)

    def test_spawn_player_returns_success(self):
        """Server should respond to a valid SpawnPlayer request with success=True."""
        req = SpawnPlayerRequest(scene_id=1, player_name="TestPlayer")
        response = self.client.invoke(4, ProcedureId.SPAWNPLAYER, req.to_bytes())
        self.assertIsNotNone(response, "No response received for SpawnPlayer")
        self.assertEqual(response.invoke_header.procedure_id, ProcedureId.SPAWNPLAYER)
        self.assertEqual(response.invoke_header.invoke_id, 4)
        resp = SpawnPlayerResponse.from_bytes(response.payload)
        self.assertTrue(resp.success)

if __name__ == '__main__':
    unittest.main()