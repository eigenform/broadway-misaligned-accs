
.global test_minimal
test_minimal:
	mflr	r0

	li		r3, 0

	# Value used for the misaligned store
	lis		r5, 0xffff
	ori		r5, r5, 0xffff

	# Pointer to some memory via the cache-inhibited mapping (0xc170_0000)
	li		r4, 0
	lis		r4, 0xc170

	# Random unrelated memory via the cacheable mapping (0x8000_4000)
	li		r6, 0
	lis		r6, 0x8000
	ori		r6, r6, 0x4000

	# Four aligned 32-bit stores starting at 0xc170_0000
	stw		r3, 0x0(r4)
	stw		r3, 0x4(r4)
	stw		r3, 0x8(r4)
	stw		r3, 0xc(r4)

	# Misaligned 32-bit store to 0xc170_0005. This should crash/hang.
	# How do we get this to succeed with the weird 64-bit/128-bit store?
	#stw		r5, 0x5(r4)
	
	# Return something just in case
	li		r3, 42
	mtlr	r0
	blr


