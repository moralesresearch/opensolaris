/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Minimal, self-contained UEFI 2.x definitions for the OpenSolaris EFI
 * kernel loader.  Only the services the loader calls are given real
 * prototypes; every other slot is kept as an opaque pointer so that the
 * table layouts match the specification exactly.
 */

#ifndef _EFI_H
#define	_EFI_H

typedef unsigned char		UINT8;
typedef unsigned short		UINT16;
typedef unsigned int		UINT32;
typedef unsigned long long	UINT64;
typedef long long		INT64;
typedef UINT64			UINTN;
typedef UINT16			CHAR16;
typedef UINT8			BOOLEAN;
typedef UINTN			EFI_STATUS;
typedef void			*EFI_HANDLE;
typedef void			*EFI_EVENT;
typedef UINT64			EFI_PHYSICAL_ADDRESS;
typedef UINT64			EFI_VIRTUAL_ADDRESS;

#if defined(__x86_64__)
#define	EFIAPI	__attribute__((ms_abi))
#else
#define	EFIAPI
#endif

#define	EFI_ERROR_BIT		(1ULL << 63)
#define	EFI_SUCCESS		0
#define	EFI_LOAD_ERROR		(EFI_ERROR_BIT | 1)
#define	EFI_INVALID_PARAMETER	(EFI_ERROR_BIT | 2)
#define	EFI_UNSUPPORTED		(EFI_ERROR_BIT | 3)
#define	EFI_BUFFER_TOO_SMALL	(EFI_ERROR_BIT | 5)
#define	EFI_NOT_FOUND		(EFI_ERROR_BIT | 14)
#define	EFI_ERROR(s)		(((s) & EFI_ERROR_BIT) != 0)

typedef struct {
	UINT32	Data1;
	UINT16	Data2;
	UINT16	Data3;
	UINT8	Data4[8];
} EFI_GUID;

typedef struct {
	UINT64	Signature;
	UINT32	Revision;
	UINT32	HeaderSize;
	UINT32	CRC32;
	UINT32	Reserved;
} EFI_TABLE_HEADER;

/* EFI_ALLOCATE_TYPE */
#define	AllocateAnyPages	0
#define	AllocateMaxAddress	1
#define	AllocateAddress		2

/* EFI_MEMORY_TYPE */
#define	EfiReservedMemoryType		0
#define	EfiLoaderCode			1
#define	EfiLoaderData			2
#define	EfiBootServicesCode		3
#define	EfiBootServicesData		4
#define	EfiRuntimeServicesCode		5
#define	EfiRuntimeServicesData		6
#define	EfiConventionalMemory		7

typedef struct {
	UINT32			Type;
	UINT32			Pad;
	EFI_PHYSICAL_ADDRESS	PhysicalStart;
	EFI_VIRTUAL_ADDRESS	VirtualStart;
	UINT64			NumberOfPages;
	UINT64			Attribute;
} EFI_MEMORY_DESCRIPTOR;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, CHAR16 *);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
	void			*Reset;
	EFI_TEXT_STRING		OutputString;
	void			*TestString;
	void			*QueryMode;
	void			*SetMode;
	void			*SetAttribute;
	void			*ClearScreen;
	void			*SetCursorPosition;
	void			*EnableCursor;
	void			*Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
	EFI_TABLE_HEADER	Hdr;

	void			*RaiseTPL;
	void			*RestoreTPL;

	EFI_STATUS (EFIAPI *AllocatePages)(UINTN, UINTN, UINTN,
	    EFI_PHYSICAL_ADDRESS *);
	EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
	EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *, EFI_MEMORY_DESCRIPTOR *,
	    UINTN *, UINTN *, UINT32 *);
	EFI_STATUS (EFIAPI *AllocatePool)(UINTN, UINTN, void **);
	EFI_STATUS (EFIAPI *FreePool)(void *);

	void			*CreateEvent;
	void			*SetTimer;
	void			*WaitForEvent;
	void			*SignalEvent;
	void			*CloseEvent;
	void			*CheckEvent;

	void			*InstallProtocolInterface;
	void			*ReinstallProtocolInterface;
	void			*UninstallProtocolInterface;
	EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
	void			*Reserved;
	void			*RegisterProtocolNotify;
	void			*LocateHandle;
	void			*LocateDevicePath;
	void			*InstallConfigurationTable;

	void			*LoadImage;
	void			*StartImage;
	void			*Exit;
	void			*UnloadImage;
	EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);

	void			*GetNextMonotonicCount;
	EFI_STATUS (EFIAPI *Stall)(UINTN);
	EFI_STATUS (EFIAPI *SetWatchdogTimer)(UINTN, UINT64, UINTN,
	    CHAR16 *);

	void			*ConnectController;
	void			*DisconnectController;

	void			*OpenProtocol;
	void			*CloseProtocol;
	void			*OpenProtocolInformation;

	void			*ProtocolsPerHandle;
	void			*LocateHandleBuffer;
	void			*LocateProtocol;
	void			*InstallMultipleProtocolInterfaces;
	void			*UninstallMultipleProtocolInterfaces;

	void			*CalculateCrc32;
	void			*CopyMem;
	void			*SetMem;
	void			*CreateEventEx;
} EFI_BOOT_SERVICES;

