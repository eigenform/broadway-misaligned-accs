#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <ogc/machine/processor.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static u32 test_buffer[0x100] __attribute__((aligned(1024)));

static vu32* const HW_AHBPROT	= (vu32*)0xcd800064;
static vu32* const HW_CLOCKS	= (vu32*)0xcd800190;
static vu32* const HW_PLLSYS	= (vu32*)0xcd8001b0;
static vu32* const HW_PLLSYSEXT	= (vu32*)0xcd8001b4;
static vu32* const HW_VERSION	= (vu32*)0xcd800214;


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
	CON_EnableGecko(1, true);
	printf("\n");
}

void __attribute__((noinline)) do_misaligned_store32(u32 addr, u32 val)
{ 
	__asm__ __volatile__ (
		"nop\n"
		"nop\n"
		"nop\n"
		"nop\n"
		"stw  %1, 0x0(%0)\n"
		"nop\n"
		"nop\n"
		"nop\n"
		"nop\n"
		::"r"(addr), "r" (val):
	);
}

void __attribute__((noinline)) do_misaligned_store16(u32 addr, u16 val)
{ 
	__asm__ __volatile__ (
		"nop\n"
		"nop\n"
		"nop\n"
		"nop\n"
		"sth  %1, 0x0(%0)\n"
		"nop\n"
		"nop\n"
		"nop\n"
		"nop\n"
		::"r"(addr), "r" (val):
	);
}

// Try setting the decrementer and expecting an exception while we're hanging
// after performing some misaligned 32-bit store.
void test_dec_exception_store32(u32 addr, u32 value)
{
	if ((mfmsr() & 0x00008000) == 0) {
		mtmsr(mfmsr() | 0x8000);
		printf("Enabled external interrupts\n");
	}
	__asm__ __volatile__(
		"mtdec %0\n" 
		: : "r" (0x00000010)
	);
	do_misaligned_store32(addr, value);
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
		return 1;
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

// The current IOS might affect the behavior in question (i.e. by setting
// up hardware in some different way than IOSv58.6176 does). When booting 
// into The Homebrew Channel, all of the HW_AHBPROT bits should be set for 
// us (so we can read/write Hollywood registers). 
//
// The title metadata for NTSC-J WiiVC Ocarina of Time indicates that the
// supported IOS version is IOSv9. FYI: reloading directly into IOSv9 here 
// will cause HW_AHBPROT bits to be cleared. You'd need ARM code execution
// in IOSv9 in order to restore your original permissions.

int check_ios(void)
{

	printf("Running with IOSv%d.%d\n", IOS_GetVersion(), IOS_GetRevision());
	printf("HW_AHBPROT    =%08x\n", *HW_AHBPROT);
	printf("HW_VERSION    =%08x\n", *HW_VERSION);
	printf("HW_CLOCKS     =%08x\n", *HW_CLOCKS);
	printf("HW_PLLSYS     =%08x\n", *HW_PLLSYS);
	printf("HW_PLLSYSEXT  =%08x\n", *HW_PLLSYSEXT);

	//int res = IOS_ReloadIOS(9);
	//if (res) {
	//	printf("Couldn't load IOSv9, returned %d\n", res);
	//	goto test_done;
	//}
	//printf("Reloaded into IOSv%d.%d\n", IOS_GetVersion(), IOS_GetRevision());
	return 0;
}

int main(int argc, char **argv) {
	init();

	if (!check_ios()) goto test_done;
	if (!check_sprs()) goto test_done;

	u32 addr		= (u32)&test_buffer[0];
	u32 addr_phys	= addr & 0x017fffff;
	u32 addr_uc		= addr_phys | 0xc0000000;

	// Notes:
	//
	// Broadway always seems to hang here, regardless of how the misaligned
	// store is split on 32-bit boundaries (and the behavior is the same for
	// all flavors of misaligned 16-bit stores too):
	//
	//		A[29-31] == 0b000 - OK (this is an aligned access)
	// 		A[29-31] == 0b001 - NG (broadway hangs)
	// 		A[29-31] == 0b010 - NG (broadway hangs)
	// 		A[29-31] == 0b011 - NG (broadway hangs)
	// 		A[29-31] == 0b100 - OK (this is an aligned access)
	// 		A[29-31] == 0b101 - NG (broadway hangs)
	// 		A[29-31] == 0b110 - NG (broadway hangs)
	// 		A[29-31] == 0b111 - NG (broadway hangs)
	//
	// Serializing instructions before this store don't seem to affect the 
	// outcome (we still hang), so I guess that totally rules out reordering.
	//
	// Why do I not get an exception and stack trace from libogc here?

	// WARNING: This will cause Broadway to hang indefinitely.
	do_misaligned_store32(addr_uc+1, 0xdeadbeef);
	//do_misaligned_store16(addr_uc, 0xdead);

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


