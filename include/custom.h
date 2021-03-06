#ifndef _CUSTOM_H_
#define _CUSTOM_H_

#include <custom_regdef.h>

typedef volatile struct Custom *const CustomPtrT;

extern struct Custom volatile _custom;

#define custom (&_custom)

/* Macros below take or'ed DMAF_* flags. */
static inline void EnableDMA(uint16_t x) { custom->dmacon = DMAF_SETCLR | x; }
static inline void DisableDMA(uint16_t x) { custom->dmacon = x; }

/* All macros below take or'ed INTF_* flags. */
static inline void EnableINT(u_short x) { custom->intena_ = INTF_SETCLR | x; }
static inline void DisableINT(u_short x) { custom->intena_ = x; }
static inline void CauseIRQ(u_short x) { custom->intreq_ = INTF_SETCLR | x; }
static inline void ClearIRQ(u_short x) { custom->intreq_ = x; }

static inline bool RightMouseButton(void) {
  return !(custom->potinp & DATLY);
}

static inline void WaitLine(uint32_t line) {
  while ((custom->vposr_ & 0x1ff00) != ((line << 8) & 0x1ff00));
}

static inline void WaitVBlank(void) { WaitLine(303); }

#endif /* !_CUSTOM_H_ */
