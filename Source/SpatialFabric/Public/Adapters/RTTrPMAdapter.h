// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "ISpatialProtocolAdapter.h"
#include "Sockets.h"

/**
 * FRTTrPMAdapter
 *
 * Implements the RTTrPM (Real-Time Tracking Protocol Motion) binary UDP format.
 * Source: RTTrP Wiki — https://rttrp.github.io/RTTrP-Wiki/RTTrPM.html
 *
 * RTTrPM is a binary UDP protocol (not OSC).  This adapter uses a raw FSocket
 * for sending, NOT ULiveOSCClientComponent (which is OSC-only).
 *
 * Default target port: 36700 (configurable).
 * Default send rate: 60 Hz.
 *
 * Packet structure per tracked object:
 *   Header:
 *     UInt16  Signature  = 0x4154  ('AT')
 *     UInt16  Version    = 0x0200  (2.00)
 *     UInt32  PacketID   (incrementing counter)
 *     UInt8   Format     = 0x00    (raw)
 *     UInt16  Size       (bytes following this field)
 *     UInt32  Context    = 0
 *     UInt8   NumModules = 1
 *   Module 1: Trackable (type 0x01)
 *     UInt8   TypeID     = 0x01
 *     UInt16  ModuleSize
 *     UInt16  NameLength
 *     char[]  Name       (object label, UTF-8)
 *     UInt8   NumSubmodules = 1
 *   Submodule: Centroid Position (type 0x02)
 *     UInt8   TypeID     = 0x02
 *     UInt16  Size       = 24
 *     double  X  (metres)
 *     double  Y  (metres)
 *     double  Z  (metres)
 *
 * All multi-byte integers and doubles are big-endian.
 */
class SPATIALFABRIC_API FRTTrPMAdapter : public ISpatialProtocolAdapter
{
public:
	FRTTrPMAdapter();
	virtual ~FRTTrPMAdapter() override;

	virtual FName GetName() const override { return TEXT("RTTrPM"); }
	virtual ESpatialAdapterType GetAdapterType() const override { return ESpatialAdapterType::RTTrPM; }

	virtual void Configure(const FSpatialAdapterConfig& InConfig) override;
	virtual void ProcessFrame(const FSpatialFrameSnapshot& Snapshot, float DeltaTime) override;
	virtual bool IsEnabled() const override { return Config.bEnabled; }

private:
	FSpatialAdapterConfig Config;

	/** Raw UDP socket for sending binary RTTrPM packets. */
	FSocket* UDPSocket = nullptr;
	/** Incrementing packet ID counter. */
	uint32 PacketCounter = 0;

	void OpenSocket();
	void CloseSocket();

	/**
	 * Build a complete RTTrPM packet for one object and return it as a
	 * byte array ready to send via UDP.
	 */
	TArray<uint8> BuildPacket(const FSpatialNormalizedState& State);

	/** Append a big-endian uint16 to Buffer. */
	static void AppendUInt16BE(TArray<uint8>& Buffer, uint16 Value);
	/** Append a big-endian uint32 to Buffer. */
	static void AppendUInt32BE(TArray<uint8>& Buffer, uint32 Value);
	/** Append a big-endian IEEE 754 double (64-bit) to Buffer. */
	static void AppendDoubleBE(TArray<uint8>& Buffer, double Value);
};
