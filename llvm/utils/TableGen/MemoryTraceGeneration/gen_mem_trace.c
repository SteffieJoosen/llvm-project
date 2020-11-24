#include <sancus_support/sm_io.h>
#include <sancus_support/sancus_step.h>

DECLARE_SM(foo, 0x1234);

void SM_ENTRY(foo) test(char key) {}

void irqHandler(void) {}

int main()
{
    msp430_io_init();
sancus_enable(&foo);
__asm__ __volatile__(
    "add r5, r6"
);

}

/* ======== TIMER A ISR ======== */
SANCUS_STEP_ISR_ENTRY2(irqHandler, __ss_end)
