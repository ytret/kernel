#pragma once

#include <stdbool.h>

void pic_init(void);

void pic_mask_all(void);
void pic_set_mask(int irq, bool b_mask);

void pic_send_eoi(int irq);
void pic_spurious_irq_handler(int irq);
