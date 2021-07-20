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

static vu32* const HW_CLOCKS	= (vu32*)0xcd000190;


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

	printf("HW_CLOCKS=%08x\n", *HW_CLOCKS);

	printf("HID0=%08x HID1=%08x HID2=%08x HID4=%08x\n", 
		mfspr(HID0), mfspr(HID1), mfspr(HID2), mfspr(HID4));

	// The state of HID{0,1,2} SPRs differs from the WiiVC test case.
	// Booting into this application from The Homebrew Channel:
	//
	//		HID0 = 0x0011_c264
	//		HID1 = 0x3000_0000
	//		HID2 = 0xa000_0000
	//
	// And for the state in Dolphin with WiiVC 
	//
	//		HID0 = 0x0011_c664 (DCFI (bit 21) is set)
	//		HID1 = 0x8000_0000 (has only PC0 (bit 0) set)
	//		HID2 = 0xe000_0000 (has WPE (bit 1) set)
	//
	//	Setting 0x0000_0400 in HID0 causes my Wii to crash spectacularly here
	//	(I guess because it invalidates the entire dcache?). Questions/notes:
	//
	//		- DCFI being set is probably just an artifact of Dolphin not 
	//		  handling it (which is probably fine anyway)
	//		- Why is the HID1 configuration different (clocking related?)
	//		- WPE bit probably irrelevant here
	//
	// mtspr(HID0, 0x0011c664); // Setting 0x0000_0400 crashes us hard
	// mtspr(HID1, 0x80000000);	// Uhhh?
	// mtspr(HID2, 0xe0000000); // The write-gather pipe shouldn't matter here
	// mtspr(HID4, 0x83900000); // HID4 bits should be identical

	u32 addr		= (u32)&test_buffer[0];
	u32 addr_phys	= addr & 0x017fffff;
	u32 addr_uc		= addr_phys | 0xc0000000;

	u32 addr_mis	= addr + 0x00000007;
	u32 addr_uc_mis	= addr_uc + 0x00000007;

	// Crash here!
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


