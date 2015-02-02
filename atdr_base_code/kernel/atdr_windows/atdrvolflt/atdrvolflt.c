#include "atdr_bitmap.h"
#include "atdrvolflt.h"


PDEVICE_OBJECT  ControlObj;

NTSTATUS atdrFltAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT pdo)
{
	NTSTATUS Status;
	PDEVICE_OBJECT  fdo;
	//KEVENT event;
	PVOL_INFO pVolInfo = (PVOL_INFO)ExAllocatePool(NonPagedPool, sizeof(VOL_INFO));
	PATDR_BITMAP patdr_bitmap = ExAllocatePool(NonPagedPool, sizeof(ATDR_BITMAP));
	PDEVICE_EXTENSION pdx;
	Status = IoCreateDevice(DriverObject,
		sizeof(DEVICE_EXTENSION), NULL,
		FILE_DEVICE_DISK, 0, FALSE, &fdo);
	IoRegisterShutdownNotification(fdo);
	if (!NT_SUCCESS(Status)) {
		DbgPrint("atdrFltAddDevice: Fialed IoCreateDevice in AddDevice \n");
		return Status;
	}
	fdo->Flags |= DO_DIRECT_IO;
	pdx = (PDEVICE_EXTENSION)fdo->DeviceExtension;
	pdx->pdo = pdo;
	pdx->fdo = fdo;
	pdx->VolInfo = pVolInfo;
	pVolInfo->atdr_tracking_enabled = FALSE;
	pdx->VolInfo->atdr_bitmap = patdr_bitmap;
	fdo->Flags |= DO_POWER_PAGABLE;
//	pdx->PagingPathCountEvent = &event;
	KeInitializeSpinLock(&patdr_bitmap->Lock);
	KeInitializeSpinLock(&patdr_bitmap->Lock_bm);

	pdx->LowerDeviceObject = IoAttachDeviceToDeviceStack(fdo, pdo);
	if (!pdx->LowerDeviceObject)
    {
		DbgPrint("atdrFltAddDevice: Fialed IoCreateDevice in AddDevice \n");
		return STATUS_INSUFFICIENT_RESOURCES;
    }
	fdo->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}



NTSTATUS
atdrFltSendToNextDriver(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS Status = STATUS_SUCCESS;
	
	if (DeviceObject == ControlObj)
	{
		Irp->IoStatus.Status = Status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	}
	
	PDEVICE_EXTENSION   pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(pdx->LowerDeviceObject, Irp);
}

NTSTATUS atdrFltForwardIrpSynchronous(IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	
	)
{
	PDEVICE_EXTENSION   pdx;
	PKEVENT event = ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT), 'eveK');
	NTSTATUS Status =	STATUS_SUCCESS;
	if (DeviceObject == ControlObj)
	{
		Irp->IoStatus.Status = Status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	}
	KeInitializeEvent(event, NotificationEvent, FALSE);
	pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	//
	// copy the irpstack for the next device
	//

	IoCopyCurrentIrpStackLocationToNext(Irp);

	//
	// set a completion routine
	//

	IoSetCompletionRoutine(Irp, atdrFltIrpCompletion,
		event, TRUE, TRUE, TRUE);

	//
	// call the next lower device
	//

	Status = IoCallDriver(pdx->LowerDeviceObject, Irp);

	//
	// wait for the actual completion
	//

	if (Status == STATUS_PENDING) {
		KeWaitForSingleObject(event, Executive, KernelMode, FALSE, NULL);
		Status = Irp->IoStatus.Status;
	}

	return Status;

} // end atdrFltForwardIrpSynchronous()



#define FILTER_DEVICE_PROPOGATE_FLAGS            0
#define FILTER_DEVICE_PROPOGATE_CHARACTERISTICS (FILE_REMOVABLE_MEDIA |  \
	FILE_READ_ONLY_DEVICE | \
	FILE_FLOPPY_DISKETTE    \
	)
VOID atdrFltSyncFilterWithTarget(IN PDEVICE_OBJECT FilterDevice, IN PDEVICE_OBJECT TargetDevice)
{
	ULONG                   propFlags;

	propFlags = TargetDevice->Flags & FILTER_DEVICE_PROPOGATE_FLAGS;
	FilterDevice->Flags |= propFlags;

	propFlags = TargetDevice->Characteristics & FILTER_DEVICE_PROPOGATE_CHARACTERISTICS;
	FilterDevice->Characteristics |= propFlags;
}



NTSTATUS
atdrFltIrpCompletion(
IN PDEVICE_OBJECT DeviceObject,
IN PIRP Irp,
IN PVOID Context
)

