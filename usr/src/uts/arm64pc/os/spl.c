/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/machlock.h>

static uint8_t
arm64_pil_to_pmr(int pil)
{
	if (pil <= 0)
		return (0xf0);
	if (pil >= PIL_MAX)
		return (0x00);
	return ((uint8_t)((PIL_MAX - pil) << 4));
}

int
getpil(void)
{
	uint64_t pmr;

	__asm__ __volatile__("mrs %0, S3_0_C4_C6_0" : "=r" (pmr));
	pmr &= 0xff;
	if (pmr >= 0xf0)
		return (0);
	return (PIL_MAX - (int)(pmr >> 4));
}

void
setpil(uint_t pil)
{
	uint64_t pmr = arm64_pil_to_pmr((int)pil);

	__asm__ __volatile__("msr S3_0_C4_C6_0, %0" :: "r" (pmr)
	    : "memory");
	isb();
}

int
splr(int pil)
{
	int old = getpil();

	if (pil > old)
		setpil((uint_t)pil);
	return (old);
}

void splx(int pil) { setpil((uint_t)pil); }
int splhigh(void) { return (splr(PIL_MAX)); }
int splhi(void) { return (splhigh()); }
int splzs(void) { return (splr(12)); }

int
spl0(void)
{
	int old = getpil();
	splx(0);
	return (old);
}

int spl6(void) { return (splr(12)); }
int spl7(void) { return (splr(13)); }
int spl8(void) { return (splr(15)); }
void set_base_spl(void) { splx(0); }
int __ipltospl(int pil) { return (pil); }

void
lock_set_spl(lock_t *lock, int pil, ushort_t *old)
{
	*old = (ushort_t)splr(pil);
	lock_set(lock);
}

void
lock_clear_splx(lock_t *lock, int pil)
{
	lock_clear(lock);
	splx(pil);
}
