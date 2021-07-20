#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <ogc/machine/processor.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void init(void)
{
	VIDEO_Init();
	PAD_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 20, 20, rmode->fbWidth,rmode->xfbHeight, 
			rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) 
		VIDEO_WaitVSync();
	printf("\n");
}

// Target for loads and stores
static u32 test_buffer[0x100] __attribute__((aligned(1024)));

int main(int argc, char **argv) {
	init();

	// In the WiiVC test case, the DBAT responsible for the uncached MEM1 
	// region is marked as cache-inhibited and guarded.

	u32 dbat1_u = mfspr(DBAT1U);
	u32 dbat1_l = mfspr(DBAT1L);
	if ((dbat1_u != 0xc0001fff) || (dbat1_l != 0x0000002a)) {
		printf("DBAT1 isn't correct for this test?\n");
		printf("DBAT1 %08x%08x\n", dbat1_u, dbat1_l);
		goto test_done;
	}

	// The state of HID SPRs differs from the WiiVC test case, in which:
	//		HID0 has DCFI (bit 21) set
	//		HID1 has only PC0 (bit 0) set
	//		HID2 has WPE (bit 1) set

	u32 hid0 = mfspr(HID0);
	u32 hid1 = mfspr(HID1);
	u32 hid2 = mfspr(HID2);
	u32 hid4 = mfspr(HID4);

	if ((hid0 != 0x0011c664) || (hid1 != 0x80000000) || 
		(hid2 != 0xe0000000) || (hid4 != 0x83900000)) {
		printf("HIDn register[s] aren't correct for this test\n");
		printf("HID0=%08x HID1=%08x HID2=%08x HID4=%08x\n", 
				hid0, hid1, hid2, hid4);
		goto test_done;
	}

	u32 addr		= (u32)&test_buffer[0];
	u32 addr_phys	= addr & 0x017fffff;
	u32 addr_uc		= addr_phys | 0xc0000000;

	u32 addr_mis	= addr + 0x00000007;
	u32 addr_uc_mis	= addr_uc + 0x00000006;

	*(u32*)addr_uc_mis = 0xdeadbeef;
	printf("Wrote 0xdeadbeef to address %08x\n", addr_mis);

	printf("%08x: %08x\n", (u32)(addr_uc+0x00), *(u32*)(addr_uc+0x00));
	printf("%08x: %08x\n", (u32)(addr_uc+0x04), *(u32*)(addr_uc+0x04));
	printf("%08x: %08x\n", (u32)(addr_uc+0x08), *(u32*)(addr_uc+0x08));
	printf("%08x: %08x\n", (u32)(addr_uc+0x0c), *(u32*)(addr_uc+0x0c));
	printf("%08x: %08x\n", (u32)(addr_uc+0x10), *(u32*)(addr_uc+0x10));



test_done:
	printf("\n");
	printf("Test completed, press START to exit ...\n");
	while(1) {
		VIDEO_WaitVSync();
		PAD_ScanPads();
		int buttonsDown = PAD_ButtonsDown(0);
		if (buttonsDown & PAD_BUTTON_START) {
			exit(0);
		}
	}
	return 0;
}


