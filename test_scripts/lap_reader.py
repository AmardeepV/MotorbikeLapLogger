import struct


METADATA_MAGIC = b"\xAB\xCD"
TELEMETRY_MAGIC = b"\xAA\x55"

METADATA_SIZE = 16
TELEMETRY_SIZE = 50


def decode_metadata(record):

    record_type = record[2]
    version = record[3]

    timestamp = struct.unpack("<I", record[4:8])[0]
    session = struct.unpack("<H", record[8:10])[0]
    lap = struct.unpack("<H", record[10:12])[0]

    return {
        "type": record_type,
        "version": version,
        "timestamp": timestamp,
        "session": session,
        "lap": lap,
    }


def decode_telemetry(packet):

    timestamp = struct.unpack("<I", packet[4:8])[0]

    return {
        "timestamp": timestamp,
    }


def metadata_type_name(record_type):

    if record_type == 0x01:
        return "SessionStart"

    if record_type == 0x02:
        return "LapStart"

    if record_type == 0x03:
        return "SessionEnd"

    return "Unknown"


# --------------------------------
# Session information
# --------------------------------

session_number = None
session_start_timestamp = None
session_end_timestamp = None

current_lap = None

lap_data = {}

total_telemetry_packets = 0


# --------------------------------
# Read the binary file
# --------------------------------

with open("SESSION001.BIN", "rb") as file:

    position = 0

    while True:

        header = file.read(2)

        if not header:
            break

        # -------------------------
        # Metadata record
        # -------------------------

        if header == METADATA_MAGIC:

            rest = file.read(METADATA_SIZE - 2)

            if len(rest) != METADATA_SIZE - 2:
                print("ERROR: incomplete metadata record")
                break

            record = header + rest

            metadata = decode_metadata(record)

            record_type = metadata["type"]

            # ---------------------
            # Session start
            # ---------------------

            if record_type == 0x01:

                session_number = metadata["session"]
                session_start_timestamp = metadata["timestamp"]
                current_lap = metadata["lap"]

                lap_data[current_lap] = {
                    "start_timestamp": metadata["timestamp"],
                    "telemetry_packets": 0
                }

            # ---------------------
            # Lap start
            # ---------------------

            elif record_type == 0x02:

                current_lap = metadata["lap"]

                lap_data[current_lap] = {
                    "start_timestamp": metadata["timestamp"],
                    "telemetry_packets": 0
                }

            # ---------------------
            # Session end
            # ---------------------

            elif record_type == 0x03:

                session_end_timestamp = metadata["timestamp"]

            else:

                print(
                    f"ERROR: Unknown metadata type "
                    f"0x{record_type:02X}"
                )

            position += METADATA_SIZE

        # -------------------------
        # Telemetry packet
        # -------------------------

        elif header == TELEMETRY_MAGIC:

            rest = file.read(TELEMETRY_SIZE - 2)

            if len(rest) != TELEMETRY_SIZE - 2:
                print("ERROR: incomplete telemetry packet")
                break

            packet = header + rest

            telemetry = decode_telemetry(packet)

            total_telemetry_packets += 1

            if current_lap is not None:

                lap_data[current_lap]["telemetry_packets"] += 1

            position += TELEMETRY_SIZE

        # -------------------------
        # Unknown record
        # -------------------------

        else:

            print(
                f"ERROR: Unknown record at offset {position}: "
                f"{header.hex(' ')}"
            )

            break


# --------------------------------
# Print summary
# --------------------------------

print()
print("==============================")
print("       SESSION SUMMARY")
print("==============================")

print(f"Session:              {session_number}")
print(f"Start timestamp:      {session_start_timestamp}")
print(f"End timestamp:        {session_end_timestamp}")

print()
print("Laps:")
print("------------------------------")

for lap_number, data in lap_data.items():

    print(
        f"Lap {lap_number}:"
    )

    print(
        f"  Start timestamp:   "
        f"{data['start_timestamp']}"
    )

    print(
        f"  Telemetry packets: "
        f"{data['telemetry_packets']}"
    )

print()
print("------------------------------")
print(
    f"Total telemetry:      "
    f"{total_telemetry_packets}"
)
print("==============================")