/*++

Routine Description:

Forwarded IRP completion routine. Set an event and return
STATUS_MORE_PROCESSING_REQUIRED. Irp forwarder will wait on this
event and then re-complete the irp after cleaning up.

Arguments:

DeviceObject is the device object of the WMI driver
Irp is the WMI irp that was just completed
Context is a PKEVENT that forwarder will wait on

Return Value:

STATUS_MORE_PORCESSING_REQUIRED

--*/

{
	PKEVENT Event = (PKEVENT)Context;

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	KeSetEvent(Event, IO_NO_INCREMENT, FALSE);
	return(STATUS_MORE_PROCESSING_REQUIRED);

} // end atdrFltIrpCompletion()

#define set_bit(nr, addr)    (void)test_and_set_bit(nr, addr)



int test_and_set_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned int *adr = (unsigned int *)addr;


	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);

	retval = (mask & *adr) != 0;
	*adr |= mask;

	return retval;
}
void atdr_set_bits(PATDR_BITMAP bitmap, int first, int last)
{
	int i = 0;
	unsigned long *bmap = NULL;
	KIRQL Irql;
	//Lock the access it may get changed due to SET_GRAIN IOCTL
	//	spin_lock(&device->bitmap->bm_lock);
	KeAcquireSpinLock(&bitmap->Lock_bm, &Irql);
	bmap = (long unsigned int *)bitmap->bitmap_area;
	for (i = first; i <= last; i++)
	{
		set_bit(i, bmap);
	}
	//	spin_unlock(&device->bitmap->bm_lock);
	KeReleaseSpinLock(&bitmap->Lock_bm, Irql);



}


//This address is in sector hence convert addr = address >> 9;
//Grain_size is in bytes
UINT32 atdr_find_pos(ATDR_BITMAP *bitmap, sector_t addr)
{
	UINT32 cur = 0;
	UINT32 	 grain_size = bitmap->grain_size;
	UINT32 num_bits = bitmap->total_num_bits;

	for (cur = 0; cur < num_bits; cur++)
	{
		if (addr < ((cur + 1) * (grain_size >> 9)))
		{
			return cur;
		}
	}
	return cur;
}


NTSTATUS atdrFltWrite(
IN PDEVICE_OBJECT DeviceObject,
IN PIRP Irp
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	if (DeviceObject == ControlObj)
	{
		Irp->IoStatus.Status = Status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return Status;
	}
	PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	PDEVICE_EXTENSION   pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	if (pdx->VolInfo->atdr_tracking_enabled == TRUE)
	{
		DbgPrint("Length = %x   offset = %I64x\n", currentIrpStack->Parameters.Write.Length,
			currentIrpStack->Parameters.Write.ByteOffset.QuadPart);
		//Rohit
		sector_t start_addr, end_addr;
		UINT32 first, last;
		
		//Converting Start Write addr and End Write addr to sectors

		start_addr = currentIrpStack->Parameters.Write.ByteOffset.QuadPart >> 9;
		end_addr = (currentIrpStack->Parameters.Write.ByteOffset.QuadPart + currentIrpStack->Parameters.Write.Length) >> 9;

		//Set the first argument so that Mdl on the device can be fetched and corresponding bit be set in the bitmap
		first = atdr_find_pos(pdx->VolInfo->atdr_bitmap, start_addr);
		last = atdr_find_pos(pdx->VolInfo->atdr_bitmap, end_addr);
		atdr_set_bits(pdx->VolInfo->atdr_bitmap, first, last);
	}
	/*
	if(pdx->VolInfo->Tracking_Enabled == TRUE)
	{
		start_addr = bio->bi_sector;
		end_addr = bio_end_sector(bio);
	
	}
	*/
	 
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(pdx->LowerDeviceObject, Irp);
	
}

NTSTATUS
atdrFltDispatchPower(
IN PDEVICE_OBJECT DeviceObject,
IN PIRP Irp
)
{
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
#if (NTDDI_VERSION < NTDDI_VISTA)
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(pdx->LowerDeviceObject, Irp);
#else
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(pdx->LowerDeviceObject, Irp);
#endif

}


NTSTATUS atdrFltStartDevice(
IN PDEVICE_OBJECT DeviceObject,
IN PIRP Irp
)
{
	PDEVICE_EXTENSION   pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;;
	NTSTATUS            Status = STATUS_SUCCESS;

	Status = atdrFltForwardIrpSynchronous(DeviceObject, Irp);

	atdrFltSyncFilterWithTarget(DeviceObject,
		pdx->LowerDeviceObject);
	
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}
	