typedef struct {
	EFI_GUID		VendorGuid;
	void			*VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
	EFI_TABLE_HEADER			Hdr;
	CHAR16					*FirmwareVendor;
	UINT32					FirmwareRevision;
	EFI_HANDLE				ConsoleInHandle;
	void					*ConIn;
	EFI_HANDLE				ConsoleOutHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL		*ConOut;
	EFI_HANDLE				StandardErrorHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL		*StdErr;
	void					*RuntimeServices;
	EFI_BOOT_SERVICES			*BootServices;
	UINTN					NumberOfTableEntries;
	EFI_CONFIGURATION_TABLE			*ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
	UINT32			Revision;
	EFI_HANDLE		ParentHandle;
	EFI_SYSTEM_TABLE	*SystemTable;
	EFI_HANDLE		DeviceHandle;
	void			*FilePath;
	void			*Reserved;
	UINT32			LoadOptionsSize;
	void			*LoadOptions;
	void			*ImageBase;
	UINT64			ImageSize;
	UINT32			ImageCodeType;
	UINT32			ImageDataType;
	void			*Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

struct _EFI_FILE_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;

struct _EFI_FILE_PROTOCOL {
	UINT64		Revision;
	EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL *, EFI_FILE_PROTOCOL **,
	    CHAR16 *, UINT64, UINT64);
	EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL *);
	void		*Delete;
	EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL *, UINTN *, void *);
	void		*Write;
	void		*GetPosition;
	void		*SetPosition;
	EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL *, EFI_GUID *,
	    UINTN *, void *);
	void		*SetInfo;
	void		*Flush;
};

#define	EFI_FILE_MODE_READ	0x0000000000000001ULL

typedef struct {
	UINT64		Revision;
	EFI_STATUS (EFIAPI *OpenVolume)(void *, EFI_FILE_PROTOCOL **);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
	UINT64		Size;
	UINT64		FileSize;
	UINT64		PhysicalSize;
	UINT8		Times[48];	/* CreateTime, LastAccess, Modification */
	UINT64		Attribute;
	CHAR16		FileName[1];
} EFI_FILE_INFO;

#define	EFI_LOADED_IMAGE_PROTOCOL_GUID \
	{ 0x5b1b31a1, 0x9562, 0x11d2, \
	{ 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }
#define	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
	{ 0x964e5b22, 0x6459, 0x11d2, \
	{ 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }
#define	EFI_FILE_INFO_GUID \
	{ 0x09576e92, 0x6d3f, 0x11d2, \
	{ 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }
#define	EFI_DTB_TABLE_GUID \
	{ 0xb1b621d5, 0xf19c, 0x41a5, \
	{ 0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0 } }
#define	EFI_ACPI_20_TABLE_GUID \
	{ 0x8868e871, 0xe4f1, 0x11d3, \
	{ 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 } }

#endif /* _EFI_H */
