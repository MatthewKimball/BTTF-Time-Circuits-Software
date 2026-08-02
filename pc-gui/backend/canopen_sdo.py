"""
Minimal CANopen SDO client (expedited transfers only).

Every object in the Time Circuits OD (CANopen/OD.h in the firmware repo) is
4 bytes or smaller, so expedited transfer is all that's needed - no segmented
or block transfer support.

Protocol reference: CiA 301, expedited SDO download/upload.
"""

import struct
import threading
import time

import can

CO_CAN_ID_SDO_SRV = 0x580  # server -> client (+ nodeId)
CO_CAN_ID_SDO_CLI = 0x600  # client -> server (+ nodeId)

# SCS/CCS command specifiers
CCS_DOWNLOAD_INITIATE = 0x20
CCS_UPLOAD_INITIATE = 0x40
SCS_DOWNLOAD_INITIATE = 0x60
SCS_UPLOAD_INITIATE = 0x40
SCS_ABORT = 0x80


class SdoAbort(Exception):
    def __init__(self, index: int, subindex: int, abort_code: int):
        self.index = index
        self.subindex = subindex
        self.abort_code = abort_code
        super().__init__(
            f"SDO abort on {index:#06x}:{subindex:02x} - code {abort_code:#010x}"
        )


class SdoTimeout(Exception):
    pass


class CanopenSdoClient:
    """One SDO client channel talking to a single CANopen node."""

    def __init__(self, bus: can.BusABC, node_id: int, timeout: float = 1.0):
        self.bus = bus
        self.node_id = node_id
        self.timeout = timeout
        self.tx_id = CO_CAN_ID_SDO_CLI + node_id  # we send here (device's Rx)
        self.rx_id = CO_CAN_ID_SDO_SRV + node_id  # we listen here (device's Tx)
        self._lock = threading.Lock()

    def _transact(self, request: bytes) -> bytes:
        """Send one SDO request frame and wait for the matching response."""
        with self._lock:
            # Drain any stale frames sitting in the receive buffer first.
            while self.bus.recv(timeout=0) is not None:
                pass

            msg = can.Message(
                arbitration_id=self.tx_id,
                data=request,
                is_extended_id=False,
            )
            self.bus.send(msg)

            deadline = time.monotonic() + self.timeout
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise SdoTimeout(
                        f"No SDO response from node {self.node_id} within {self.timeout}s"
                    )
                reply = self.bus.recv(timeout=remaining)
                if reply is None:
                    raise SdoTimeout(
                        f"No SDO response from node {self.node_id} within {self.timeout}s"
                    )
                if reply.arbitration_id == self.rx_id:
                    return bytes(reply.data)
                # Not our SDO response (other bus traffic) - keep waiting.

    def read(self, index: int, subindex: int, size_hint: int | None = None) -> int:
        """Expedited SDO upload. Returns the value as an unsigned int."""
        request = struct.pack(
            "<BHBxxxx", CCS_UPLOAD_INITIATE, index, subindex
        )
        response = self._transact(request)
        cmd = response[0]
        r_index, r_subindex = struct.unpack_from("<HB", response, 1)

        if cmd & 0xE0 == SCS_ABORT:
            abort_code = struct.unpack_from("<I", response, 4)[0]
            raise SdoAbort(r_index, r_subindex, abort_code)

        if r_index != index or r_subindex != subindex:
            raise RuntimeError(
                f"SDO response mismatch: expected {index:#06x}:{subindex:02x}, "
                f"got {r_index:#06x}:{r_subindex:02x}"
            )

        e_bit = (cmd >> 1) & 0x01
        s_bit = cmd & 0x01
        if not e_bit:
            raise RuntimeError("Segmented SDO transfer not supported")

        if s_bit:
            n = (cmd >> 2) & 0x03
            length = 4 - n
        elif size_hint is not None:
            length = size_hint
        else:
            length = 4

        value = int.from_bytes(response[4 : 4 + length], "little", signed=False)
        return value

    def write(self, index: int, subindex: int, value: int, size: int) -> None:
        """Expedited SDO download. `size` is the object's byte width (1/2/4)."""
        if size not in (1, 2, 4):
            raise ValueError("Expedited SDO only supports 1, 2, or 4 byte objects")

        data = value.to_bytes(size, "little", signed=False)
        n = 4 - size
        cmd = CCS_DOWNLOAD_INITIATE | (n << 2) | 0x03  # e=1, s=1
        request = struct.pack("<BHB", cmd, index, subindex) + data.ljust(4, b"\x00")

        response = self._transact(request)
        cmd = response[0]
        r_index, r_subindex = struct.unpack_from("<HB", response, 1)

        if cmd & 0xE0 == SCS_ABORT:
            abort_code = struct.unpack_from("<I", response, 4)[0]
            raise SdoAbort(r_index, r_subindex, abort_code)

        if cmd != SCS_DOWNLOAD_INITIATE or r_index != index or r_subindex != subindex:
            raise RuntimeError(
                f"Unexpected SDO download response for {index:#06x}:{subindex:02x}: "
                f"{response.hex()}"
            )