NTSTATUS atdrFltRemoveDevice(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS            Status;
	PDEVICE_EXTENSION   pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	Status = atdrFltForwardIrpSynchronous(DeviceObject, Irp);

	PDEVICE_OBJECT TargetDevObj = pDeviceExtension->LowerDeviceObject;
	pDeviceExtension->LowerDeviceObject = NULL;
	IoDetachDevice(TargetDevObj);
	IoDeleteDevice(DeviceObject);


	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}

NTSTATUS
atdrFltDispatchPnp(
IN PDEVICE_OBJECT DeviceObject,
IN PIRP Irp
)
{
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS            Status = STATUS_SUCCESS;
	
	switch (irpSp->MinorFunction) {
	case IRP_MN_START_DEVICE:
		Status = atdrFltStartDevice(DeviceObject, Irp);
		break;
//	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
	//{
		  
		// PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
		// ULONG count;
    	// BOOLEAN setPagable;
		// UNICODE_STRING pagefileName;

		 //if (irpStack->Parameters.UsageNotification.Type != DeviceUsageTypePaging) {
		//	 Status = atdrFltSendToNextDriver(DeviceObject, Irp);
			// break; // out of case statement
		 //}

		 //RtlInitUnicodeString(&pagefileName, L"\\pagefile.sys");
		 //deviceExtension = DeviceObject->DeviceExtension;
		 //
		 // wait on the paging path event
		 //
		 //Status = KeWaitForSingleObject(&deviceExtension->PagingPathCountEvent,
			// Executive, KernelMode,
			 //FALSE, NULL);



		//
	//}
	//	break;
	case IRP_MN_REMOVE_DEVICE:
		Status = atdrFltRemoveDevice(DeviceObject, Irp);
		break;
	default:
		Status = atdrFltStartDevice(DeviceObject, Irp);
	}
	return Status;
}

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG               ulIndex;
	PDRIVER_DISPATCH  * dispatch;
	UNREFERENCED_PARAMETER(RegistryPath);
	UNICODE_STRING nameString, linkString;
	
	RtlInitUnicodeString(&nameString, L"\\Device\\atdrvolflt");
	status = IoCreateDevice(
		DriverObject,
		0,
		&nameString,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&ControlObj);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("DriverEntry failed: IoCreateDevice fail to create device\n");
		return status;
	}
	RtlInitUnicodeString(&linkString, L"\\DosDevices\\atdrvolflt");
	status = IoCreateSymbolicLink(&linkString, &nameString);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(ControlObj);
		DbgPrint("DriverEntry failed: IoCreateSymbolicLink fails to create symlink\n");
		return status;
	}

	for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
		ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
		ulIndex++, dispatch++) 
	{
		*dispatch = atdrFltSendToNextDriver;
	}
	DriverObject->DriverExtension->AddDevice = atdrFltAddDevice;
	DriverObject->DriverUnload = atdrFltUnload;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = atdrFltDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = atdrFltWrite;
	DriverObject->MajorFunction[IRP_MJ_PNP] = atdrFltDispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_POWER] = atdrFltDispatchPower;
	DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = atdrFltShutdownFlush;
	DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = atdrFltShutdownFlush;
	return STATUS_SUCCESS;
}

NTSTATUS atdrFltShutdownFlush(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	)
{
	PDEVICE_EXTENSION   pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(pdx->LowerDeviceObject, Irp);
}

