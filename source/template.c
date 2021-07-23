#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <ogc/machine/processor.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static u32 *fixed_buffer = (u32*)0x81700000;
static u32 *fixed_buffer_uc = (u32*)0xc1700000;

extern int test_minimal(void);

void init_video(void)
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
	CON_EnableGecko(1, true);
	printf("\n");
}

// The state of HID{0,1,2} SPRs differs from the WiiVC test case running
// on Dolphin, but they might not matter on the surface of things. Booting 
// into this application from The Homebrew Channel:
//
//		HID0 = 0x0011_c264
//		HID1 = 0x3000_0000
//		HID2 = 0xa000_0000
//		MSR  = 0x0000_b032
//
// And for the state in Dolphin while doing the misaligned store:
//
//		HID0 = 0x0011_c664 (DCFI (bit 21) is set)
//		HID1 = 0x8000_0000 (has only PC0 (bit 0) set)
//		HID2 = 0xe000_0000 (has WPE (bit 1) set)
//		MSR  = 0x0000_a032 (ME (bit 19) is clear)
//
//	Setting 0x0000_0400 in HID0 causes my Wii to crash spectacularly here
//	(I guess because it invalidates the entire dcache?). Questions/notes:
//
//		- DCFI being set is probably just an artifact of Dolphin not 
//		  handling it (which is probably fine anyway)
//		- Why is the HID1 configuration different (clocking related?);
//		  this probably reflects some bits in a Hollywood register
//		- WPE bit probably irrelevant here

int check_sprs(void)
{
	// In the WiiVC test case, the DBAT responsible for the uncached MEM1 
	// region is marked as cache-inhibited and guarded.

	u32 dbat1_u = mfspr(DBAT1U);
	u32 dbat1_l = mfspr(DBAT1L);
	if ((dbat1_u != 0xc0001fff) || (dbat1_l != 0x0000002a)) {
		printf("DBAT1 isn't correct for this test?\n");
		printf("DBAT1 %08x%08x\n", dbat1_u, dbat1_l);
		return -1;
	}

	printf("HID0=%08x HID1=%08x HID2=%08x HID4=%08x\n", 
		mfspr(HID0), mfspr(HID1), mfspr(HID2), mfspr(HID4));
	printf("MSR=%08x\n", mfmsr());

	// mtspr(HID0, 0x0011c664); // Setting 0x0000_0400 crashes us hard
	// mtspr(HID1, 0x80000000);	// Uhhh aren't these supposed to be read-only?
	// mtspr(HID2, 0xe0000000); // The write-gather pipe shouldn't matter here
	// mtspr(HID4, 0x83900000); // HID4 bits should be identical

	return 0;
}

void print_buffer(void) 
{
	printf("%08x: %08x %08x %08x %08x\n", (u32)fixed_buffer_uc, 
			fixed_buffer_uc[0], fixed_buffer_uc[1], 
			fixed_buffer_uc[2], fixed_buffer_uc[3]);
}

int main(int argc, char **argv) {
	init_video();

	if (check_sprs() != 0) 
		goto test_done;

	// See source/test.S 
	test_minimal();

	// Print the resulting buffer
	print_buffer();

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


