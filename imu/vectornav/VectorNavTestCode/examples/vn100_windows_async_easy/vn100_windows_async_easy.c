#include <stdio.h>
#include <Windows.h>
#include <tchar.h>
#include "vectornav.h"

/* Change the connection settings to your configuration. */
const char* const COM_PORT = "COM1";
const int BAUD_RATE = 115200;

int _tmain(int argc, _TCHAR* argv[])
{
	VN_ERROR_CODE errorCode;
	Vn100 vn100;
	int i;

	errorCode = vn100_connect(
        &vn100,
        COM_PORT,
        BAUD_RATE);
	
	if (errorCode != VNERR_NO_ERROR)
	{
		printf("Error encountered when trying to connect to the sensor.\n");

		return 0;
	}
	
	/* Configure the VN-100 to output asynchronous data. */
	errorCode = vn100_setAsynchronousDataOutputType(
        &vn100,
        VNASYNC_VNYPR,
        true);
	
	printf("Yaw, Pitch, Roll\n");

	/* Pause to ensure we have received the first asynchronous data record
	   from the sensor. */
	Sleep(1000);

	for (i = 0; i < 10; i++) {

		VnDeviceCompositeData data;

		/* The library is handling and storing asynchronous data by itself.
		   Calling this function retrieves the most recently processed
		   asynchronous data packet. */
		vn100_getCurrentAsyncData(
			&vn100,
			&data);

		printf("  %+#7.2f %+#7.2f %+#7.2f\n",
			data.ypr.yaw,
			data.ypr.pitch,
			data.ypr.roll);

		Sleep(1000);
	}
	
	errorCode = vn100_disconnect(&vn100);
	
	if (errorCode != VNERR_NO_ERROR)
	{
		printf("Error encountered when trying to disconnect from the sensor.\n");
		
		return 0;
	}

	return 0;
}