NTSTATUS atdrFltDeviceControl(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	)
{	
		
	NTSTATUS status = STATUS_SUCCESS;
	KIRQL OldIrql;
	PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	UINT32 ctrlCode = currentIrpStack->Parameters.DeviceIoControl.IoControlCode;
	PDEVICE_EXTENSION   pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PVOL_INFO pVolInfo = pdx->VolInfo;
	switch (ctrlCode)
	{
		case IOCTL_MOUNTDEV_LINK_CREATED:
	    {
			
			PMOUNTDEV_NAME pMountDevName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;

			if (DeviceObject == ControlObj)
			{
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				return status;
			}
			if (currentIrpStack->Parameters.DeviceIoControl.InputBufferLength >= 0x62)
			{
				RtlCopyMemory(pVolInfo->wGUID, &pMountDevName->Name[11], 0x48);
				pVolInfo->wGUID[36] = L'\0';
				
			}
			else if (currentIrpStack->Parameters.DeviceIoControl.InputBufferLength >= 0x1E)
			{
				pVolInfo->wDriveLetter = pMountDevName->Name[12];
				
			}
			IoSkipCurrentIrpStackLocation(Irp);
			status = IoCallDriver(pdx->LowerDeviceObject, Irp);
			//atdrFltStartDevice(DeviceObject, Irp); We got a panic when trying to mount the devices
			break;
		}
		case ATDRCTL_DEV_CREATE:
		{
						
			PATDR_BITMAP atdr_bitmap = pVolInfo->atdr_bitmap;
			PVOID  MappedUserAddress = NULL;
			IOCTL *ioctl = (IOCTL *)Irp->AssociatedIrp.SystemBuffer;

			atdr_bitmap->bitmap_size = ioctl->u_bitmap_size;
			atdr_bitmap->grain_size = ioctl->u_grain_size;
						
			atdr_bitmap->total_num_bits = ioctl->u_bitmap_size * 8;

			if (atdr_bitmap->bitmap_size > 4 * 4096)
				atdr_bitmap->bitmap_size = 4 * 4096;
			if (PsGetCurrentThread() == Irp->Tail.Overlay.Thread)
			{
				POUTPUT_CREATE Output = (POUTPUT_CREATE)Irp->AssociatedIrp.SystemBuffer;
				if (pdx->VolInfo->atdr_tracking_enabled == FALSE)
				{

					pdx->VolInfo->atdr_tracking_enabled = TRUE;
					status = MapBufferForUserModeUse(pVolInfo, &MappedUserAddress);
					atdr_bitmap->MappedUserAddress = MappedUserAddress;
					atdr_bitmap->MappedUserAddressBackup = (char *)atdr_bitmap->MappedUserAddress + atdr_bitmap->bitmap_size;

					//		RtlCopyMemory(ioctl->active_bitmap, &MappedUserAddress, 4);

					
					Output->MappedUserAddress = MappedUserAddress;

					Irp->IoStatus.Information = sizeof(OUTPUT_CREATE);
				} else {
					atdr_bitmap->Mdl = IoAllocateMdl(atdr_bitmap->bitmap_area_start, 2 * atdr_bitmap->bitmap_size, FALSE, FALSE, OPTIONAL NULL);
					__try {
						MmProbeAndLockPages(atdr_bitmap->Mdl, KernelMode, IoWriteAccess);
						atdr_bitmap->MappedUserAddress = MmMapLockedPagesSpecifyCache(atdr_bitmap->Mdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority);
						Output->MappedUserAddress = atdr_bitmap->MappedUserAddress;
						Irp->IoStatus.Information = sizeof(OUTPUT_CREATE);
					}
					__except (EXCEPTION_EXECUTE_HANDLER)
					{
						DbgPrint("Exception occured while mapping pages to user mode\n");
						IoFreeMdl(atdr_bitmap->Mdl);
						Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
						Output->MappedUserAddress = NULL;
						Irp->IoStatus.Information = sizeof(OUTPUT_CREATE);
					}
				}
       	}
			Irp->IoStatus.Status = status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			break;
		}
		case ATDRCTL_GET_USER:
		{
			PDEVICE_EXTENSION  pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
			PVOL_INFO pVolInfo = pdx->VolInfo;
			PATDR_BITMAP atdr_bitmap = pVolInfo->atdr_bitmap;
			BMAP_USER user = atdr_bitmap->bmap_user;
			POUTPUT_USER Output = (POUTPUT_USER)Irp->AssociatedIrp.SystemBuffer;
			if (user == KERNEL)
			{
				Output->bmap_user = 0;
			}
			else {
				Output->bmap_user = 1;
			}
			Irp->IoStatus.Information = sizeof(OUTPUT_USER);
			Irp->IoStatus.Status = status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			break;
		}
		case ATDR_FLIP_BITMAP_AREA:
		{
			PDEVICE_EXTENSION  pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
			PVOL_INFO pVolInfo = pdx->VolInfo;
			PATDR_BITMAP atdr_bitmap = pVolInfo->atdr_bitmap;		
			atdr_bitmap->Mdl = NULL;
			KeAcquireSpinLock(&atdr_bitmap->Lock, &OldIrql);		 //If the current user of Bitmap is USER
			if (atdr_bitmap->bmap_user == USER) {
				 //Change the bitmap_area to the area e.g.
				 atdr_bitmap->bitmap_area = atdr_bitmap->bitmap_area_start;
						//Change it to the Kernel e.g. Userlan need to use teh backup area
				 atdr_bitmap->bmap_user = KERNEL;
			}
			else 
			{
					 //Change the bitmap_area to the backup area e.g.
				 atdr_bitmap->bitmap_area = atdr_bitmap->bitmap_area_backup;

			 //Change it to the User e.g. Now userlan code can use this area
				 atdr_bitmap->bmap_user = USER;
			}

			KeReleaseSpinLock(&atdr_bitmap->Lock, OldIrql);
			Irp->IoStatus.Status = status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			break;
			
		}
		case ATDRCTL_DEV_DESTROY:
		{
			PATDR_BITMAP atdr_bitmap = pVolInfo->atdr_bitmap;
			
			if (pVolInfo->atdr_tracking_enabled == TRUE)
			{

				MmUnmapLockedPages(atdr_bitmap->MappedUserAddress, atdr_bitmap->Mdl);
				//IoFreeMdl(atdr_bitmap->Mdl);
				ExFreePoolWithTag(atdr_bitmap->bitmap_area_start, 'mtiB');
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
			}
			break;
		}
		default:
		{
		    if (DeviceObject == ControlObj)
			{    
			     Irp->IoStatus.Status = status;
				 IoCompleteRequest(Irp, IO_NO_INCREMENT);
			}
			else
			{
				PDEVICE_EXTENSION   pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
			    IoSkipCurrentIrpStackLocation(Irp);
				status = IoCallDriver(pdx->LowerDeviceObject, Irp);
			}			   
	     }
	}
	return status;
}

