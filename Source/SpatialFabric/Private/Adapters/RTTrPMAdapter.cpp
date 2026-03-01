// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#include "Adapters/RTTrPMAdapter.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

// RTTrPM protocol constants (RTTrP Wiki: https://rttrp.github.io/RTTrP-Wiki/RTTrPM.html)
static constexpr uint16 RTTRPM_SIGNATURE     = 0x4154; // 'AT'
static constexpr uint16 RTTRPM_VERSION       = 0x0200; // 2.00
static constexpr uint8  RTTRPM_FORMAT_RAW    = 0x00;
static constexpr uint8  RTTRPM_MOD_TRACKABLE = 0x01;
static constexpr uint8  RTTRPM_SUBMOD_CENTROID_POS = 0x02;

FRTTrPMAdapter::FRTTrPMAdapter()
{
}

FRTTrPMAdapter::~FRTTrPMAdapter()
{
	CloseSocket();
}

void FRTTrPMAdapter::Configure(const FSpatialAdapterConfig& InConfig)
{
	Config = InConfig;
	CachedSendRateHz = InConfig.SendRateHz;
	CloseSocket();
	if (Config.bEnabled)
	{
		OpenSocket();
	}
}

void FRTTrPMAdapter::ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime)
{
	if (!Config.bEnabled || !UDPSocket) { return; }
	if (!ShouldSendThisFrame(DeltaTime)) { return; }

	for (const FSpatialNormalizedState& State : Snapshot.Objects)
	{
		TArray<uint8> Packet = BuildPacket(State);
		if (Packet.Num() == 0) { continue; }

		ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!SocketSub) { continue; }

		TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
		bool bValid = false;
		Addr->SetIp(*Config.TargetIP, bValid);
		Addr->SetPort(Config.TargetPort);

		if (bValid)
		{
			int32 BytesSent = 0;
			UDPSocket->SendTo(Packet.GetData(), Packet.Num(), BytesSent, *Addr);

			if (OnLog)
			{
				FSpatialFabricLogEntry Entry;
				Entry.Adapter   = TEXT("RTTrPM");
				Entry.Direction = TEXT("OUT");
				Entry.Address   = FString::Printf(TEXT("Trackable[%d] '%s'"),
					State.ObjectID, *State.Label);
				const FVector& M = State.StageMeters;
				Entry.ValueStr  = FString::Printf(TEXT("%.3fm %.3fm %.3fm"), M.X, M.Y, M.Z);
				FDateTime Now = FDateTime::Now();
				Entry.Timestamp = FString::Printf(TEXT("%02d:%02d:%02d"),
					Now.GetHour(), Now.GetMinute(), Now.GetSecond());
				OnLog(Entry);
			}
		}
	}
}

