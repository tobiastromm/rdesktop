/* -*- c-basic-offset: 8 -*- */
/* Explicit capability boundary for optional RDPDR device backends that have
   not yet been implemented for Windows. The RDPDR protocol core remains in
   use for builds with supported devices. */
#include "rdesktop.h"

static RD_NTSTATUS
unsupported_create(uint32 device, uint32 desired_access, uint32 share_mode,
		   uint32 create_disposition, uint32 flags_and_attributes,
		   char *filename, RD_NTHANDLE *handle)
{
	UNUSED(device);
	UNUSED(desired_access);
	UNUSED(share_mode);
	UNUSED(create_disposition);
	UNUSED(flags_and_attributes);
	UNUSED(filename);
	UNUSED(handle);
	return RD_STATUS_NOT_SUPPORTED;
}

static RD_NTSTATUS
unsupported_close(RD_NTHANDLE handle)
{
	UNUSED(handle);
	return RD_STATUS_NOT_SUPPORTED;
}

static RD_NTSTATUS
unsupported_read(RD_NTHANDLE handle, uint8 *data, uint32 length, uint64 offset,
		 uint32 *result)
{
	UNUSED(handle);
	UNUSED(data);
	UNUSED(length);
	UNUSED(offset);
	if (result != NULL)
		*result = 0;
	return RD_STATUS_NOT_SUPPORTED;
}

static RD_NTSTATUS
unsupported_write(RD_NTHANDLE handle, uint8 *data, uint32 length, uint64 offset,
		  uint32 *result)
{
	return unsupported_read(handle, data, length, offset, result);
}

static RD_NTSTATUS
unsupported_device_control(RD_NTHANDLE handle, uint32 request, STREAM in, STREAM out)
{
	UNUSED(handle);
	UNUSED(request);
	UNUSED(in);
	UNUSED(out);
	return RD_STATUS_NOT_SUPPORTED;
}

DEVICE_FNS serial_fns = {
	unsupported_create, unsupported_close, unsupported_read, unsupported_write,
	unsupported_device_control
};
DEVICE_FNS parallel_fns = {
	unsupported_create, unsupported_close, unsupported_read, unsupported_write,
	unsupported_device_control
};
DEVICE_FNS printer_fns = {
	unsupported_create, unsupported_close, unsupported_read, unsupported_write,
	unsupported_device_control
};
DEVICE_FNS disk_fns = {
	unsupported_create, unsupported_close, unsupported_read, unsupported_write,
	unsupported_device_control
};

static int
unsupported_device(const char *name, uint32 *id, char *optarg)
{
	UNUSED(id);
	UNUSED(optarg);
	logger(Core, Error, "%s device redirection is not supported on this Win32 build", name);
	return -1;
}

int serial_enum_devices(uint32 *id, char *optarg)
{
	return unsupported_device("Serial", id, optarg);
}

int parallel_enum_devices(uint32 *id, char *optarg)
{
	return unsupported_device("Parallel", id, optarg);
}

int printer_enum_devices(uint32 *id, char *optarg)
{
	return unsupported_device("Printer", id, optarg);
}

int disk_enum_devices(uint32 *id, char *optarg)
{
	return unsupported_device("Disk", id, optarg);
}

RD_BOOL serial_get_event(RD_NTHANDLE handle, uint32 *result)
{
	UNUSED(handle);
	if (result != NULL)
		*result = 0;
	return False;
}

RD_BOOL serial_get_timeout(RD_NTHANDLE handle, uint32 length, uint32 *timeout,
			   uint32 *interval_timeout)
{
	UNUSED(handle);
	UNUSED(length);
	if (timeout != NULL)
		*timeout = 0;
	if (interval_timeout != NULL)
		*interval_timeout = 0;
	return False;
}

RD_NTSTATUS disk_query_information(RD_NTHANDLE handle, uint32 info_class, STREAM out)
{
	UNUSED(handle); UNUSED(info_class); UNUSED(out);
	return RD_STATUS_NOT_SUPPORTED;
}

RD_NTSTATUS disk_set_information(RD_NTHANDLE handle, uint32 info_class, STREAM in, STREAM out)
{
	UNUSED(handle); UNUSED(info_class); UNUSED(in); UNUSED(out);
	return RD_STATUS_NOT_SUPPORTED;
}

RD_NTSTATUS disk_check_notify(RD_NTHANDLE handle)
{
	UNUSED(handle);
	return RD_STATUS_NOT_SUPPORTED;
}

RD_NTSTATUS disk_create_notify(RD_NTHANDLE handle, uint32 info_class)
{
	UNUSED(handle); UNUSED(info_class);
	return RD_STATUS_NOT_SUPPORTED;
}

RD_NTSTATUS disk_query_volume_information(RD_NTHANDLE handle, uint32 info_class, STREAM out)
{
	UNUSED(handle); UNUSED(info_class); UNUSED(out);
	return RD_STATUS_NOT_SUPPORTED;
}

RD_NTSTATUS disk_query_directory(RD_NTHANDLE handle, uint32 info_class, char *pattern, STREAM out)
{
	UNUSED(handle); UNUSED(info_class); UNUSED(pattern); UNUSED(out);
	return RD_STATUS_NOT_SUPPORTED;
}

int
printercache_load_blob(char *printer_name, uint8 **data)
{
	UNUSED(printer_name);
	if (data != NULL)
		*data = NULL;
	return 0;
}

void
printercache_process(STREAM s)
{
	UNUSED(s);
}
