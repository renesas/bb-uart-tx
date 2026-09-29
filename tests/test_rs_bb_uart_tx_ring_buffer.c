/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_bb_uart_tx_ring_buffer.c
 * @brief Unit tests for the Bit-Banged UART TX ring buffer manager.
 *
 * Tests rs_bb_uart_tx_ring_buffer_{reset,enqueue,dequeue,count_get,free_get}()
 * (declared in rs_bb_uart_tx_ring_buffer.h).  The ring buffer is pure logic —
 * no I/O, no FSP HAL, no timer — so this file needs no mocks and runs
 * identically on the host and on target.
 *
 * Design facts under test (see rs_bb_uart_tx_ring_buffer.h):
 *   - single-producer / single-consumer, lock-free, head/tail only;
 *   - usable capacity is (BUFFER_SIZE - 1): one slot is reserved so that
 *     head == tail is an unambiguous "empty";
 *   - enqueue is all-or-nothing;
 *   - indices wrap with a bitmask, so a write can straddle the end of storage.
 *
 * Test groups:
 *   BbUartRingBuffer — reset, capacity, FIFO order, all-or-nothing rejection,
 *                      wrap-around, occupancy accounting.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity_fixture.h"

/* RS_BB_UART_TX_TEST_HOST_BUILD is defined on the compiler command line
 * (tests/meson.build host_test_cargs).  Do NOT redefine it here. */

#include "rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h"
#include "rs_bb_uart_tx_test_helpers.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

#define SIZE   ((uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES)

static rs_bb_uart_tx_ring_buffer_t g_rb;

/** Enqueue @p n bytes of a deterministic ramp pattern starting at @p seed.
 *  The scratch buffer is static (not stack): the on-target image runs these
 *  tests on the BSP main stack, which RASC sizes at ~1 KB by default. */
static bool enqueue_ramp(uint32_t n, uint8_t seed)
{
    static uint8_t tmp[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];
    for (uint32_t i = 0U; i < n; i++)
    {
        tmp[i] = (uint8_t) (seed + i);
    }
    return rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, tmp, n);
}

/** Dequeue @p n bytes and check they follow the ramp starting at @p seed. */
static void expect_ramp(uint32_t n, uint8_t seed)
{
    for (uint32_t i = 0U; i < n; i++)
    {
        uint8_t b = 0U;
        TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b));
        TEST_ASSERT_EQUAL_HEX8((uint8_t) (seed + i), b);
    }
}

/* ── Test group: BbUartRingBuffer ─────────────────────────────────────────── */

TEST_GROUP(BbUartRingBuffer);

TEST_SETUP(BbUartRingBuffer)
{
    memset(&g_rb, 0xEE, sizeof(g_rb));   /* dirty on purpose: reset must not rely on zeroed RAM */
    rs_bb_uart_tx_ring_buffer_reset(&g_rb);
}

TEST_TEAR_DOWN(BbUartRingBuffer)
{
}

/* Reset leaves the buffer empty with head == tail == 0 */
TEST(BbUartRingBuffer, ResetLeavesBufferEmpty)
{
    TEST_ASSERT_EQUAL_UINT32(0U, g_rb.head);
    TEST_ASSERT_EQUAL_UINT32(0U, g_rb.tail);
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
}

/* Usable capacity is SIZE - 1 (one slot reserved) */
TEST(BbUartRingBuffer, UsableCapacityIsSizeMinusOne)
{
    TEST_ASSERT_EQUAL_UINT32(SIZE - 1U, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
}

/* Zero-length enqueue is a trivial success even with a NULL source pointer */
TEST(BbUartRingBuffer, ZeroLengthEnqueueSucceedsWithNullData)
{
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, NULL, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
    TEST_ASSERT_EQUAL_UINT32(0U, g_rb.head);
}

/* One byte in, same byte out */
TEST(BbUartRingBuffer, SingleByteRoundTrip)
{
    uint8_t in  = 0x5AU;
    uint8_t out = 0U;

    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, &in, 1U));
    TEST_ASSERT_EQUAL_UINT32(1U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &out));
    TEST_ASSERT_EQUAL_HEX8(0x5AU, out);
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
}

/* Bytes come out in the order they went in */
TEST(BbUartRingBuffer, FifoOrderPreserved)
{
    TEST_ASSERT_TRUE(enqueue_ramp(37U, 0x10U));
    expect_ramp(37U, 0x10U);
}