TArray<uint8> FRTTrPMAdapter::BuildPacket(const FSpatialNormalizedState& State)
{
	TArray<uint8> Buffer;

	// Encode trackable name as UTF-8 bytes
	FTCHARToUTF8 NameUTF8(*State.Label);
	const uint8* NameBytes  = (const uint8*)NameUTF8.Get();
	const uint16 NameLen    = (uint16)NameUTF8.Length();

	// ── Centroid Position submodule ─────────────────────────────────────────
	// Type(1) + Size(2) + X(8) + Y(8) + Z(8) = 27 bytes
	const uint16 SubmodSize = 1 + 2 + 8 + 8 + 8; // TypeID + Size field + 3 doubles

	// ── Trackable module ────────────────────────────────────────────────────
	// Type(1) + ModSize(2) + NameLen(2) + Name(N) + NumSubmodules(1) + submod
	const uint16 ModSize = 1 + 2 + NameLen + 1 + SubmodSize;

	// ── Packet size (bytes after the Size field itself) ────────────────────
	// Context(4) + NumModules(1) + Type(1) + ModSize(2) + ModBody(ModSize)
	// The "size" field in the header counts everything after the 'size' field
	// (from Context onwards).
	const uint16 PacketSize = 4 + 1 + 1 + 2 + ModSize;

	// ── Header ─────────────────────────────────────────────────────────────
	AppendUInt16BE(Buffer, RTTRPM_SIGNATURE);                // 0x4154
	AppendUInt16BE(Buffer, RTTRPM_VERSION);                  // 0x0200
	AppendUInt32BE(Buffer, ++PacketCounter);                 // Packet ID
	Buffer.Add(RTTRPM_FORMAT_RAW);                           // Format = 0
	AppendUInt16BE(Buffer, PacketSize);                      // Packet size
	AppendUInt32BE(Buffer, 0);                               // Context = 0
	Buffer.Add(1);                                           // NumModules = 1

	// ── Trackable module ────────────────────────────────────────────────────
	Buffer.Add(RTTRPM_MOD_TRACKABLE);                        // TypeID = 0x01
	AppendUInt16BE(Buffer, ModSize);                         // Module size
	AppendUInt16BE(Buffer, NameLen);                         // Name length
	Buffer.Append(NameBytes, NameLen);                       // Name bytes
	Buffer.Add(1);                                           // NumSubmodules = 1

	// ── Centroid Position submodule ─────────────────────────────────────────
	Buffer.Add(RTTRPM_SUBMOD_CENTROID_POS);                  // TypeID = 0x02
	AppendUInt16BE(Buffer, SubmodSize);                      // Submodule size
	AppendDoubleBE(Buffer, (double)State.StageMeters.X);     // X in metres
	AppendDoubleBE(Buffer, (double)State.StageMeters.Y);     // Y in metres
	AppendDoubleBE(Buffer, (double)State.StageMeters.Z);     // Z in metres

	return Buffer;
}

// ── Socket helpers ────────────────────────────────────────────────────────

void FRTTrPMAdapter::OpenSocket()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSub) { return; }

	UDPSocket = SocketSub->CreateSocket(NAME_DGram, TEXT("RTTrPMSend"), false);
	if (UDPSocket)
	{
		UDPSocket->SetNonBlocking(true);
	}
}

void FRTTrPMAdapter::CloseSocket()
{
	if (UDPSocket)
	{
		ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSub)
		{
			SocketSub->DestroySocket(UDPSocket);
		}
		UDPSocket = nullptr;
	}
}

// ── Big-endian serialization ──────────────────────────────────────────────

void FRTTrPMAdapter::AppendUInt16BE(TArray<uint8>& Buffer, uint16 Value)
{
	Buffer.Add((uint8)(Value >> 8));
	Buffer.Add((uint8)(Value & 0xFF));
}

void FRTTrPMAdapter::AppendUInt32BE(TArray<uint8>& Buffer, uint32 Value)
{
	Buffer.Add((uint8)((Value >> 24) & 0xFF));
	Buffer.Add((uint8)((Value >> 16) & 0xFF));
	Buffer.Add((uint8)((Value >>  8) & 0xFF));
	Buffer.Add((uint8)( Value        & 0xFF));
}

void FRTTrPMAdapter::AppendDoubleBE(TArray<uint8>& Buffer, double Value)
{
	// Reinterpret as 64-bit integer for byte extraction
	uint64 Bits;
	FMemory::Memcpy(&Bits, &Value, sizeof(double));

	Buffer.Add((uint8)((Bits >> 56) & 0xFF));
	Buffer.Add((uint8)((Bits >> 48) & 0xFF));
	Buffer.Add((uint8)((Bits >> 40) & 0xFF));
	Buffer.Add((uint8)((Bits >> 32) & 0xFF));
	Buffer.Add((uint8)((Bits >> 24) & 0xFF));
	Buffer.Add((uint8)((Bits >> 16) & 0xFF));
	Buffer.Add((uint8)((Bits >>  8) & 0xFF));
	Buffer.Add((uint8)( Bits        & 0xFF));
}
