/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * mq.h - the MQ arithmetic decoder of JBIG2 (ITU-T T.88, annex E) and
 * JPEG 2000 (ITU-T T.800, annex C): the same coder in both. A context is
 * one byte: its state's index (0-46) times 2, plus its more probable
 * symbol; a zeroed array is every context's start.
 *
 * mq_decode is the call; MQ_DECODE the same inline on the caller's own
 * copies of the registers (a 68030's inner loop: MQ_LOAD, then
 * MQ_DECODE a pixel at a time, then MQ_SAVE), the decision's table one
 * long a state (mq_tab: Qe, the next state after an MPS and after an LPS,
 * the LPS's switch).
 */
#ifndef MQ_H
#define MQ_H

#include <stdint.h>

typedef struct {
	const uint8_t *p, *end;		/* the next byte; past the data, 0xFF is read */
	uint32_t c;
	uint32_t a;
	int ct;
} MQ;

/* Qe << 16 | (next after an MPS) << 9 | (next after an LPS) << 1 | switch,
   the next states as context bytes (index * 2) */
extern const uint32_t mq_tab[47];

void mq_init(MQ *m, const uint8_t *data, long n);
int mq_decode(MQ *m, uint8_t *cx);
void mq_byte_in(MQ *m);

#define MQ_LOAD(m, c_, a_, ct_) do { (c_) = (m)->c; (a_) = (m)->a; (ct_) = (m)->ct; } while (0)
#define MQ_SAVE(m, c_, a_, ct_) do { (m)->c = (c_); (m)->a = (a_); (m)->ct = (ct_); } while (0)

/* d = the decision of context byte *cxp; c_, a_, ct_ the registers (locals),
   m the MQ for its bytes */
#define MQ_DECODE(m, c_, a_, ct_, cxp, d)                                                      \
	do {                                                                                   \
		uint8_t *cx_ = (cxp);                                                          \
		unsigned st_ = *cx_, mps_ = st_ & 1;                                           \
		uint32_t e_ = mq_tab[st_ >> 1], q_ = e_ >> 16;                                 \
		(a_) -= q_;                                                                    \
		if (((c_) >> 16) >= q_) {                                                      \
			(c_) -= q_ << 16;                                                      \
			if ((a_) & 0x8000) {                                                   \
				(d) = (int)mps_;                                               \
				break;                                                         \
			}                                                                      \
			if ((a_) < q_) {                                                       \
				(d) = (int)(mps_ ^ 1);                                         \
				*cx_ = (uint8_t)((e_ & 0xFE) | (mps_ ^ (e_ & 1)));             \
			} else {                                                               \
				(d) = (int)mps_;                                               \
				*cx_ = (uint8_t)(((e_ >> 8) & 0xFE) | mps_);                   \
			}                                                                      \
		} else {                                                                       \
			if ((a_) < q_) {                                                       \
				(d) = (int)mps_;                                               \
				*cx_ = (uint8_t)(((e_ >> 8) & 0xFE) | mps_);                   \
			} else {                                                               \
				(d) = (int)(mps_ ^ 1);                                         \
				*cx_ = (uint8_t)((e_ & 0xFE) | (mps_ ^ (e_ & 1)));             \
			}                                                                      \
			(a_) = q_;                                                             \
		}                                                                              \
		do {                                                                           \
			if ((ct_) == 0) {                                                      \
				MQ_SAVE(m, c_, a_, ct_);                                       \
				mq_byte_in(m);                                                 \
				MQ_LOAD(m, c_, a_, ct_);                                       \
			}                                                                      \
			(a_) <<= 1;                                                            \
			(c_) <<= 1;                                                            \
			(ct_)--;                                                               \
		} while (!((a_) & 0x8000));                                                    \
	} while (0)

#endif