/* Separate enqueue calls concatenate in call order */
TEST(BbUartRingBuffer, MultipleEnqueuesConcatenateInOrder)
{
    uint8_t a[3] = { 1U, 2U, 3U };
    uint8_t b[2] = { 4U, 5U };

    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, a, 3U));
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, b, 2U));
    TEST_ASSERT_EQUAL_UINT32(5U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
    expect_ramp(5U, 1U);
}

/* Dequeue on an empty buffer returns false and does not modify the output */
TEST(BbUartRingBuffer, DequeueEmptyReturnsFalseAndLeavesOutputUntouched)
{
    uint8_t out = 0xA5U;

    TEST_ASSERT_FALSE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &out));
    TEST_ASSERT_EQUAL_HEX8(0xA5U, out);
    TEST_ASSERT_EQUAL_UINT32(0U, g_rb.tail);
}

/* Exactly SIZE-1 bytes fit; the buffer is then full */
TEST(BbUartRingBuffer, EnqueueFillsToCapacity)
{
    TEST_ASSERT_TRUE(enqueue_ramp(HELPER_RB_CAPACITY, 0U));
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
}

/* Requesting the full raw storage size (SIZE bytes) is rejected: one slot is reserved */
TEST(BbUartRingBuffer, EnqueueOfFullStorageSizeRejected)
{
    TEST_ASSERT_FALSE(enqueue_ramp(SIZE, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
}

/* When full, even a single extra byte is rejected */
TEST(BbUartRingBuffer, EnqueueWhenFullRejected)
{
    uint8_t extra = 0x99U;

    TEST_ASSERT_TRUE(enqueue_ramp(HELPER_RB_CAPACITY, 0U));
    TEST_ASSERT_FALSE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, &extra, 1U));
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
}

/* All-or-nothing: an oversized request leaves indices and storage untouched */
TEST(BbUartRingBuffer, AllOrNothingRejectionLeavesBufferUnchanged)
{
    rs_bb_uart_tx_ring_buffer_t before;
    uint8_t                     big[16];

    memset(big, 0x77, sizeof(big));
    TEST_ASSERT_TRUE(enqueue_ramp(HELPER_RB_CAPACITY - 10U, 0U));
    memcpy(&before, &g_rb, sizeof(before));

    TEST_ASSERT_FALSE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, big, 11U));   /* one byte too many */
    TEST_ASSERT_EQUAL_MEMORY(&before, &g_rb, sizeof(before));                /* head, tail AND data */

    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, big, 10U));    /* exact fit still works */
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
}

/* Draining a full buffer by one byte frees exactly one slot */
TEST(BbUartRingBuffer, FullThenDequeueOneFreesExactlyOneSlot)
{
    uint8_t b = 0U;
    uint8_t extra = 0xC3U;

    TEST_ASSERT_TRUE(enqueue_ramp(HELPER_RB_CAPACITY, 0U));
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b));
    TEST_ASSERT_EQUAL_UINT32(1U, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, &extra, 1U));
    TEST_ASSERT_FALSE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, &extra, 1U));
}

/* A write that straddles the end of storage is split correctly and reads back intact */
TEST(BbUartRingBuffer, WrapAroundSplitCopyPreservesData)
{
    uint8_t b = 0U;

    /* Advance head/tail to 5 bytes before the end of storage. */
    TEST_ASSERT_TRUE(enqueue_ramp(SIZE - 5U, 0U));
    for (uint32_t i = 0U; i < (SIZE - 5U); i++)
    {
        TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b));
    }
    TEST_ASSERT_EQUAL_UINT32(SIZE - 5U, g_rb.head);

    /* 12 bytes: 5 land at the tail end of storage, 7 wrap to the start. */
    TEST_ASSERT_TRUE(enqueue_ramp(12U, 0xA0U));
    TEST_ASSERT_EQUAL_UINT32(7U, g_rb.head);
    expect_ramp(12U, 0xA0U);
}

/* Write that ends exactly on the last storage slot does not wrap head incorrectly */
TEST(BbUartRingBuffer, WriteEndingExactlyAtStorageEndWrapsHeadToZero)
{
    uint8_t b = 0U;

    TEST_ASSERT_TRUE(enqueue_ramp(SIZE - 8U, 0U));
    for (uint32_t i = 0U; i < (SIZE - 8U); i++)
    {
        TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b));
    }
    TEST_ASSERT_TRUE(enqueue_ramp(8U, 0x40U));       /* fills slots [SIZE-8, SIZE-1] */
    TEST_ASSERT_EQUAL_UINT32(0U, g_rb.head);         /* head wrapped to 0 */
    TEST_ASSERT_EQUAL_UINT32(8U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
    expect_ramp(8U, 0x40U);
}