NTSTATUS  MapBufferForUserModeUse(PVOL_INFO pVolInfo, void **MappedUserAddress)
{
	NTSTATUS status = STATUS_SUCCESS;
	PATDR_BITMAP atdr_bitmap = pVolInfo->atdr_bitmap;
	PHYSICAL_ADDRESS    lowAddress;
	PHYSICAL_ADDRESS    highAddress;
	SIZE_T              totalBytes;
	lowAddress.QuadPart = 0;
	highAddress.QuadPart = 0xFFFFFFFFFFFFFFFF;
	totalBytes = 4 * 4096 * 2;

	atdr_bitmap->bitmap_area_start = ExAllocatePoolWithTag(NonPagedPool, 2 * atdr_bitmap->bitmap_size,'mtiB');
	RtlZeroMemory(atdr_bitmap->bitmap_area_start, 2 * atdr_bitmap->bitmap_size);
	atdr_bitmap->Mdl = IoAllocateMdl(atdr_bitmap->bitmap_area_start, 2 * atdr_bitmap->bitmap_size, FALSE, FALSE, OPTIONAL NULL);
	if (atdr_bitmap->Mdl == NULL)
	{
		DbgPrint("MapBufferForUserModeUse: Failed to Allocate mdl");
		ExFreePoolWithTag(atdr_bitmap->bitmap_area_start, 'mtiB');
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	atdr_bitmap->bitmap_area = atdr_bitmap->bitmap_area_start;
	atdr_bitmap->bmap_user = KERNEL;
	atdr_bitmap->bitmap_area_backup = atdr_bitmap->bitmap_area + atdr_bitmap->bitmap_size;
	
	//atdr_bitmap->Mdl = MmAllocatePagesForMdl(lowAddress, highAddress, lowAddress, totalBytes);
	__try	
	{
		MmProbeAndLockPages(atdr_bitmap->Mdl, KernelMode, IoWriteAccess);
		*MappedUserAddress = MmMapLockedPagesSpecifyCache(atdr_bitmap->Mdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		DbgPrint("Exception occured while mapping pages to user mode\n");
		IoFreeMdl(atdr_bitmap->Mdl);
		ExFreePoolWithTag(atdr_bitmap->bitmap_area_start, 'mtiB');
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	return status;
}


void atdrFltUnload(
	IN PDRIVER_OBJECT DriverObject
	)
{
	PDEVICE_OBJECT  fdo = NULL;
	
	PAGED_CODE();

	UNREFERENCED_PARAMETER(DriverObject);
	fdo = DriverObject->DeviceObject->AttachedDevice;
	while (fdo)
	{
		PDEVICE_OBJECT  next_device = NULL;
		PDEVICE_EXTENSION  pdx = NULL;
		pdx = (PDEVICE_EXTENSION)fdo->DeviceExtension;
		PVOL_INFO pVolInfo = pdx->VolInfo;
		PATDR_BITMAP atdr_bitmap = pVolInfo->atdr_bitmap;


		if (pVolInfo)
		{
			IoFreeMdl(atdr_bitmap->Mdl);
			ExFreePoolWithTag(atdr_bitmap->bitmap_area_start, 'mtiB');
		}
		//Move to the next DeviceObject in the List of drivers
		if (DriverObject && DriverObject->DeviceObject)
		{
			if (DriverObject->DeviceObject == ControlObj)
			{
				IoDeleteDevice(ControlObj);
			}
			else {
				next_device = DriverObject->DeviceObject->NextDevice;
				if (next_device)
				{
					fdo = next_device->AttachedDevice;
				}
			}
		}
		
	}
	return;
}