/* count + free always equals usable capacity, across many mixed operations */
TEST(BbUartRingBuffer, CountAndFreeAlwaysSumToCapacity)
{
    uint8_t b = 0U;

    for (uint32_t round = 0U; round < 200U; round++)
    {
        (void) enqueue_ramp((round % 23U) + 1U, (uint8_t) round);
        for (uint32_t k = 0U; k < (round % 11U); k++)
        {
            (void) rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b);
        }
        TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY,
                                 rs_bb_uart_tx_ring_buffer_count_get(&g_rb) +
                                 rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
    }
}

/* Indices never leave [0, SIZE) even after many wrap cycles */
TEST(BbUartRingBuffer, IndicesStayWithinStorageAfterManyWraps)
{
    uint8_t b = 0U;

    for (uint32_t round = 0U; round < 3000U; round++)
    {
        TEST_ASSERT_TRUE(enqueue_ramp(7U, (uint8_t) round));
        expect_ramp(7U, (uint8_t) round);
        TEST_ASSERT_TRUE(g_rb.head < SIZE);
        TEST_ASSERT_TRUE(g_rb.tail < SIZE);
    }
    TEST_ASSERT_FALSE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b));
}

/* Repeated fill/drain cycles with different patterns never corrupt data */
TEST(BbUartRingBuffer, RepeatedFullFillAndDrainCycles)
{
    for (uint32_t cycle = 0U; cycle < 5U; cycle++)
    {
        TEST_ASSERT_TRUE(enqueue_ramp(HELPER_RB_CAPACITY, (uint8_t) (cycle * 37U)));
        TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
        expect_ramp(HELPER_RB_CAPACITY, (uint8_t) (cycle * 37U));
        TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, rs_bb_uart_tx_ring_buffer_free_get(&g_rb));
    }
}

/* Reset after use returns to the empty state (stale data is unreachable) */
TEST(BbUartRingBuffer, ResetAfterUseReturnsToEmpty)
{
    uint8_t b = 0xEEU;

    TEST_ASSERT_TRUE(enqueue_ramp(50U, 1U));
    rs_bb_uart_tx_ring_buffer_reset(&g_rb);
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
    TEST_ASSERT_FALSE(rs_bb_uart_tx_ring_buffer_dequeue(&g_rb, &b));
    TEST_ASSERT_EQUAL_HEX8(0xEEU, b);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartRingBuffer)
{
    RUN_TEST_CASE(BbUartRingBuffer, ResetLeavesBufferEmpty);
    RUN_TEST_CASE(BbUartRingBuffer, UsableCapacityIsSizeMinusOne);
    RUN_TEST_CASE(BbUartRingBuffer, ZeroLengthEnqueueSucceedsWithNullData);
    RUN_TEST_CASE(BbUartRingBuffer, SingleByteRoundTrip);
    RUN_TEST_CASE(BbUartRingBuffer, FifoOrderPreserved);
    RUN_TEST_CASE(BbUartRingBuffer, MultipleEnqueuesConcatenateInOrder);
    RUN_TEST_CASE(BbUartRingBuffer, DequeueEmptyReturnsFalseAndLeavesOutputUntouched);
    RUN_TEST_CASE(BbUartRingBuffer, EnqueueFillsToCapacity);
    RUN_TEST_CASE(BbUartRingBuffer, EnqueueOfFullStorageSizeRejected);
    RUN_TEST_CASE(BbUartRingBuffer, EnqueueWhenFullRejected);
    RUN_TEST_CASE(BbUartRingBuffer, AllOrNothingRejectionLeavesBufferUnchanged);
    RUN_TEST_CASE(BbUartRingBuffer, FullThenDequeueOneFreesExactlyOneSlot);
    RUN_TEST_CASE(BbUartRingBuffer, WrapAroundSplitCopyPreservesData);
    RUN_TEST_CASE(BbUartRingBuffer, WriteEndingExactlyAtStorageEndWrapsHeadToZero);
    RUN_TEST_CASE(BbUartRingBuffer, CountAndFreeAlwaysSumToCapacity);
    RUN_TEST_CASE(BbUartRingBuffer, IndicesStayWithinStorageAfterManyWraps);
    RUN_TEST_CASE(BbUartRingBuffer, RepeatedFullFillAndDrainCycles);
    RUN_TEST_CASE(BbUartRingBuffer, ResetAfterUseReturnsToEmpty);
}
